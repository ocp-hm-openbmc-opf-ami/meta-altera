SUMMARY = "Re-run I3C bus init (RSTDAA/SETDASA/ENTDAA) from Linux for sensor debug"
DESCRIPTION = "Unbinds and rebinds the DesignWare I3C master so the core \
re-enumerates every device, capturing the DAA transactions and any NACK/timeout \
in the kernel log. The userspace equivalent of a 'reset bus + start DAA' button \
for debugging sensors that fail to enumerate (e.g. a TSC1641 that never answers \
DAA), with no reboot or scope. Best paired with CONFIG_DYNAMIC_DEBUG=y."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries.

SRC_URI = "file://i3c-reinit.sh"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${UNPACKDIR}/i3c-reinit.sh ${D}${bindir}/i3c-reinit
}

FILES:${PN} = "${bindir}/i3c-reinit"
