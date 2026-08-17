// SPDX-License-Identifier: MIT
//
// tsc1641-current-bridge — publish the Altera sensor board's two on-board ST
// TSC1641 power monitors to D-Bus / Redfish.
//
// The board carries TWO TSC1641 (see altera-sensor-board schematic):
//   VH   rail (U3, ADDR 1000010 = static 0x42) — heater rail, 0.1 ohm shunt
//   VFAN rail (U8, ADDR 1000011 = static 0x43) — fan rail,    0.1 ohm shunt
//
// Both enumerate natively on the Synopsys DesignWare I3C bus (the controller
// cannot mix legacy-I2C and native-I3C on one bus, and the P3T1755 already
// enumerates natively), so each appears only as a hwmon node named "tsc1641" —
// NOT on a legacy i2c bus. dbus-sensors' PSUSensor only instantiates legacy-i2c
// hwmon devices, so these sensors never reach D-Bus on their own.
//
// Because BOTH chips share the hwmon "name" ("tsc1641"), they are told apart by
// the I3C address embedded in the sysfs device path that each hwmon resolves to:
//   VH   -> i3c "0-208020a2001"   (provisional ID of the 0x42 part)
//   VFAN -> i3c "0-208020a3001"   (provisional ID of the 0x43 part)
// This daemon resolves each hwmonN's canonical path, matches that substring to a
// rail, and every poll interval reads each input attribute and pushes the value
// (converted to SI base units) onto that rail's ExternalSensors. The eight
// ExternalSensors must be declared in the entity-manager config (Units -> path
// namespace; see dbus-sensors SensorPaths getPathForUnits):
//   in0_input    (mV)  -> <rail>_Voltage   Units=Volts    .../voltage/...
//   curr1_input  (mA)  -> <rail>_Current   Units=Amperes  .../current/...
//   power1_input (uW)  -> <rail>_Power      Units=Watts    .../power/...
//   temp1_input  (mC)  -> <rail>_Die_Temp  Units=DegreesC .../temperature/...
//
// The shunt scaling (0.1 ohm) is applied by the kernel tsc1641 driver via the
// per-node shunt-resistor-micro-ohms = <100000> DT property, so curr1/power1
// already account for it; the divisors below only convert hwmon units to SI.
//
// The only build dependency beyond libc is libsystemd (sd-bus).
//
// Overrides via env:
//   TSC_HWMON_NAME   hwmon "name" to match      (default "tsc1641")
//   TSC_POLL_SEC     poll interval in seconds    (default 1)

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

#define SENSOR_SERVICE "xyz.openbmc_project.ExternalSensor"
#define SENSOR_IFACE "xyz.openbmc_project.Sensor.Value"

/* One TSC1641 hwmon input attribute -> one dbus-sensors ExternalSensor.
 * "divisor" converts the standard-hwmon raw integer to the SI base unit the
 * ExternalSensor expects: in*_input is mV, curr*_input is mA, power*_input is
 * uW, temp*_input is m degC. The path namespace must match the entity-manager
 * "Units" (Volts->voltage, Amperes->current, Watts->power, DegreesC->
 * temperature); see dbus-sensors SensorPaths getPathForUnits. */
struct channel {
    const char *attr;
    double divisor;
    const char *path;
    const char *label;
};

#define NCHAN 4

/* One physical TSC1641, identified by a substring of the I3C sysfs device path
 * (the provisional ID the bus assigns at enumeration, which differs per static
 * address). hwmon_dir is re-resolved whenever a read fails so hwmon index churn
 * or a late driver probe is tolerated. */
struct tsc_device {
    const char *id; /* short tag for the journal */
    const char *match; /* substring expected in the hwmon's canonical path */
    struct channel channels[NCHAN];
    char hwmon_dir[300];
    int have_dir;
};

static struct tsc_device devices[] = {
    {
        .id = "Heater",
        .match = "208020a2001",
        .channels =
            {
                {"in0_input", 1000.0,
                 "/xyz/openbmc_project/sensors/voltage/Heater_Supply_Voltage", "V"},
                {"curr1_input", 1000.0,
                 "/xyz/openbmc_project/sensors/current/Heater_Current", "A"},
                {"power1_input", 1000000.0,
                 "/xyz/openbmc_project/sensors/power/Heater_Power", "W"},
                {"temp1_input", 1000.0,
                 "/xyz/openbmc_project/sensors/temperature/Heater_Power_Meter_Temp", "C"},
            },
    },
    {
        .id = "Fan",
        .match = "208020a3001",
        .channels =
            {
                {"in0_input", 1000.0,
                 "/xyz/openbmc_project/sensors/voltage/Fan_Supply_Voltage", "V"},
                {"curr1_input", 1000.0,
                 "/xyz/openbmc_project/sensors/current/Fan_Current", "A"},
                {"power1_input", 1000000.0,
                 "/xyz/openbmc_project/sensors/power/Fan_Power", "W"},
                {"temp1_input", 1000.0,
                 "/xyz/openbmc_project/sensors/temperature/Fan_Power_Meter_Temp", "C"},
            },
    },
};
#define NDEV ((int)(sizeof(devices) / sizeof(devices[0])))

static const char *hwmon_name = "tsc1641";

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

/* Find the /sys/class/hwmon/hwmonN whose "name" matches hwmon_name AND whose
 * canonical device path contains dev->match (the I3C address that tells this
 * chip apart from its twin). Copies the hwmonN path into dev->hwmon_dir.
 * Returns 0 on success. */
static int resolve_device(struct tsc_device *dev)
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
        char dirp[300];
        char namep[400];
        char name[64];
        snprintf(dirp, sizeof(dirp), "%s/%s", base, e->d_name);
        snprintf(namep, sizeof(namep), "%s/name", dirp);
        if (read_sysfs_str(namep, name, sizeof(name)) < 0)
            continue;
        if (strcmp(name, hwmon_name) != 0)
            continue;
        /* Resolve the canonical path so the I3C address is visible, then make
         * sure THIS hwmon belongs to the chip we want. */
        char real[PATH_MAX];
        if (!realpath(dirp, real))
            continue;
        if (!strstr(real, dev->match))
            continue;
        snprintf(dev->hwmon_dir, sizeof(dev->hwmon_dir), "%s", dirp);
        found = 0;
        break;
    }
    closedir(d);
    return found;
}

/* Push a value onto an ExternalSensor Value property. Failure is non-fatal: the
 * object may not exist yet (entity-manager still publishing) or the bus may be
 * momentarily down — log throttled to ~1 Hz and retry next poll. */
static void publish(const char *path, double value)
{
    if (!bus)
        return;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = sd_bus_set_property(bus, SENSOR_SERVICE, path, SENSOR_IFACE, "Value", &err, "d", value);
    if (r < 0) {
        static time_t last_err = 0;
        time_t now = time(NULL);
        if (now != last_err) {
            last_err = now;
            fprintf(stderr, "tsc1641: set Value on %s failed: %s\n", path, err.message ? err.message : strerror(-r));
        }
    }
    sd_bus_error_free(&err);
}

/* Read one device's hwmon attributes and publish them. Returns 1 if at least
 * one attribute was read (device still present), 0 otherwise. */
static int poll_device(struct tsc_device *dev, int do_log)
{
    int read_ok = 0;
    for (int i = 0; i < NCHAN; i++) {
        char path[384];
        char raw[64];
        snprintf(path, sizeof(path), "%s/%s", dev->hwmon_dir, dev->channels[i].attr);
        if (read_sysfs_str(path, raw, sizeof(raw)) != 0)
            continue;
        char *end = NULL;
        long rawv = strtol(raw, &end, 10);
        if (end == raw)
            continue;
        double value = rawv / dev->channels[i].divisor;
        publish(dev->channels[i].path, value);
        read_ok = 1;
        if (do_log)
            printf("tsc1641: %s %s = %.4f %s\n", dev->id, dev->channels[i].attr, value, dev->channels[i].label);
    }
    return read_ok;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *n = getenv("TSC_HWMON_NAME");
    if (n && *n)
        hwmon_name = n;
    long poll_sec = 1;
    const char *ps = getenv("TSC_POLL_SEC");
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
        fprintf(stderr, "tsc1641: sd_bus_open_system failed: %s\n", strerror(-br));
        return 1;
    }

    printf("tsc1641: bridging %d hwmon '%s' device(s) -> %d ExternalSensors "
           "each, every %lds\n",
           NDEV, hwmon_name, NCHAN, poll_sec);

    for (int i = 0; i < NDEV; i++)
        devices[i].have_dir = (resolve_device(&devices[i]) == 0);
    time_t last_log = 0;

    while (running) {
        time_t now = time(NULL);
        int do_log = (now != last_log); /* throttle journal to ~1 Hz */

        for (int i = 0; i < NDEV; i++) {
            struct tsc_device *dev = &devices[i];
            if (!dev->have_dir)
                dev->have_dir = (resolve_device(dev) == 0);

            if (dev->have_dir) {
                if (!poll_device(dev, do_log))
                    dev->have_dir = 0; /* hwmon vanished; re-resolve next poll */
            } else if (do_log) {
                static time_t last_warn = 0;
                if (now != last_warn) {
                    last_warn = now;
                    fprintf(stderr, "tsc1641: %s hwmon '%s' (%s) not found yet\n", dev->id, hwmon_name, dev->match);
                }
            }
        }

        if (do_log)
            last_log = now;

        /* Sleep in 1s slices so SIGTERM is noticed promptly. */
        for (long s = 0; s < poll_sec && running; s++)
            sleep(1);
    }

    printf("tsc1641: exiting\n");
    if (bus)
        sd_bus_unref(bus);
    return 0;
}
