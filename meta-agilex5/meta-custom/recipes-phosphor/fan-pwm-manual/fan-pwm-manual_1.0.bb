SUMMARY = "Manual fan PWM bring-up helper (pause swampd, hold a fixed duty, restore)"
DESCRIPTION = "CLI debug aid for hardware bring-up. Stops swampd so a \
fixed fan PWM duty can be held on the axi_fan_control hwmon (or written straight to \
the FPGA PWM register via devmem) to measure the host_fpwm pin (AG21 / Pi header pin \
33), then restarts swampd for temperature-driven control. Not a daemon and makes no \
persistent change."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch

# file:// sources unpack directly into ${UNPACKDIR}, not ${UNPACKDIR}/${BP}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = "file://fan-pwm-manual"

# systemctl (start/stop swampd) comes from systemd; devmem is provided by busybox.
RDEPENDS:${PN} += "systemd"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${UNPACKDIR}/fan-pwm-manual ${D}${bindir}/fan-pwm-manual
}

FILES:${PN} = "${bindir}/fan-pwm-manual"
