SUMMARY = "Default FRU for Agilex5"
DESCRIPTION = "Installs a default FRU blob required by OneTree FRU packagegroups"
LICENSE = "CLOSED"

SRC_URI = "file://baseboard.fru.bin"
S = "${UNPACKDIR}"

# BitBake in this environment rejects variable-containing cleandirs entries for
# this task. Keep unpack cleanup empty for this local file:// recipe.
do_unpack[cleandirs] = ""
base_do_unpack[cleandirs] = ""

do_install() {
    install -d ${D}${sysconfdir}/fru
    install -m 0644 ${UNPACKDIR}/baseboard.fru.bin ${D}${sysconfdir}/fru/baseboard.fru.bin
}

FILES:${PN} += "${sysconfdir}/fru/baseboard.fru.bin"
