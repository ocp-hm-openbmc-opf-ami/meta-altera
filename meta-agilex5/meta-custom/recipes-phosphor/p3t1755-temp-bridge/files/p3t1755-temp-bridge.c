// SPDX-License-Identifier: MIT
//
// p3t1755-temp-bridge — publish the Thermo 10 Click (NXP P3T1755) temperature
// to D-Bus / Redfish.
//
// On the Agilex 5 the P3T1755 sits on the Synopsys DesignWare I3C bus and is
// enumerated *natively as an I3C device* (lm75_i3c). It therefore shows up as a
// hwmon node (/sys/class/hwmon/hwmonN with name "p3t1755") but NOT on a legacy
// i2c bus. dbus-sensors only knows how to instantiate legacy-i2c hwmon devices,
// so it cannot read the I3C-native P3T1755 and "Board_Temp" never reaches
// D-Bus (the dashboard tile stays empty and phosphor-pid-control sees no input).
//
// This small daemon bridges the gap: it locates the p3t1755 hwmon, reads
// temp1_input (millidegrees C) every poll interval, and pushes the value onto
// the dbus-sensors ExternalSensor "Board_Temp" (Units=DegreesC ->
// /xyz/openbmc_project/sensors/temperature/Board_Temp). That is the exact path
// phosphor-pid-control consumes, so this also feeds the closed thermal loop.
//
// The only build dependency beyond libc is libsystemd (sd-bus).
//
// Overrides via env:
//   P3T_HWMON_NAME   hwmon "name" to match            (default "p3t1755")
//   P3T_SENSOR_PATH  ExternalSensor object path        (default below)
//   P3T_POLL_SEC     poll interval in seconds          (default 1)

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

/* dbus-sensors ExternalSensor that surfaces the temperature on Redfish. Must
 * match the entity-manager "Board_Temp" Exposes entry (Units=DegreesC ->
 * /xyz/openbmc_project/sensors/temperature/Board_Temp). */
#define SENSOR_SERVICE "xyz.openbmc_project.ExternalSensor"
#define SENSOR_IFACE "xyz.openbmc_project.Sensor.Value"
static const char *sensor_path = "/xyz/openbmc_project/sensors/temperature/Board_Temp";

static const char *hwmon_name = "p3t1755";

static sd_bus *bus = NULL;
static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

/* Read a sysfs string into buf, trimming a single trailing newline. */
static int read_sysfs_str(const char *path, char *buf, size_t len)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (!fgets(buf, (int)len, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    return 0;
}

/* Scan /sys/class/hwmon for a node whose "name" matches hwmon_name and write
 * the path to its temp1_input into out. Returns 0 on success. The lookup is
 * redone whenever a read fails, so hwmon index churn / late probe is tolerated.
 */
static int find_temp_input(char *out, size_t out_len)
{
    const char *base = "/sys/class/hwmon";
    DIR *d = opendir(base);
    if (!d)
        return -1;
    struct dirent *e;
    int found = -1;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "hwmon", 5) != 0)
            continue;
        char namep[256];
        char name[64];
        snprintf(namep, sizeof(namep), "%s/%s/name", base, e->d_name);
        if (read_sysfs_str(namep, name, sizeof(name)) < 0)
            continue;
        if (strcmp(name, hwmon_name) != 0)
            continue;
        snprintf(out, out_len, "%s/%s/temp1_input", base, e->d_name);
        found = 0;
        break;
    }
    closedir(d);
    return found;
}

/* Push the temperature (deg C) onto the ExternalSensor Value property. Failure
 * is non-fatal: the object may not exist yet (entity-manager still publishing)
 * or the bus may be momentarily down — log throttled and retry next poll. */
static void publish_temp(double deg_c)
{
    if (!bus)
        return;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_set_property(bus, SENSOR_SERVICE, sensor_path, SENSOR_IFACE, "Value", &err, "d", deg_c);
    if (r < 0) {
        static time_t last_err = 0;
        time_t now = time(NULL);
        if (now != last_err) {
            last_err = now;
            fprintf(stderr, "p3t1755: set Value failed: %s\n", err.message ? err.message : strerror(-r));
        }
    }
    sd_bus_error_free(&err);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *n = getenv("P3T_HWMON_NAME");
    if (n && *n)
        hwmon_name = n;
    const char *sp = getenv("P3T_SENSOR_PATH");
    if (sp && *sp)
        sensor_path = sp;
    long poll_sec = 1;
    const char *ps = getenv("P3T_POLL_SEC");
    if (ps && *ps) {
        long v = strtol(ps, NULL, 10);
        if (v > 0)
            poll_sec = v;
    }

    struct sigaction sa = { 0 };
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);

    int br = sd_bus_open_system(&bus);
    if (br < 0) {
        fprintf(stderr, "p3t1755: sd_bus_open_system failed: %s\n", strerror(-br));
        return 1;
    }

    printf("p3t1755: bridging hwmon '%s' temp1_input -> %s every %lds\n", hwmon_name, sensor_path, poll_sec);

    char input_path[320] = { 0 };
    int have_path = (find_temp_input(input_path, sizeof(input_path)) == 0);
    time_t last_log = 0;

    while (running) {
        if (!have_path)
            have_path = (find_temp_input(input_path, sizeof(input_path)) == 0);

        if (have_path) {
            char raw[64];
            if (read_sysfs_str(input_path, raw, sizeof(raw)) == 0) {
                char *end = NULL;
                long milli = strtol(raw, &end, 10);
                if (end != raw) {
                    double deg_c = milli / 1000.0;
                    publish_temp(deg_c);
                    time_t now = time(NULL);
                    if (now != last_log) { /* throttle journal to ~1 Hz */
                        last_log = now;
                        printf("p3t1755: %.3f C\n", deg_c);
                    }
                }
            } else {
                /* hwmon went away (index churn / re-probe); re-resolve. */
                have_path = 0;
            }
        } else {
            static time_t last_warn = 0;
            time_t now = time(NULL);
            if (now != last_warn) {
                last_warn = now;
                fprintf(stderr, "p3t1755: hwmon '%s' not found yet\n", hwmon_name);
            }
        }

        /* Sleep in 1s slices so SIGTERM is noticed promptly. */
        for (long s = 0; s < poll_sec && running; s++)
            sleep(1);
    }

    printf("p3t1755: exiting\n");
    if (bus)
        sd_bus_unref(bus);
    return 0;
}
