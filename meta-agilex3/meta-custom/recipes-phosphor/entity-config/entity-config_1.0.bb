SUMMARY = "entity-manager chassis configuration for Agilex 3"
DESCRIPTION = "Provides a static entity-manager configuration that publishes \
a Chassis inventory object on D-Bus so bmcweb can serve the chassis under \
/redfish/v1/Chassis and its Sensors collection. \
The configuration always probes (Probe = TRUE). \
Also loads the axi-fan-control hwmon driver before fansensor starts."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch systemd

# file:// sources unpack directly into ${UNPACKDIR}, not ${UNPACKDIR}/${BP}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = " \
    file://entity-config.json \
    file://axi-fan-control-modprobe.service \
    "

# This image's systemd has no systemd-modules-load.service, so /etc/modules-load.d
# is never processed. Load the fan driver via a dedicated oneshot unit ordered
# before fansensor instead.
SYSTEMD_SERVICE:${PN} = "axi-fan-control-modprobe.service"

do_install() {
    install -d ${D}${datadir}/entity-manager/configurations
    install -m 0644 ${UNPACKDIR}/entity-config.json \
        ${D}${datadir}/entity-manager/configurations/entity-config.json

    # Kept for documentation / future images that do ship systemd-modules-load.
    install -d ${D}${sysconfdir}/modules-load.d
    echo "axi-fan-control" > ${D}${sysconfdir}/modules-load.d/axi-fan-control.conf

    # Oneshot unit that modprobes axi-fan-control before fansensor scans.
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/axi-fan-control-modprobe.service \
        ${D}${systemd_system_unitdir}/axi-fan-control-modprobe.service
}

FILES:${PN} = " \
    ${datadir}/entity-manager/configurations \
    ${sysconfdir}/modules-load.d/axi-fan-control.conf \
    ${systemd_system_unitdir}/axi-fan-control-modprobe.service \
    "

RDEPENDS:${PN} = "entity-manager"
