"use strict";

// ---------------------------------------------------------------------------
// Config. bmcBase = "" means "same origin" (dashboard served from the BMC, or
// reached via an SSH tunnel that maps localhost -> BMC:443). Override at runtime
// with the BMC field in the header (persisted to storage).
// ---------------------------------------------------------------------------
const CONFIG = {
  bmcBase: localStorage.getItem("bmcBase") ?? "",   // e.g. "https://192.168.9.42"
  pollMs:  +(localStorage.getItem("pollMs") ?? 2000),
  history: 60,                                       // points kept per sparkline
  logMax:  12,                                       // event-log rows shown
  // Known Redfish event-log collections, tried in order (hostless BMCs may not
  // expose a "system"). First one that answers wins.
  logPaths: [
    "/redfish/v1/Systems/system/LogServices/EventLog/Entries",
    "/redfish/v1/Managers/bmc/LogServices/EventLog/Entries",
    "/redfish/v1/Managers/bmc/LogServices/Journal/Entries",
  ],
  // OEM / custom endpoints for non-Redfish quantities. The dashboard probes them
  // every poll and lights the card up when they answer.
  oem: {
    heater: "/redfish/v1/Chassis/Agilex5E013B/Oem/Altera/Heater",
    accel:  "/redfish/v1/Chassis/Agilex5E013B/Oem/Altera/Accelerometer",
    fanPwm: "/redfish/v1/Chassis/Agilex5E013B/Oem/Altera/FanPwm",
  },
};

// Chart.js is vendored same-origin (chart.umd.min.js). If absent for any reason
// the dashboard still shows live values; sparklines are simply skipped.
const HAS_CHART = (typeof window.Chart !== "undefined");

// Sparkline colour palette, cycled per card (matches the multi-colour mockup).
const PALETTE = ["#36d399", "#2f93ff", "#f7b955", "#a78bfa", "#38c6e6", "#f1556c"];

// ---------------------------------------------------------------------------
// Telemetry layout. Cards are grouped into ordered, titled sections and ordered
// within each section per the "items" list below (matched by sensor Id/Name,
// normalised so "Board_Temp" / "Board Temp" / "board-temp" all match). Sensors
// not listed in any section fall into DEFAULT_SECTION (appended after the listed
// ones). DISPLAY_NAMES overrides a card's visible title without renaming the
// underlying Redfish sensor.
// ---------------------------------------------------------------------------
const SECTIONS = [
  { id: "altera", title: "Altera Sensor Board",
    items: ["Fan_Ctrl_PWM", "Fan_Ctrl", "Board_Temp",
            "Heater_Supply_Voltage", "Heater_Current", "Heater_Power", "Heater_Power_Meter_Temp",
            "Fan_Supply_Voltage", "Fan_Current", "Fan_Power", "Fan_Power_Meter_Temp",
            "Slider", "Fan_Vibration"] },
  { id: "sdmxcvr", title: "SDM / XCVR Temperature",
    items: ["SDM_Temp", "XCVR_BotLeft_Temp", "XCVR_BotRight_Temp",
            "XCVR_TopRight_Temp"] },
];
const DEFAULT_SECTION = "altera";          // uncategorized sensors land here
const DISPLAY_NAMES = {
  "fanctrl": "Fan Ctrl Tach",
  "slider": "Slider",
  "fanvibration": "Fan Vibration",
  // TSC1641 power-meter rails: show clean labels (objects are Heater_*/Fan_*).
  "heatersupplyvoltage": "Heater Supply Voltage",
  "heatercurrent": "Heater Current",
  "heaterpower": "Heater Power",
  "heaterpowermetertemp": "Heater Power Meter Temp",
  "fansupplyvoltage": "Fan Supply Voltage",
  "fancurrent": "Fan Current",
  "fanpower": "Fan Power",
  "fanpowermetertemp": "Fan Power Meter Temp",
};

const norm = (s) => String(s ?? "").toLowerCase().replace(/[^a-z0-9]/g, "");

// normalised sensor id -> { sectionId, order } lookup, built once from SECTIONS.
const SECTION_LOOKUP = new Map();
SECTIONS.forEach((sec) =>
  sec.items.forEach((nm, i) =>
    SECTION_LOOKUP.set(norm(nm), { sectionId: sec.id, order: i })));

// Resolve a sensor to its section id, in-section order, and display name.
function placement(s, fallbackName) {
  const keys = [norm(s.Id), norm(s.Name)].filter(Boolean);
  let hit = null;
  for (const k of keys) { if (SECTION_LOOKUP.has(k)) { hit = SECTION_LOOKUP.get(k); break; } }
  let disp = null;
  for (const k of keys) { if (k in DISPLAY_NAMES) { disp = DISPLAY_NAMES[k]; break; } }
  return {
    sectionId: hit ? hit.sectionId : DEFAULT_SECTION,
    order: hit ? hit.order : 500,           // listed first, then uncategorized
    name: disp || s.Name || fallbackName,
  };
}

// Create the titled section containers (idempotent) in SECTIONS order.
function ensureSections() {
  const host = $("sensorSections");
  if (!host) return;
  for (const sec of SECTIONS) {
    if (document.getElementById("section-" + sec.id)) continue;
    const wrap = document.createElement("section");
    wrap.className = "sensor-section";
    wrap.id = "section-" + sec.id;
    const title = document.createElement("div");
    title.className = "section-title";
    title.textContent = sec.title;
    const grid = document.createElement("div");
    grid.className = "grid";
    grid.id = "grid-" + sec.id;
    wrap.appendChild(title);
    wrap.appendChild(grid);
    host.appendChild(wrap);
  }
}

const $ = (id) => document.getElementById(id);
const charts = new Map();   // key -> { chart, data, color }
let chassisId = null;
let chassisModel = null;    // inventory model/part for the BMC node label (board-agnostic)
let logPath = null;         // resolved working event-log collection
let timer = null;

const setText = (id, v) => { const el = $(id); if (el) el.textContent = (v == null || v === "") ? "\u2014" : String(v); };

// --- tiny fetch helper (same-origin uses cookies; cross-origin needs CORS) ---
async function rf(path, opts = {}) {
  const url = (CONFIG.bmcBase || "") + path;
  const res = await fetch(url, { credentials: "include", ...opts });
  if (!res.ok) throw new Error(`${res.status} ${res.statusText} for ${path}`);
  return res.json();
}

// --- discover the first chassis that has a Sensors collection ---
async function discoverChassis() {
  const coll = await rf("/redfish/v1/Chassis");
  for (const m of coll.Members ?? []) {
    const id = m["@odata.id"].split("/").pop();
    try {
      const ch = await rf(m["@odata.id"]);
      if (ch.Sensors) {
        chassisId = id;
        // Board-agnostic label: prefer inventory Model, then PartNumber.
        chassisModel = ch.Model || ch.PartNumber || null;
        return id;
      }
    } catch (_) { /* keep trying */ }
  }
  chassisId = "Agilex5E013B";
  return chassisId;
}

// --- pull every sensor in one shot via $expand, fall back to enumeration ---
async function fetchSensors() {
  const base = `/redfish/v1/Chassis/${chassisId}/Sensors`;
  try {
    const j = await rf(base + "?$expand=*($levels=1)");
    if (Array.isArray(j.Members) && j.Members[0] && "Reading" in j.Members[0])
      return j.Members;
  } catch (_) { /* $expand may be unsupported; enumerate */ }
  const coll = await rf(base);
  const out = [];
  for (const m of coll.Members ?? []) {
    try { out.push(await rf(m["@odata.id"])); } catch (_) {}
  }
  return out;
}

// --- unit text per Redfish ReadingType ---
function unitFor(s) {
  const u = {
    Temperature: "\u00B0C", Rotational: "RPM", Voltage: "V",
    Current: "A", Power: "W", Frequency: "Hz", Humidity: "%",
    Percent: "%", Energy: "J", Pressure: "Pa",
  };
  return u[s.ReadingType] ?? (s.ReadingUnits ?? "");
}

function fmt(v) {
  if (v == null || Number.isNaN(v)) return "\u2014";
  const a = Math.abs(v);
  if (a !== 0 && a < 1) return v.toFixed(3);
  if (a < 100) return v.toFixed(1);
  return Math.round(v).toString();
}

// --- create or update a telemetry card with a coloured sparkline ---
function upsertCard(key, opts) {
  let card = document.getElementById("card-" + key);
  if (!card) {
    const color = PALETTE[charts.size % PALETTE.length];
    card = document.createElement("div");
    card.className = "card";
    card.id = "card-" + key;
    card.innerHTML = `
      <div class="name"><span class="cardName"></span><span class="tag"></span></div>
      <div class="value"><span class="num">\u2014</span><span class="unit"></span></div>
      <div class="sub"></div>
      <canvas></canvas>`;
    const grid = document.getElementById("grid-" + (opts.sectionId || DEFAULT_SECTION))
               || $("sensorSections");
    grid.appendChild(card);
    // CSP blocks style="" markup but allows CSSOM from script; flex/grid "order"
    // lets us slot the card into its configured in-section position.
    if (opts.order != null) card.style.order = String(opts.order);
    const data = { labels: [], values: [] };
    let chart = null;
    if (HAS_CHART) {
      const ctx = card.querySelector("canvas").getContext("2d");
      const grad = ctx.createLinearGradient(0, 0, 0, 42);
      grad.addColorStop(0, color + "55");
      grad.addColorStop(1, color + "00");
      chart = new Chart(ctx, {
        type: "line",
        data: { labels: data.labels, datasets: [{
          data: data.values, borderColor: color, borderWidth: 1.6,
          pointRadius: 0, tension: 0.3, fill: true, backgroundColor: grad }] },
        options: { animation: false, responsive: true, maintainAspectRatio: false,
          scales: { x: { display: false }, y: { display: false } },
          plugins: { legend: { display: false }, tooltip: { enabled: false } } },
      });
    } else {
      card.querySelector("canvas").style.display = "none";
    }
    charts.set(key, { chart, data, color });
    card.querySelector(".cardName").textContent = opts.name;
    card.querySelector(".tag").textContent = opts.tag ?? "";
  }
  const numEl = card.querySelector(".num");
  const unitEl = card.querySelector(".unit");
  const subEl = card.querySelector(".sub");
  card.classList.toggle("pending", opts.value == null);
  card.classList.remove("warn", "bad");
  if (opts.state === "warn") card.classList.add("warn");
  if (opts.state === "bad") card.classList.add("bad");
  numEl.textContent = fmt(opts.value);
  unitEl.textContent = opts.value == null ? "" : (" " + (opts.unit ?? ""));
  if (opts.sub != null) subEl.textContent = opts.sub;

  if (opts.value != null && !Number.isNaN(opts.value)) {
    const { chart, data } = charts.get(key);
    data.labels.push(""); data.values.push(opts.value);
    while (data.values.length > CONFIG.history) { data.labels.shift(); data.values.shift(); }
    if (chart) chart.update("none");
  }
}

// --- thresholds -> state colour, using Redfish Thresholds if present ---
function sensorState(s) {
  const t = s.Thresholds; const v = s.Reading;
  if (t == null || v == null) return "ok";
  const uc = t.UpperCritical?.Reading, lc = t.LowerCritical?.Reading;
  const uw = t.UpperCaution?.Reading, lw = t.LowerCaution?.Reading;
  if ((uc != null && v >= uc) || (lc != null && v <= lc)) return "bad";
  if ((uw != null && v >= uw) || (lw != null && v <= lw)) return "warn";
  return "ok";
}

// --- probe OEM endpoints purely to drive the topology presence dots. We do NOT
// render heater/accel telemetry cards: they have no backend yet, so an empty card
// adds noise. The lightweight probe still tells the Heater/Accel topology nodes
// whether a backend has appeared (red until then). Fan PWM is shown via the
// discovered "Fan Ctrl PWM" Redfish sensor, so no OEM PWM card either. ---
async function pollOem() {
  const present = { heater: false, accel: false };
  try { await rf(CONFIG.oem.heater); present.heater = true; } catch (_) {}
  try { await rf(CONFIG.oem.accel);  present.accel  = true; } catch (_) {}
  return present;
}

// --- ID EEPROM (CAT24C32 @ 0x50 on HPS I2C1). entity-manager's fru-device reads
// a valid IPMI FRU from it once the bus is enabled and the chip is programmed,
// publishing it as inventory -> Redfish. We light the "ID EEPROM" topology node
// when a chassis exposes a real (FRU-sourced) serial: the static System/BMC
// inventory leaves SerialNumber "Unknown", so a concrete serial means the EEPROM
// was actually read. Best-effort and non-fatal; the node stays red until then. ---
async function pollEeprom() {
  try {
    const coll = await rf("/redfish/v1/Chassis");
    for (const m of coll.Members ?? []) {
      let ch;
      try { ch = await rf(m["@odata.id"]); } catch (_) { continue; }
      const sn = String(ch.SerialNumber ?? "").trim();
      if (sn && sn.toLowerCase() !== "unknown") {
        const label = [ch.Model || ch.PartNumber, "SN " + sn].filter(Boolean).join(" \u00B7 ");
        return { present: true, label };
      }
    }
  } catch (_) { /* Redfish unreachable; leave node red */ }
  return { present: false, label: null };
}

// --- topology status dots: green = sensor present/publishing, red = not detected ---
function setDot(id, present) {
  const el = $(id);
  if (el) el.className = "dot " + (present ? "ok" : "bad");
}

// On-die SDM temps are always present even with no external sensor attached;
// exclude them so the "Temp Sensor" node reflects the THERMO 10 / sensor board.
// power.?meter also excludes the TSC1641 die temps (Heater/Fan Power Meter Temp),
// which are the power-meter's own die, not the board thermal.
const SDM_RE = /sdm|die|xcvr|transceiver|fpga|soc|on.?die|power.?meter/i;
// Slider telemetry from the QT Py UART (Slider, humidity-namespace %). Drives the
// "Heater + Slider" topology node. Match "slider"/"knob" so the TSC1641 Heater_*
// power-meter sensors are counted on the Power Meter node, not here.
const HEATER_RE = /slider|knob/i;
// Accelerometer telemetry: the iis2dulpx-accel daemon publishes a Fan_Vibration
// humidity-namespace (%) sensor. Its presence lights the "Accel" topology node even
// without an OEM endpoint.
const ACCEL_RE = /vibration|accel/i;

function updateTopology(sensors, oem) {
  const isNum = (s) => typeof s.Reading === "number";
  const named = (s) => s.Name || s.Id || "";
  const has = (pred) => sensors.some(pred);
  setDot("dot-temp",  has(s => s.ReadingType === "Temperature" && isNum(s) && !SDM_RE.test(named(s))));
  setDot("dot-power", has(s => ["Power", "Current", "Voltage"].includes(s.ReadingType) && isNum(s) && !HEATER_RE.test(named(s))));
  // A fan-tach D-Bus object always exists; a *physical* fan is only "detected"
  // when the tach actually reports RPM > 0 (0 RPM = no fan / stalled).
  setDot("dot-fan",   has(s => s.ReadingType === "Rotational" && isNum(s) && s.Reading > 0));
  setDot("dot-accel", !!oem.accel || has(s => ACCEL_RE.test(named(s)) && isNum(s)));
  // Heater node: green when the QT Py UART bridge is publishing (Slider
  // reaches Redfish), or if an OEM heater backend ever appears.
  setDot("dot-heater", !!oem.heater || has(s => HEATER_RE.test(named(s)) && isNum(s)));
}

function topologyUnknown() {
  for (const id of ["dot-temp", "dot-power", "dot-accel", "dot-fan", "dot-heater", "dot-eeprom"]) {
    const el = $(id); if (el) el.className = "dot pending";
  }
}

// --- pull recent Redfish event-log entries; resolve the working path once ---
async function fetchEventLog() {
  const tryPath = async (p) => {
    const j = await rf(p + "?$top=" + CONFIG.logMax);
    return Array.isArray(j.Members) ? j.Members : [];
  };
  if (logPath) { try { return await tryPath(logPath); } catch (_) { logPath = null; } }
  for (const p of CONFIG.logPaths) {
    try { const m = await tryPath(p); logPath = p; return m; } catch (_) {}
  }
  return null;   // no log service reachable
}

function sevClass(s) {
  if (s === "Critical") return "sev-bad";
  if (s === "Warning")  return "sev-warn";
  return "";
}

function renderLog(entries) {
  const box = $("eventLog");
  if (entries == null) {
    box.innerHTML = '<div class="log-empty">No Redfish LogService reachable on this BMC.</div>';
    return;
  }
  if (entries.length === 0) {
    box.innerHTML = '<div class="log-empty">No event-log entries yet.</div>';
    return;
  }
  // newest last (mockup style), keep most-recent N
  const rows = entries.slice(-CONFIG.logMax).map((e) => {
    const t = e.Created ? new Date(e.Created) : null;
    const ts = t ? t.toLocaleTimeString([], { hour12: false }) : "--:--:--";
    const msg = e.Message || e.MessageId || "(no message)";
    const cls = sevClass(e.Severity);
    return `<div class="log-row ${cls}"><span class="lt">${ts}</span><span class="lm"></span></div>`;
  });
  box.innerHTML = rows.join("");
  // set messages via textContent to avoid HTML injection from log strings
  const msgs = entries.slice(-CONFIG.logMax).map((e) => e.Message || e.MessageId || "(no message)");
  box.querySelectorAll(".lm").forEach((el, i) => { el.textContent = msgs[i]; });
  box.scrollTop = box.scrollHeight;
}

// --- one poll cycle ---
async function poll() {
  try {
    const sensors = await fetchSensors();
    $("emptyHint").style.display = sensors.length ? "none" : "block";
    for (const s of sensors) {
      const id = (s.Id || s.Name || s["@odata.id"]?.split("/").pop() || "sensor");
      const key = "rf-" + id.replace(/[^a-z0-9]/gi, "_");
      const place = placement(s, id);
      upsertCard(key, {
        name: place.name,
        sectionId: place.sectionId,
        order: place.order,
        tag: s.ReadingType || "",
        value: (typeof s.Reading === "number") ? s.Reading : null,
        unit: unitFor(s),
        state: sensorState(s),
        sub: s.Status?.Health ? `health: ${s.Status.Health}` : "",
      });
    }
    const oem = await pollOem();
    updateTopology(sensors, oem);
    const eeprom = await pollEeprom();
    setDot("dot-eeprom", eeprom.present);
    if (eeprom.label) setText("eepromInfo", eeprom.label);
    try { renderLog(await fetchEventLog()); } catch (_) { renderLog(null); }
    setStatus(true, `${sensors.length} sensors \u00B7 ${new Date().toLocaleTimeString([], { hour12: false })}`);
    $("error").textContent = "";
  } catch (e) {
    setStatus(false, "poll failed");
    topologyUnknown();
    $("error").textContent = String(e.message || e) +
      "  (check BMC URL, login, and CORS \u2014 see README)";
  }
}

function setStatus(ok, txt) {
  const d = $("dot");
  d.className = "dot " + (ok ? "ok" : "bad");
  $("statusTxt").textContent = txt;
  const topo = $("topoDot");
  if (topo) topo.className = "dot " + (ok ? "ok" : "bad");
}

function start() {
  if (timer) clearInterval(timer);
  ensureSections();
  poll();
  timer = setInterval(poll, CONFIG.pollMs);
}

async function init() {
  $("bmc").value = CONFIG.bmcBase;
  $("interval").value = CONFIG.pollMs;
  $("apply").addEventListener("click", () => {
    CONFIG.bmcBase = $("bmc").value.trim().replace(/\/$/, "");
    CONFIG.pollMs = Math.max(500, +$("interval").value || 2000);
    localStorage.setItem("bmcBase", CONFIG.bmcBase);
    localStorage.setItem("pollMs", CONFIG.pollMs);
    boot();
  });
  boot();
}

async function boot() {
  setStatus(false, "connecting\u2026");
  logPath = null;
  try {
    await discoverChassis();
    $("chassisLabel").textContent = "chassis: " + chassisId;
    setText("bmcModel", chassisModel || chassisId);
    start();
  } catch (e) {
    setStatus(false, "no chassis");
    $("error").textContent = "Could not reach Redfish: " + (e.message || e);
  }
}

window.addEventListener("DOMContentLoaded", init);
