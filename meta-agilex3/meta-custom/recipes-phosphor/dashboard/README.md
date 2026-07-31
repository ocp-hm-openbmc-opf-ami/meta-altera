# Agilex 5E OpenBMC — Chassis Dashboard

A branded, **live** view of the BMC's sensors, styled as an Altera
"Programmable Platform Management" console. Auto-refreshing, no build step.

The page auto-discovers every Redfish sensor under
`/redfish/v1/Chassis/<id>/Sensors` and renders a value + colour sparkline per
sensor, alongside a system-topology map (with live presence dots), a "Sensors OK"
health tile, a capability checklist, and the live Redfish event log. Quantities
that are *not* standard Redfish sensors yet (heater, accelerometer) are represented
as **topology nodes** — their dots stay red until a backend appears — rather than
empty telemetry cards.

See `openbmc_functional_spec.md` §C12 and §D4 (Web UI / live telemetry).

## Files (all same-origin — bmcweb CSP blocks inline assets + CDNs)

| File | Purpose |
|------|---------|
| `index.html` | Markup + the **inlined Altera wordmark `<svg>`** (no external image) |
| `dashboard.css` | Theme (navy/Altera-blue gradient) + centered framed layout |
| `dashboard.js` | Redfish polling, card/sparkline rendering, event log |
| `chart.umd.min.js` | Vendored Chart.js for sparklines (`script-src 'self'`) |

bmcweb attaches `default-src 'none'; img-src 'self' data:; style-src 'self';
script-src 'self'`. That blocks inline `<style>`/`<script>`, `style=""`, and any
CDN — hence the split files, the **inlined SVG logo** (an inline SVG element is
part of the document, not a fetch), and the **vendored** Chart.js. If
`chart.umd.min.js` is ever absent the dashboard still shows live values, just
without sparklines (Chart.js use is guarded).

## What it shows

- **System Topology** — node map of this board's signal chain (BMC → Temp →
  Power Meter → Accel → Fan+Tach → Heater+Slider). Each status dot is **live**:
  **green = that sensor is present/publishing, red = not detected** (the head BMC
  dot tracks Redfish connectivity). On-die SDM temps are excluded from the "Temp
  Sensor" check (regex `SDM_RE` in `dashboard.js`) so that dot reflects the
  external THERMO 10 / sensor-board temp, not the always-present die sensors. The
  **Fan** dot needs the tach to report **RPM > 0** (a tach D-Bus object always
  exists; 0 RPM means no fan / stalled, so it stays red).
- **Telemetry Overview** — every discovered Redfish sensor (temperature, fan
  RPM/PWM, voltage, …). Each keeps a rolling sparkline (default 60 points).
  Warn/bad colours come from the sensor's Redfish `Thresholds` if present.
- **Temperature-Driven Control** — static capability checklist scoped to this
  demo (temperature-driven fan control via swampd, failsafe fan policy on sensor
  loss, multi-protocol sensor aggregation over I2C/I3C/SPI/UART, Redfish remote
  telemetry & control).
- **Event Log** — real Redfish log entries (see below).

## Hosting (how it's served)

Packaged by the `dashboard` recipe into `/usr/share/www/dashboard/`.
bmcweb recursively scans `/usr/share/www` at startup and registers a route per
file, so it is served **same-origin** with Redfish at **`https://<bmc>/dashboard/`**
after boot — no CORS, no separate web server; it reuses the bmcweb login cookie.

Reach it directly, or via an SSH tunnel:

```bash
ssh -L 127.0.0.1:8443:<bmc>:443 <jumphost>
# then browse https://localhost:8443/dashboard/   (note the trailing slash)
```

The stock webui-vue stays at `/`; both coexist. Leave the **BMC** field blank so
fetches use the same origin (the session cookie authenticates automatically once
you've logged into the web UI / Redfish).

## Event log endpoints

`dashboard.js` probes these in order and caches the first that answers (hostless
BMCs may not have a "system"):

1. `/redfish/v1/Systems/system/LogServices/EventLog/Entries`
2. `/redfish/v1/Managers/bmc/LogServices/EventLog/Entries`
3. `/redfish/v1/Managers/bmc/LogServices/Journal/Entries`

## Wiring the demo quantities (when backends exist)

Edit `CONFIG.oem` in `dashboard.js` to point at the real endpoints:

| Card | Expected JSON | Backend to build (spec) |
|------|---------------|--------------------------|
| Fan PWM duty | `{ "DutyCycle": 0-100 }` | fansensor `Pwm` / phosphor-pid-control (D4.3) |
| Heater | `{ "PowerWatts": n, "SetpointC": n }` | custom UART↔D-Bus bridge for QT Py (D4.3) |
| Accelerometer | `{ "x": g, "y": g, "z": g }` | IIO driver + OEM/side-panel (D4.3) |

If you later expose PWM/heater as *standard* Redfish controls/sensors instead of
OEM paths, they'll also appear automatically in Telemetry Overview and the demo
cards can be removed — exactly the "prune later" path.

## Updating the vendored Chart.js

```bash
curl -L -o files/chart.umd.min.js \
  https://cdn.jsdelivr.net/npm/chart.js@4.4.3/dist/chart.umd.min.js
```
