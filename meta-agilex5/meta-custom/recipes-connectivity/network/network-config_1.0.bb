SUMMARY = "Agilex 5E 013B BMC network configuration"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# file:// sources unpack directly into ${UNPACKDIR}, not ${UNPACKDIR}/${BP}
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = "file://00-bmc-eth0.network"

do_install() {
    # Install to /etc/systemd/network/ not /lib/systemd/network/.
    #
    # /lib/ is shadowed at runtime: 60-phosphor-networkd-default.network
    # (installed there by the phosphor-network package) matches all ethernet
    # interfaces and wins over anything without a lower numeric prefix.
    #
    # /etc/ takes precedence over /lib/ for all prefixes, so our file wins
    # immediately. The 00-bmc- prefix also matches the naming convention that
    # phosphor-networkd uses when it writes its own runtime config, so it
    # reads this file on startup rather than generating one from defaults.
    install -d ${D}${sysconfdir}/systemd/network/
    install -m 0644 ${UNPACKDIR}/00-bmc-eth0.network \
        ${D}${sysconfdir}/systemd/network/
}

FILES:${PN} = "${sysconfdir}/systemd/network/"
