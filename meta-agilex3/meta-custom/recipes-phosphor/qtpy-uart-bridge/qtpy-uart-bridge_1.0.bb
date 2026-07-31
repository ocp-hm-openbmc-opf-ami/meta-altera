SUMMARY = "QT Py UART telemetry bridge for the Altera sensor board (bring-up/demo)"
DESCRIPTION = "Reads line-based ASCII telemetry (KEY:VALUE tokens; SLIDERS is the \
required potentiometer value, a raw 16-bit ADC count streamed ~10 Hz) from the \
Altera sensor board's QT Py MCU over the Pi-header UART (/dev/ttyS1 = HPS UART1, \
FPGA pin-muxed to AF24/AG24). Scales it to 0-100% and mirrors it onto the Slider \
ExternalSensor (humidity namespace) so it appears on the Redfish dashboard, and \
also publishes /run/qtpy/heater.json. Tolerant parser so the QT Py line format \
can evolve; see qtpy-uart-protocol.md. Test now by driving /dev/ttyS1 from a \
Raspberry Pi."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# libsystemd (sd-bus) is used to push the slider onto the ExternalSensor.
DEPENDS = "systemd"

inherit systemd

# file:// sources unpack directly into ${UNPACKDIR}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://qtpy-uart-bridge.c \
    file://qtpy-uart-bridge.service \
    "

SYSTEMD_SERVICE:${PN} = "qtpy-uart-bridge.service"
# Enabled by default: it blocks harmlessly on /dev/ttyS1 until bytes arrive, so
# it is safe to ship before the sensor board exists (drive ttyS1 from a Pi to
# emulate the QT Py).
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o qtpy-uart-bridge \
        ${UNPACKDIR}/qtpy-uart-bridge.c -lsystemd
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 qtpy-uart-bridge ${D}${bindir}/qtpy-uart-bridge

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/qtpy-uart-bridge.service \
        ${D}${systemd_system_unitdir}/qtpy-uart-bridge.service
}

FILES:${PN} = " \
    ${bindir}/qtpy-uart-bridge \
    ${systemd_system_unitdir}/qtpy-uart-bridge.service \
    "
