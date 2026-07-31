SUMMARY = "Set root Redfish privilege on first boot"
DESCRIPTION = "Oneshot systemd service that sets the root user's OpenBMC \
privilege level to priv-admin after phosphor-user-manager starts. Without \
this the Roles field in Redfish session responses is empty and the web UI \
login silently fails after a clean flash."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd allarch

SRC_URI = " \
    file://first-boot-set-priv.sh \
    file://first-boot-set-priv.service \
"

S = "${UNPACKDIR}"

SYSTEMD_SERVICE:${PN} = "first-boot-set-priv.service"

do_install() {
    install -d ${D}${libexecdir}
    install -m 0755 ${UNPACKDIR}/first-boot-set-priv.sh \
        ${D}${libexecdir}/first-boot-set-priv.sh

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/first-boot-set-priv.service \
        ${D}${systemd_system_unitdir}/first-boot-set-priv.service
}

FILES:${PN} = " \
    ${libexecdir}/first-boot-set-priv.sh \
    ${systemd_system_unitdir}/first-boot-set-priv.service \
"

RDEPENDS:${PN} = "systemd"
