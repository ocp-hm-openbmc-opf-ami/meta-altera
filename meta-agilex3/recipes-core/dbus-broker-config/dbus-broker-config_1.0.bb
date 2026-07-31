SUMMARY = "Systemd sandbox overrides for Agilex 3 OpenBMC"
DESCRIPTION = "Installs systemd drop-ins that disable PrivateTmp/ProtectSystem \
on services that require mount namespace kernel support (CONFIG_MNT_NS). \
Covers dbus-broker, systemd-hostnamed, systemd-timedated, and \
xyz.openbmc_project.User.Manager. Required until the kernel built with \
config_openbmc.cfg (which enables CONFIG_MNT_NS) is booted."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd allarch

SRC_URI = " \
    file://openbmc-sandbox.conf \
    file://hostnamed-sandbox.conf \
    file://timedated-sandbox.conf \
    file://user-manager-sandbox.conf \
"

S = "${UNPACKDIR}"

SYSTEMD_SERVICE:${PN} = ""

do_install() {
    # dbus-broker
    install -d ${D}${systemd_system_unitdir}/dbus-broker.service.d
    install -m 0644 ${UNPACKDIR}/openbmc-sandbox.conf \
        ${D}${systemd_system_unitdir}/dbus-broker.service.d/openbmc-sandbox.conf

    # systemd-hostnamed: bmcweb queries this for BMC hostname (Managers/bmc)
    install -d ${D}${systemd_system_unitdir}/systemd-hostnamed.service.d
    install -m 0644 ${UNPACKDIR}/hostnamed-sandbox.conf \
        ${D}${systemd_system_unitdir}/systemd-hostnamed.service.d/openbmc-sandbox.conf

    # systemd-timedated: needed for time sync and Redfish time endpoints
    install -d ${D}${systemd_system_unitdir}/systemd-timedated.service.d
    install -m 0644 ${UNPACKDIR}/timedated-sandbox.conf \
        ${D}${systemd_system_unitdir}/systemd-timedated.service.d/openbmc-sandbox.conf

    # phosphor-user-manager: required for Redfish Roles to be populated
    install -d ${D}${systemd_system_unitdir}/xyz.openbmc_project.User.Manager.service.d
    install -m 0644 ${UNPACKDIR}/user-manager-sandbox.conf \
        ${D}${systemd_system_unitdir}/xyz.openbmc_project.User.Manager.service.d/openbmc-sandbox.conf
}

FILES:${PN} = " \
    ${systemd_system_unitdir}/dbus-broker.service.d/openbmc-sandbox.conf \
    ${systemd_system_unitdir}/systemd-hostnamed.service.d/openbmc-sandbox.conf \
    ${systemd_system_unitdir}/systemd-timedated.service.d/openbmc-sandbox.conf \
    ${systemd_system_unitdir}/xyz.openbmc_project.User.Manager.service.d/openbmc-sandbox.conf \
"
