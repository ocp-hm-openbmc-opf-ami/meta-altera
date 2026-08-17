SUMMARY = "P3T1755 (Thermo 10 Click) I3C temperature to D-Bus bridge"
DESCRIPTION = "The NXP P3T1755 on the Agilex 5 I3C bus enumerates natively as an \
I3C device (lm75_i3c) and so appears only as a hwmon node, not on a legacy i2c \
bus that dbus-sensors can instantiate. This daemon reads the p3t1755 hwmon \
temp1_input and publishes it onto the dbus-sensors ExternalSensor 'Board_Temp' \
(Units=DegreesC -> /xyz/openbmc_project/sensors/temperature/Board_Temp), the \
same path phosphor-pid-control consumes, so the temperature reaches Redfish and \
feeds the temperature-driven fan control."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# libsystemd (sd-bus) is used to push the value onto the ExternalSensor.
DEPENDS = "systemd"

inherit systemd

# file:// sources unpack directly into ${UNPACKDIR}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://p3t1755-temp-bridge.c \
    file://p3t1755-temp-bridge.service \
    "

SYSTEMD_SERVICE:${PN} = "p3t1755-temp-bridge.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o p3t1755-temp-bridge \
        ${UNPACKDIR}/p3t1755-temp-bridge.c -lsystemd
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 p3t1755-temp-bridge ${D}${bindir}/p3t1755-temp-bridge

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/p3t1755-temp-bridge.service \
        ${D}${systemd_system_unitdir}/p3t1755-temp-bridge.service
}

FILES:${PN} = " \
    ${bindir}/p3t1755-temp-bridge \
    ${systemd_system_unitdir}/p3t1755-temp-bridge.service \
    "
