// SPDX-License-Identifier: MIT
//
// iis2dulpx-accel — minimal userspace reader for the ST IIS2DULPX 3-axis
// accelerometer (Altera sensor board) over Linux spidev.
//
// The IIS2DULPX has no in-tree Linux driver in the socfpga-lts 6.18 kernel, so
// it is exposed as a raw SPI slave via spidev (DT child "accel@0" on spi0 /
// SPIM0, bound through the allow-listed "rohm,dh2228fv" compatible). This daemon
// talks the IIS2DULPX register protocol directly: it verifies WHO_AM_I, brings
// the device up at 100 Hz / +-2 g, then polls X/Y/Z, converts to milli-g, logs
// to the journal, and publishes a JSON snapshot to /run for other consumers.
//
// For the web dashboard it also derives a placeholder "fan vibration" percentage
// from the X/Y/Z magnitude (deviation from 1 g at rest) and mirrors it onto the
// dbus-sensors ExternalSensor "Fan_Vibration" (Units=PercentRH -> /sensors/
// humidity/...), which bmcweb surfaces as a "%" reading tile (the "percent"
// namespace that Units=Percent would use is not surfaced by bmcweb). The
// percentage is intentionally a rough stand-in (real vibration analysis comes
// later); the raw X/Y/Z stays in the /run JSON.
//
// Register map / scaling are taken from ST's iis2dulpx-pid driver:
//   WHO_AM_I = 0x0F (expect 0x47); CTRL1 0x10 (bit4 if_add_inc, bit5 sw_reset);
//   CTRL4 0x13 (bit5 bdu); CTRL5 0x14 (odr[7:4], bw[3:2], fs[1:0]);
//   STATUS 0x25 (bit0 DRDY); OUTX_L 0x28 .. OUTZ_H 0x2D (LE int16, auto-inc).
//   +-2 g sensitivity = 0.061 mg/LSB.
//
// SPI framing (ST MEMS): first byte = register address; MSB(0x80)=1 -> read,
// 0 -> write. Multi-byte transfers auto-increment when CTRL1.if_add_inc=1.
//
// This is demo/bring-up tooling, not a production OpenBMC sensor daemon. Beyond
// libc and the kernel spidev uapi it links libm (vibration magnitude) and
// libsystemd (sd-bus, to push the ExternalSensor value).

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

/* IIS2DULPX registers */
#define REG_WHO_AM_I 0x0FU
#define WHO_AM_I_VAL 0x47U
#define REG_CTRL1 0x10U
#define REG_CTRL4 0x13U
#define REG_CTRL5 0x14U
#define REG_STATUS 0x25U
#define REG_OUT_X_L 0x28U
/*
 * EN_DEVICE_CONFIG (0x3E), bit0 SOFT_PD (write-only): the SPI "power-up"
 * command. The IIS2DULPX boots into deep power-down where ALL registers
 * (including WHO_AM_I) are inaccessible, so a raw SPI read returns 0xff (the
 * board's pull-up on the un-driven SDO line). Per datasheet §3.3.1.2 the SPI
 * wake-up is a write of SOFT_PD=1, after which the part reaches soft
 * power-down within 25 ms and WHO_AM_I reads 0x47. (Matches the ST reference
 * driver's iis2dulpx_exit_deep_power_down(), AN5812 §3.1.1.1/3.1.1.2.)
 */
#define REG_EN_DEVICE_CONFIG 0x3EU
#define EN_DEVICE_CONFIG_SOFT_PD 0x01U
#define ACCEL_POWERUP_US 30000 /* >= 25 ms max power-up time */

#define CTRL1_IF_ADD_INC 0x10U
#define CTRL1_SW_RESET 0x20U
#define CTRL4_BDU 0x20U
/* CTRL5: odr=0x8 (100 Hz, low-power) << 4, bw=0, fs=0 (+-2 g) -> 0x80 */
#define CTRL5_100HZ_LP_2G 0x80U
#define STATUS_DRDY 0x01U

#define SENS_MG_PER_LSB_2G 0.061f /* milli-g per LSB at +-2 g */

#define SPI_READ_BIT 0x80U

static const char *default_dev = "/dev/spidev0.0";
static const char *json_path = "/run/iis2dulpx/accel.json";

/* dbus-sensors ExternalSensor that mirrors the derived vibration percentage onto
 * Redfish. Units=PercentRH in the entity-manager config -> "humidity" namespace,
 * which bmcweb exposes as a "%" reading (the "percent" namespace that
 * Units=Percent would use is not surfaced by bmcweb). Override with
 * IIS2DULPX_VIB. */
#define SENSOR_SERVICE "xyz.openbmc_project.ExternalSensor"
#define SENSOR_IFACE "xyz.openbmc_project.Sensor.Value"
static const char *vib_path = "/xyz/openbmc_project/sensors/humidity/Fan_Vibration";

static sd_bus *bus = NULL;

static uint32_t spi_speed = 5000000U; /* 5 MHz (chip supports up to 10 MHz) */
static uint8_t spi_mode = SPI_MODE_3; /* IIS2DULPX 4-wire SPI = mode 3 */
static uint8_t spi_bits = 8U;

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static int spi_read(int fd, uint8_t reg, uint8_t *buf, size_t n)
{
    uint8_t tx[8] = { 0 };
    uint8_t rx[8] = { 0 };
    if (n + 1 > sizeof(tx))
        return -1;
    tx[0] = reg | SPI_READ_BIT;
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = (uint32_t)(n + 1),
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1)
        return -1;
    memcpy(buf, rx + 1, n);
    return 0;
}

static int spi_write(int fd, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7FU), val };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .len = 2,
        .speed_hz = spi_speed,
        .bits_per_word = spi_bits,
    };
    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1 ? -1 : 0;
}

/* Open the configured spidev node; if it is missing, fall back to the first
 * /dev/spidev* found (the controller's bus number can shift). */
static int open_spidev(const char *want, char *chosen, size_t chosen_len)
{
    int fd = open(want, O_RDWR);
    if (fd >= 0) {
        snprintf(chosen, chosen_len, "%s", want);
        return fd;
    }

    DIR *d = opendir("/dev");
    if (!d)
        return -1;
    struct dirent *e;
    char path[300];
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "spidev", 6) != 0)
            continue;
        snprintf(path, sizeof(path), "/dev/%s", e->d_name);
        fd = open(path, O_RDWR);
        if (fd >= 0) {
            snprintf(chosen, chosen_len, "%s", path);
            closedir(d);
            return fd;
        }
    }
    closedir(d);
    return -1;
}

static int configure_spi(int fd)
{
    if (ioctl(fd, SPI_IOC_WR_MODE, &spi_mode) < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits) < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0)
        return -1;
    return 0;
}

static int configure_accel(int fd)
{
    /* Software reset, then wait for it to self-clear. */
    if (spi_write(fd, REG_CTRL1, CTRL1_SW_RESET) < 0)
        return -1;
    for (int i = 0; i < 20; i++) {
        uint8_t c1 = 0;
        usleep(2000);
        if (spi_read(fd, REG_CTRL1, &c1, 1) < 0)
            return -1;
        if (!(c1 & CTRL1_SW_RESET))
            break;
    }

    if (spi_write(fd, REG_CTRL1, CTRL1_IF_ADD_INC) < 0) /* burst auto-increment */
        return -1;
    if (spi_write(fd, REG_CTRL4, CTRL4_BDU) < 0) /* block data update */
        return -1;
    if (spi_write(fd, REG_CTRL5, CTRL5_100HZ_LP_2G) < 0) /* 100 Hz, +-2 g */
        return -1;
    usleep(20000); /* allow first sample at 100 Hz */
    return 0;
}

static void write_json(float xg, float yg, float zg, int ok)
{
    /* Atomic-ish publish: write a temp file then rename into place. */
    mkdir("/run/iis2dulpx", 0755);
    char tmp[300];
    snprintf(tmp, sizeof(tmp), "%s.tmp", json_path);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f,
            "{\"sensor\":\"IIS2DULPX\",\"ok\":%s,"
            "\"x_mg\":%.1f,\"y_mg\":%.1f,\"z_mg\":%.1f,"
            "\"timestamp\":%lld}\n",
            ok ? "true" : "false", xg, yg, zg, (long long)time(NULL));
    fclose(f);
    rename(tmp, json_path);
}

/* Derive a placeholder "fan vibration" percentage from an X/Y/Z sample (mg).
 * At rest the magnitude is ~1 g (1000 mg) regardless of orientation, so the
 * deviation from 1 g is a cheap, orientation-independent motion proxy. Scaled so
 * a full 1 g of deviation reads 100%. This is a stand-in; a real metric (RMS of
 * a high-passed signal, FFT band energy, ...) can replace it later. */
static double vibration_pct(float xg, float yg, float zg)
{
    double mag = sqrt((double)xg * xg + (double)yg * yg + (double)zg * zg);
    double dev = fabs(mag - 1000.0); /* mg away from 1 g rest */
    double pct = dev / 10.0; /* 1000 mg -> 100% */
    if (pct < 0.0)
        pct = 0.0;
    if (pct > 100.0)
        pct = 100.0;
    return pct;
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

/* Push a value onto the ExternalSensor's Value property. Failure is non-fatal.
 * Mirrors qtpy-uart-bridge: an idle sd-bus connection can be dropped by the
 * broker, so (re)connect if the handle is gone and retry once on a failed
 * write. If the bus never comes up the daemon still logs + writes /run JSON. */
static void publish_value(const char *path, double v)
{
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!bus || sd_bus_is_open(bus) <= 0) {
            if (connect_bus() < 0) {
                static time_t last_err = 0;
                time_t now = time(NULL);
                if (now != last_err) {
                    last_err = now;
                    fprintf(stderr, "iis2dulpx: cannot open system bus\n");
                }
                return;
            }
        }
        sd_bus_error err = SD_BUS_ERROR_NULL;
        int r = sd_bus_set_property(bus, SENSOR_SERVICE, path, SENSOR_IFACE, "Value", &err, "d", v);
        sd_bus_error_free(&err);
        if (r >= 0)
            return;

        sd_bus_flush_close_unref(bus);
        bus = NULL;
        if (attempt == 1) {
            static time_t last_err = 0;
            time_t now = time(NULL);
            if (now != last_err) {
                last_err = now;
                fprintf(stderr, "iis2dulpx: set %s Value failed: %s\n", path, strerror(-r));
            }
        }
    }
}

int main(int argc, char **argv)
{
    const char *dev = (argc > 1) ? argv[1] : getenv("IIS2DULPX_SPIDEV");
    if (!dev || !*dev)
        dev = default_dev;
    const char *vp = getenv("IIS2DULPX_VIB");
    if (vp && *vp)
        vib_path = vp;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Open the system bus for ExternalSensor updates (non-fatal: publish_value
     * reconnects on demand, so a failure here just delays the first publish). */
    if (connect_bus() < 0)
        fprintf(stderr, "iis2dulpx: system bus not up yet; will retry on "
                        "publish, /run JSON still works\n");

    char chosen[300] = { 0 };
    int fd = open_spidev(dev, chosen, sizeof(chosen));
    if (fd < 0) {
        fprintf(stderr, "iis2dulpx: no spidev node (tried %s): %s\n", dev, strerror(errno));
        return 1;
    }
    printf("iis2dulpx: using %s\n", chosen);

    if (configure_spi(fd) < 0) {
        fprintf(stderr, "iis2dulpx: SPI config failed: %s\n", strerror(errno));
        return 1;
    }

    /* Wait-for-chip state machine. SPI has no hotplug signal, so we poll
     * WHO_AM_I until the accelerometer is present (returns 0x47). This lets the
     * service be enabled at boot and simply "spring to life" when the Altera
     * sensor board's IIS2DULPX (on SPIM0/CS0) is ready, with no manual start
     * and no reboot. With no slave attached MISO floats and WHO_AM_I reads
     * 0x00/0xff, which we treat as "not yet". */
    printf("iis2dulpx: waiting for IIS2DULPX (WHO_AM_I=0x%02x) on %s...\n", WHO_AM_I_VAL, chosen);

    int detected = 0;
    unsigned tick = 0;
    unsigned wait_log = 0;
    while (running) {
        if (!detected) {
            uint8_t whoami = 0;
            /* Wake the part from deep power-down first. Until SOFT_PD is
             * written the IIS2DULPX does not drive SDO, so WHO_AM_I reads as
             * 0xff and detection never completes. The write is harmless if the
             * device is already awake, and re-issuing it each poll recovers a
             * part that was unplugged/re-powered. */
            spi_write(fd, REG_EN_DEVICE_CONFIG, EN_DEVICE_CONFIG_SOFT_PD);
            usleep(ACCEL_POWERUP_US);
            if (spi_read(fd, REG_WHO_AM_I, &whoami, 1) == 0 && whoami == WHO_AM_I_VAL) {
                if (configure_accel(fd) == 0) {
                    detected = 1;
                    tick = 0;
                    printf("iis2dulpx: detected (WHO_AM_I=0x%02x); 100 Hz, +-2 g; "
                           "streaming X/Y/Z (mg)\n",
                           whoami);
                } else {
                    fprintf(stderr, "iis2dulpx: configuration failed, retrying\n");
                    sleep(2);
                }
            } else {
                /* Log roughly once a minute (cadence below is 5 s). */
                if (wait_log++ % 12 == 0)
                    printf("iis2dulpx: no IIS2DULPX yet (WHO_AM_I=0x%02x); "
                           "waiting...\n",
                           whoami);
                write_json(0.0f, 0.0f, 0.0f, 0);
                sleep(5);
            }
            continue;
        }

        uint8_t st = 0;
        if (spi_read(fd, REG_STATUS, &st, 1) < 0) {
            fprintf(stderr, "iis2dulpx: SPI read error; rechecking device\n");
            detected = 0;
            continue;
        }
        if (st & STATUS_DRDY) {
            uint8_t raw[6] = { 0 };
            if (spi_read(fd, REG_OUT_X_L, raw, sizeof(raw)) == 0) {
                int16_t rx = (int16_t)((raw[1] << 8) | raw[0]);
                int16_t ry = (int16_t)((raw[3] << 8) | raw[2]);
                int16_t rz = (int16_t)((raw[5] << 8) | raw[4]);
                float xg = rx * SENS_MG_PER_LSB_2G;
                float yg = ry * SENS_MG_PER_LSB_2G;
                float zg = rz * SENS_MG_PER_LSB_2G;
                write_json(xg, yg, zg, 1);
                /* Throttle journal output + ExternalSensor publish to ~1 Hz
                 * (poll runs at ~100 Hz). The entity-manager Fan_Vibration
                 * Timeout (~15 s) marks it unavailable if these stop. */
                if (++tick % 100 == 0) {
                    double vib = vibration_pct(xg, yg, zg);
                    printf("iis2dulpx: X=%.1f Y=%.1f Z=%.1f mg vib=%.0f%%\n", xg, yg, zg, vib);
                    publish_value(vib_path, vib);
                }
            }
        }
        usleep(10000); /* ~100 Hz */
    }

    printf("iis2dulpx: exiting\n");
    write_json(0.0f, 0.0f, 0.0f, 0);
    if (bus)
        sd_bus_unref(bus);
    close(fd);
    return 0;
}
