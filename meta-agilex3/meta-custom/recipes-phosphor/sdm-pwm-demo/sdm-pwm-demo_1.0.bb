SUMMARY = "DEMO-only open-loop SDM-temp -> fan PWM (pre-hardware bring-up)"
DESCRIPTION = "Temporary demo helper that maps an SDM on-die temperature to the \
axi_fan_control PWM so the temperature -> PWM path can be shown on the dashboard \
before the MikroE Thermo 10 sensor and Geekworm X-FAN40 arrive. This is NOT the \
production control path (swampd/phosphor-pid-control); the service Conflicts with \
swampd and is disabled by default. Remove this recipe to revert."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch systemd

# file:// sources unpack directly into ${UNPACKDIR}, not ${UNPACKDIR}/${BP}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://sdm-pwm-demo.sh \
    file://sdm-pwm-demo.service \
    "

SYSTEMD_SERVICE:${PN} = "sdm-pwm-demo.service"
# Disabled by default: this is a manual, demo-only override of swampd. Start it
# with `systemctl start sdm-pwm-demo` (which stops swampd via Conflicts=).
SYSTEMD_AUTO_ENABLE:${PN} = "disable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${UNPACKDIR}/sdm-pwm-demo.sh ${D}${bindir}/sdm-pwm-demo.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/sdm-pwm-demo.service \
        ${D}${systemd_system_unitdir}/sdm-pwm-demo.service
}

FILES:${PN} = " \
    ${bindir}/sdm-pwm-demo.sh \
    ${systemd_system_unitdir}/sdm-pwm-demo.service \
    "
