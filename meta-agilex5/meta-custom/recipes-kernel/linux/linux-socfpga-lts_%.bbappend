# Append GSRD specific kernel config fragments and Patches.
FILESEXTRAPATHS:prepend := "${THISDIR}/device-tree:${THISDIR}/linux-socfpga-lts/configs:${THISDIR}/linux-socfpga-lts/patches:${THISDIR}/linux-socfpga-lts/files:"

python () {
    import os

    enable = d.getVar("FPGA_CORE_PGM_ENABLE") or "0"

    #
    # Handle CUSTOM_LINUX_DTS_FILE only if FPGA_CORE_PGM_ENABLE == "1"
    #
    custom_dts = d.getVar("CUSTOM_LINUX_DTS_FILE")

    if enable == "1":
        if not custom_dts:
            bb.fatal("CUSTOM_LINUX_DTS_FILE must be set when FPGA_CORE_PGM_ENABLE = 1")
        # Compute DTB
        dtb = os.path.basename(custom_dts).replace(".dts", ".dtb")
        d.setVar("FPGA_LINUX_DTB_FILE", dtb)
    else:
        # Optional → do NOT fatal, but set DTB_FILE empty so no invalid append happens
        d.setVar("FPGA_LINUX_DTB_FILE", "")

    #
    # LINUX_DTS_FILE is ALWAYS required
    #
    linux_dts = d.getVar("LINUX_DTS_FILE")
    if not linux_dts:
        bb.fatal("LINUX_DTS_FILE must be set (base Linux DTS missing)")

    linux_dtb = os.path.basename(linux_dts).replace(".dts", ".dtb")
    d.setVar("LINUX_DTB_FILE", linux_dtb)
}


#KERNEL_DEVICETREE:append = " intel/${FPGA_LINUX_DTB_FILE}"
KERNEL_DEVICETREE:append = "${@(' intel/%s' % d.getVar('FPGA_LINUX_DTB_FILE')) if d.getVar('FPGA_LINUX_DTB_FILE') else ''}"


#IMAGE_BOOT_FILES:append = " ${FPGA_LINUX_DTB_FILE}"
IMAGE_BOOT_FILES:append = "${@(' %s' % d.getVar('FPGA_LINUX_DTB_FILE')) if d.getVar('FPGA_LINUX_DTB_FILE') else ''}"


SRC_URI:append = " file://agilex5.scc \
		   file://edac.scc \
		   file://initrd.scc \
		   file://jffs2.scc \
		   file://sensors.scc \
		   file://ubifs.scc \
           file://usbedac.scc \
           file://openbmc.scc \
"

# Step 4 (D4) — out-of-tree backport of the ST TSC1641 power-monitor hwmon
# driver for the MikroE Current 12 Click (MIKROE-6065) and the Phase-2 Altera
# sensor board power meter (same chip). The driver only landed upstream in Linux
# v7.0 (drivers/hwmon/tsc1641.c), so it is absent from this socfpga-lts 6.18
# tree. The source is vendored verbatim from torvalds/linux and copied into
# drivers/hwmon at do_patch time (see do_patch:append below), then built
# unconditionally via an obj-y line appended to the hwmon Makefile. It needs
# CONFIG_HWMON=y and CONFIG_REGMAP_I2C (already selected by SENSORS_LM75).
SRC_URI:append = " file://tsc1641.c"

# Auto-add all DTS / DTSI into SRC_URI at PARSE TIME
python __anonymous () {
    import os

    layerdir = d.getVar("LAYERDIR_meta_custom")
    tree = os.path.join(layerdir, "recipes-kernel/linux/device-tree")

    if not os.path.isdir(tree):
        bb.fatal("Device-tree folder not found: %s" % tree)

    for fname in os.listdir(tree):
        if fname.endswith(".dts") or fname.endswith(".dtsi"):
            #bb.note("Adding to SRC_URI at parse time: %s" % fname)
            d.appendVar("SRC_URI", " file://%s" % fname)
}

do_patch:append() {

    # In walnascar, file:// local sources land in ${UNPACKDIR} (sources-unpack/),
    # not directly in ${WORKDIR}.
    DTS_SRC_DIR="${UNPACKDIR}"
    DTS_DST_DIR="${S}/arch/arm64/boot/dts/intel"
    MAKEFILE="${DTS_DST_DIR}/Makefile"

    enable="${FPGA_CORE_PGM_ENABLE}"

    #
    # Step 4 (D4) — install the vendored TSC1641 power-monitor driver into the
    # kernel hwmon tree and force it into the build (built-in). Guard the
    # Makefile edit so repeated do_patch runs stay idempotent.
    #
    HWMON_DST_DIR="${S}/drivers/hwmon"
    if [ -f "${DTS_SRC_DIR}/tsc1641.c" ]; then
        install -m 0644 "${DTS_SRC_DIR}/tsc1641.c" "${HWMON_DST_DIR}/tsc1641.c"
        if ! grep -Fq "tsc1641.o" "${HWMON_DST_DIR}/Makefile"; then
            echo "obj-y += tsc1641.o" >> "${HWMON_DST_DIR}/Makefile"
        fi
    fi

    #
    # If FPGA_CORE_PGM_ENABLE = 1 → must install CUSTOM_LINUX_DTS_FILE
    #
    if [ "${enable}" = "1" ]; then
        if [ ! -f "${DTS_SRC_DIR}/${CUSTOM_LINUX_DTS_FILE}" ]; then
            bbfatal "CUSTOM_LINUX_DTS_FILE ${CUSTOM_LINUX_DTS_FILE} not found in ${DTS_SRC_DIR}"
        fi

        install -m 0644 "${DTS_SRC_DIR}/${CUSTOM_LINUX_DTS_FILE}" "${DTS_DST_DIR}"
    fi

    #
    # Copy all DTSI files into kernel source
    #
    find "${DTS_SRC_DIR}" -type f -name "*.dtsi" | while read DTSI; do
        BASENAME=$(basename "${DTSI}")
        install -m 0644 "${DTSI}" "${DTS_DST_DIR}/${BASENAME}"
    done

    #
    # Add DTB entry to kernel Makefile only when custom DTS is enabled
    #
    if [ "${enable}" = "1" ] && [ -n "${FPGA_LINUX_DTB_FILE}" ]; then
        if ! grep -Fq "${FPGA_LINUX_DTB_FILE}" "${MAKEFILE}"; then
            echo "dtb-\$(CONFIG_ARCH_INTEL_SOCFPGA) += ${FPGA_LINUX_DTB_FILE}" >> "${MAKEFILE}"
        fi
    fi
}


addtask do_patch after do_unpack before do_configure
