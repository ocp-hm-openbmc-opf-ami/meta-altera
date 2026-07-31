SUMMARY = "ST IIS2DULPX accelerometer SPI reader (bring-up/demo)"
DESCRIPTION = "Minimal userspace daemon that reads the ST IIS2DULPX 3-axis \
accelerometer (Altera sensor board) over /dev/spidev0.0 \
(HPS SPIM0 routed through the FPGA fabric in Shi Lin's GHRD). The chip has no \
in-tree kernel driver, so it is exposed via spidev (DT child bound through the \
allow-listed rohm,dh2228fv compatible) and driven directly here. Verifies \
WHO_AM_I, configures 100 Hz / +-2 g, then logs X/Y/Z (mg) to the journal and \
publishes /run/iis2dulpx/accel.json. Demo-only; disabled by default."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# libsystemd (sd-bus) mirrors the vibration % onto the Fan_Vibration ExternalSensor.
DEPENDS = "systemd"

inherit systemd

# file:// sources unpack directly into ${UNPACKDIR}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://iis2dulpx-accel.c \
    file://iis2dulpx-accel.service \
    "

SYSTEMD_SERVICE:${PN} = "iis2dulpx-accel.service"
# Enabled by default: the daemon polls WHO_AM_I and idles harmlessly until the
# Altera sensor board's IIS2DULPX (on SPIM0/CS0) is ready, then auto-starts
# streaming. No manual start or reboot needed.
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o iis2dulpx-accel \
        ${UNPACKDIR}/iis2dulpx-accel.c -lsystemd -lm
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 iis2dulpx-accel ${D}${bindir}/iis2dulpx-accel

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/iis2dulpx-accel.service \
        ${D}${systemd_system_unitdir}/iis2dulpx-accel.service
}

FILES:${PN} = " \
    ${bindir}/iis2dulpx-accel \
    ${systemd_system_unitdir}/iis2dulpx-accel.service \
    "
