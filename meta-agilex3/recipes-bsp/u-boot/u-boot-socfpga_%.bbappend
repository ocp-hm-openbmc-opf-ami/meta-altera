SUMMARY = "U-Boot customization for Altera SoCFPGA OpenBMC"
DESCRIPTION = "Patches U-Boot DTS to set u-boot,spl-boot-order based on \
SSBL_BOOT_SOURCE (mmc0 / qspi / nand). Conditionally disables the FAT \
environment for QSPI-only boot."

# Path to optional U-Boot device tree source files
UBOOT_DEVICE_TREE_SRC_PATH := "${THISDIR}/files/dts"

# Prepend config fragment and patch directories
FILESEXTRAPATHS:prepend := "${THISDIR}/files/configs:${THISDIR}/files/patches:"

# For QSPI-only boot, disable the FAT environment so U-Boot does not try
# to read/write uboot.env from an SD card FAT partition that isn't there.
SRC_URI += "${@bb.utils.contains('SSBL_BOOT_SRC', 'qspi', 'file://disable-fat-env.cfg', '', d)}"

python () {
    import bb

    dtb = d.getVar("UBOOT_DEVICE_TREE")
    if not dtb:
        bb.fatal("UBOOT_DEVICE_TREE is not set. Define it in the machine conf \
(e.g., socfpga_agilex3_socdk.dtb).")

    base = dtb.replace(".dtb", "") if dtb.endswith(".dtb") else dtb
    dtsi_file = f"{d.getVar('S')}/arch/arm/dts/{base}-u-boot.dtsi"
    d.setVar("UBOOT_DTSI_FILE", dtsi_file)
}

# Patch SPL boot order in the U-Boot DTSI based on SSBL_BOOT_SOURCE
do_configure:append() {
    if [ "${SSBL_BOOT_SRC}" = "mmc0" ]; then
        sed -i 's/^\(\s*u-boot,spl-boot-order\s*=\s*\).*$/\1\&mmc,\&flash0,"\/memory";/' "${UBOOT_DTSI_FILE}"
    elif [ "${SSBL_BOOT_SRC}" = "qspi" ]; then
        sed -i 's/^\(\s*u-boot,spl-boot-order\s*=\s*\).*$/\1\&flash0,"\/memory";/' "${UBOOT_DTSI_FILE}"
    elif [ "${SSBL_BOOT_SRC}" = "nand" ]; then
        sed -i 's/^\(\s*u-boot,spl-boot-order\s*=\s*\).*$/\1\&nand,\&flash0,"\/memory";/' "${UBOOT_DTSI_FILE}"
    fi
}
