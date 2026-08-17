SUMMARY = "TSC1641 (VH/VFAN) I3C power monitors to D-Bus bridge"
DESCRIPTION = "The altera sensor board's two ST TSC1641 on the I3C bus enumerate \
natively as I3C devices (the DesignWare I3C master cannot mix legacy-I2C and \
native-I3C on one bus) and so appear only as hwmon nodes, not on a legacy i2c \
bus that dbus-sensors can instantiate. This daemon reads both tsc1641 hwmons \
(told apart by their I3C address: VH/heater rail and VFAN/fan rail) and \
publishes each (voltage, current, power, die temperature) onto its dbus-sensors \
ExternalSensor (Heater_*/Fan_*) so the rail readings reach Redfish."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# libsystemd (sd-bus) is used to push values onto the ExternalSensors.
DEPENDS = "systemd"

inherit systemd

# file:// sources unpack directly into ${UNPACKDIR}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://tsc1641-current-bridge.c \
    file://tsc1641-current-bridge.service \
    "

SYSTEMD_SERVICE:${PN} = "tsc1641-current-bridge.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o tsc1641-current-bridge \
        ${UNPACKDIR}/tsc1641-current-bridge.c -lsystemd
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 tsc1641-current-bridge ${D}${bindir}/tsc1641-current-bridge

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/tsc1641-current-bridge.service \
        ${D}${systemd_system_unitdir}/tsc1641-current-bridge.service
}

FILES:${PN} = " \
    ${bindir}/tsc1641-current-bridge \
    ${systemd_system_unitdir}/tsc1641-current-bridge.service \
    "
