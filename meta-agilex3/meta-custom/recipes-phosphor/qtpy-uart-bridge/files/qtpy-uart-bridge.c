// SPDX-License-Identifier: MIT
//
// qtpy-uart-bridge — read line-based telemetry from the Altera sensor board's
// QT Py MCU over the Pi-header UART (/dev/ttyS1 = HPS UART1, FPGA-muxed) and
// publish the latest values as JSON to /run for other consumers.
//
// This is bring-up/demo scaffolding. The QT Py firmware streams ASCII lines of
// whitespace-separated KEY:VALUE tokens, e.g.:
//
//     Sliders: 32000
//
// SLIDERS (potentiometer / knob position) is the value the QT Py reports,
// streamed continuously at ~10 Hz (every 100 ms). It is a raw 16-bit ADC count
// (0-65535, CircuitPython's analog scale); this bridge scales it to 0-100% for
// the "Slider" sensor (the BMC marks the sensor unavailable after ~15 s of
// silence). On the Altera sensor board the heater power and temperatures are
// measured by BMC-side I3C sensors (FPGA), not the QT Py, so SLIDERS is normally
// the only token present. The legacy KNOB:<0-100>% token (already a percent) and
// the other tokens below are still accepted for loopback/dev convenience. See
// qtpy-uart-protocol.md.
//
// The parser is deliberately tolerant: it scans each line for known keys and
// ignores anything else, so the exact QT Py format can change without breaking
// it. A "KEY: value" form with a space after the colon is accepted too.
// Recognised keys (case-insensitive; trailing unit letters stripped):
//
//     SLIDERS | SLIDER      -> knob_pct    (raw 0-65535 ADC, scaled to percent)
//     KNOB    | POT          -> knob_pct   (percent, taken as-is)
//     HEATER  | POWER        -> power_w    (Watts)
//     SET     | SETPOINT     -> setpoint_c (deg C)
//     TEMP                   -> temp_c     (deg C)
//
// Output:
//   1. The knob position is mirrored onto a dbus-sensors ExternalSensor so it
//      appears on the Redfish dashboard with history automatically (no bmcweb
//      change):
//        - knob position -> "Slider" (Units=PercentRH -> /sensors/humidity/).
//          The knob is the stimulus the requirement tracks ("...over time as the
//          potentiometer knob is adjusted"). bmcweb does not surface the "percent"
//          namespace that Units=Percent would land in, so the config uses
//          PercentRH: dbus-sensors maps it to the "humidity" namespace, which
//          bmcweb does expose under Chassis/Sensors as a "%" reading, so the
//          slider shows as a dashboard tile.
//   2. /run/qtpy/heater.json, refreshed on every parsed line, as a debug aid.
//      Any legacy tokens (HEATER/SET/TEMP) are captured here only — they are not
//      published to Redfish (heater power/temp come from BMC-side I3C sensors).
//   3. A throttled journal line.
//
// The only build dependency beyond libc + termios is libsystemd (sd-bus), used
// to set the ExternalSensor Value property.
//
// Usage: qtpy-uart-bridge [device] [baud]   (defaults: /dev/ttyS1 115200)

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

#include <sys/stat.h>

static const char *default_dev = "/dev/ttyS1";
static const char *json_path = "/run/qtpy/heater.json"; /* override: QTPY_JSON */

/* dbus-sensors ExternalSensor that mirrors the knob value onto Redfish. The path
 * must match the object dbus-sensors creates from the entity-manager Exposes
 * entry. Slider uses Units=PercentRH, which dbus-sensors maps to the
 * "humidity" namespace (bmcweb exposes it as a "%" reading; the "percent"
 * namespace that Units=Percent would use is not surfaced by bmcweb).
 * Override path: QTPY_KNOB. */
#define SENSOR_SERVICE "xyz.openbmc_project.ExternalSensor"
#define SENSOR_IFACE "xyz.openbmc_project.Sensor.Value"
static const char *knob_path = "/xyz/openbmc_project/sensors/humidity/Slider";

static sd_bus *bus = NULL;

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static speed_t baud_to_speed(long baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return B0;
    }
}

static int configure_tty(int fd, speed_t speed)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0)
        return -1;
    cfmakeraw(&tio);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    tio.c_cflag |= (CLOCAL | CREAD); /* ignore modem lines, enable receiver */
    tio.c_cflag &= ~CRTSCTS; /* no hardware flow control */
    /* VMIN=0/VTIME=10: read() returns as soon as bytes arrive, or after 1 s of
     * idle with 0 bytes. The periodic wake lets the main loop notice a stop
     * request even when no UART data is flowing. */
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;
    if (tcsetattr(fd, TCSANOW, &tio) < 0)
        return -1;
    tcflush(fd, TCIFLUSH);
    return 0;
}

/* Latest decoded telemetry; "*_seen" flags gate them into the JSON. */
struct telemetry {
    double power_w, setpoint_c, knob_pct, temp_c;
    int power_seen, setpoint_seen, knob_seen, temp_seen;
};

/* Parse the numeric prefix of a value token (handles a leading sign and a
 * trailing unit letter like W / C / %). Returns 1 on success. */
static int parse_num(const char *s, double *out)
{
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return 0;
    *out = v;
    return 1;
}

/* Apply one recognised KEY/number pair to the telemetry struct. The QT Py's
 * SLIDERS value is a raw 16-bit ADC count (0-65535, CircuitPython's analog
 * scale); scale it to 0-100% for the Slider sensor. The legacy KNOB/POT token is
 * already a percentage and is taken as-is. */
static void apply_kv(const char *key, double num, struct telemetry *t)
{
    if (!strcasecmp(key, "SLIDERS") || !strcasecmp(key, "SLIDER")) {
        double pct = num / 65535.0 * 100.0;
        if (pct < 0.0)
            pct = 0.0;
        else if (pct > 100.0)
            pct = 100.0;
        t->knob_pct = pct;
        t->knob_seen = 1;
    } else if (!strcasecmp(key, "KNOB") || !strcasecmp(key, "POT")) {
        t->knob_pct = num;
        t->knob_seen = 1;
    } else if (!strcasecmp(key, "HEATER") || !strcasecmp(key, "POWER")) {
        t->power_w = num;
        t->power_seen = 1;
    } else if (!strcasecmp(key, "SET") || !strcasecmp(key, "SETPOINT")) {
        t->setpoint_c = num;
        t->setpoint_seen = 1;
    } else if (!strcasecmp(key, "TEMP")) {
        t->temp_c = num;
        t->temp_seen = 1;
    }
}

static void parse_line(char *line, struct telemetry *t)
{
    /* Tokens are whitespace-separated. A token is normally "KEY:VALUE", but when
     * the firmware prints a space after the colon (e.g. "Sliders: 32000") the
     * value arrives as the next token; pending_key carries the key across to it. */
    char pending_key[32] = "";
    for (char *tok = strtok(line, " \t\r\n"); tok != NULL; tok = strtok(NULL, " \t\r\n")) {
        char *colon = strchr(tok, ':');
        if (colon) {
            *colon = '\0';
            const char *key = tok;
            const char *val = colon + 1;
            double num;
            if (*val && parse_num(val, &num)) {
                apply_kv(key, num, t);
                pending_key[0] = '\0';
            } else {
                /* "KEY:" alone — the number is the following token. */
                snprintf(pending_key, sizeof(pending_key), "%s", key);
            }
        } else if (pending_key[0]) {
            double num;
            if (parse_num(tok, &num))
                apply_kv(pending_key, num, t);
            pending_key[0] = '\0';
        }
    }
}

static void write_json(const struct telemetry *t, const char *raw)
{
    /* Ensure the parent directory of json_path exists. */
    char dir[256];
    snprintf(dir, sizeof(dir), "%s", json_path);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) {
        *slash = '\0';
        mkdir(dir, 0755);
    }
    char tmp[300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", json_path);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f, "{\"source\":\"qtpy\"");
    if (t->power_seen)
        fprintf(f, ",\"power_w\":%.2f", t->power_w);
    if (t->setpoint_seen)
        fprintf(f, ",\"setpoint_c\":%.1f", t->setpoint_c);
    if (t->knob_seen)
        fprintf(f, ",\"knob_pct\":%.0f", t->knob_pct);
    if (t->temp_seen)
        fprintf(f, ",\"temp_c\":%.1f", t->temp_c);
    /* Echo the raw line (truncated, quotes stripped) for debugging. */
    char safe[160];
    size_t j = 0;
    for (size_t i = 0; raw[i] && j < sizeof(safe) - 1; i++) {
        char c = raw[i];
        if (c == '"' || c == '\\' || (unsigned char)c < 0x20)
            c = ' ';
        safe[j++] = c;
    }
    safe[j] = '\0';
    fprintf(f, ",\"raw\":\"%s\",\"timestamp\":%lld}\n", safe, (long long)time(NULL));
    fclose(f);
    rename(tmp, json_path);
}

/* (Re)open the system bus. Closes any stale handle first. */
static int connect_bus(void)
{
    if (bus) {
        sd_bus_flush_close_unref(bus);
        bus = NULL;
    }
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        bus = NULL;
        return r;
    }
    return 0;
}

/* Push a value onto an ExternalSensor's Value property. Failure is non-fatal.
 *
 * The bridge only publishes when a UART line arrives, so the bus connection can
 * sit idle for minutes (e.g. sparse loopback testing); an idle, never-serviced
 * sd-bus connection can be dropped by the broker, after which set_property
 * returns -ENOTCONN. Self-heal: (re)connect if the handle is gone/closed, and
 * on a failed write drop the (likely stale) connection and retry once. */
static void publish_value(const char *path, double v)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!bus || sd_bus_is_open(bus) <= 0) {
            if (connect_bus() < 0) {
                static time_t last_err = 0;
                time_t now = time(NULL);
                if (now != last_err) {
                    last_err = now;
                    fprintf(stderr, "qtpy: cannot open system bus\n");
                }
                return;
            }
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        int r = sd_bus_set_property(bus, SENSOR_SERVICE, path, SENSOR_IFACE, "Value", &err, "d", v);
        sd_bus_error_free(&err);
        if (r >= 0)
            return;

        /* Drop the connection so the next iteration reconnects fresh. */
        sd_bus_flush_close_unref(bus);
        bus = NULL;
        if (attempt == 1) {
            static time_t last_err = 0;
            time_t now = time(NULL);
            if (now != last_err) {
                last_err = now;
                fprintf(stderr, "qtpy: set %s Value failed: %s\n", path, strerror(-r));
            }
        }
    }
}

int main(int argc, char **argv)
{
    const char *dev = (argc > 1) ? argv[1] : getenv("QTPY_TTY");
    if (!dev || !*dev)
        dev = default_dev;
    long baud = (argc > 2) ? strtol(argv[2], NULL, 10) : 0;
    if (baud == 0) {
        const char *b = getenv("QTPY_BAUD");
        baud = b ? strtol(b, NULL, 10) : 115200;
    }
    speed_t speed = baud_to_speed(baud);
    if (speed == B0) {
        fprintf(stderr, "qtpy: unsupported baud %ld\n", baud);
        return 1;
    }
    const char *jp = getenv("QTPY_JSON");
    if (jp && *jp)
        json_path = jp;
    const char *kp = getenv("QTPY_KNOB");
    if (kp && *kp)
        knob_path = kp;

    /* sigaction without SA_RESTART so a blocking read() returns EINTR on stop
     * (glibc signal() defaults to SA_RESTART, which would mask SIGTERM here). */
    struct sigaction sa = { 0 };
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);

    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        fprintf(stderr, "qtpy: cannot open %s: %s\n", dev, strerror(errno));
        return 1;
    }
    if (configure_tty(fd, speed) < 0) {
        fprintf(stderr, "qtpy: tty config failed on %s: %s\n", dev, strerror(errno));
        close(fd);
        return 1;
    }
    /* Open the system bus for ExternalSensor updates (non-fatal: publish_value
     * reconnects on demand, so a failure here just delays the first publish). */
    if (connect_bus() < 0) {
        fprintf(stderr, "qtpy: system bus not up yet; will retry on publish, "
                        "/run JSON still works\n");
    }

    printf("qtpy: reading %s @ %ld 8N1; waiting for QT Py lines...\n", dev, baud);

    struct telemetry t = { 0 };
    char line[256];
    size_t len = 0;
    unsigned lines = 0;
    time_t last_log = 0;

    while (running) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "qtpy: read error: %s\n", strerror(errno));
            break;
        }
        if (n == 0)
            continue;
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                if (len == 0)
                    continue;
                line[len] = '\0';
                char raw[256];
                memcpy(raw, line, len + 1);
                parse_line(line, &t);
                write_json(&t, raw);
                if (t.knob_seen)
                    publish_value(knob_path, t.knob_pct);
                lines++;
                time_t now = time(NULL);
                if (now != last_log) { /* throttle journal to ~1 Hz */
                    last_log = now;
                    printf("qtpy: %s\n", raw);
                }
                len = 0;
            } else if (len < sizeof(line) - 1) {
                line[len++] = c;
            } else {
                len = 0; /* overlong line; drop and resync */
            }
        }
    }

    printf("qtpy: exiting after %u lines\n", lines);
    if (bus)
        sd_bus_unref(bus);
    close(fd);
    return 0;
}
