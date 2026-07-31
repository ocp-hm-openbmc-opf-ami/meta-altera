SUMMARY = "Mask Phase 0/1 unsupported OpenBMC services for Agilex 3"
DESCRIPTION = "Creates systemd mask symlinks for services that have no hardware \
support yet. Prevents boot-time spam from services that would otherwise \
retry continuously. Each service can be unmasked in a later phase when the \
corresponding hardware or configuration is added."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch

# Phase 1 masks:
#   obmc-ikvm         — No KVM/video hardware on this board.
#   trace-enable      — Boot loader trace events not applicable here.
#   sysfs-led@*       — BSP-level HPS LED sysfs entries not used here.
MASKED_SERVICES = " \
    obmc-ikvm.service \
    trace-enable.service \
    sysfs-led@hps_led0.service \
    sysfs-led@hps_led1.service \
"

do_install() {
    # Install masks to /etc/systemd/system/ (higher priority than /usr/lib/systemd/system/)
    # so they override installed service files without conflicting with their package.
    install -d ${D}${sysconfdir}/systemd/system
    for svc in ${MASKED_SERVICES}; do
        ln -sf /dev/null ${D}${sysconfdir}/systemd/system/${svc}
    done
}

FILES:${PN} = "${sysconfdir}/systemd/system"
