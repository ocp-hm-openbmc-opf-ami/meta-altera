SUMMARY = "U-Boot boot environment for Altera SoCFPGA OpenBMC"
DESCRIPTION = "Configures the U-Boot environment: sets boot_targets from \
SSBL_BOOT_SOURCE, overrides MTD partition sizes for 64 MB QSPI flash, and \
optionally sets custom_bootargs from KERNEL_BOOTARGS. \
Board-specific overrides (MAC pinning, etc.) are applied by the board sublayer."

do_configure:append() {
    # Set boot target (mmc0 / qspi / nand)
    if [ -n "${SSBL_BOOT_SRC}" ]; then
        sed -i -e "s/boot_targets=.*/boot_targets=${SSBL_BOOT_SRC}/" ${S}/u-boot-env.txt
    else
        bbwarn "SSBL_BOOT_SRC is empty — configure the boot source."
    fi

    # Set optional kernel command-line args
    if [ -n "${KERNEL_BOOTARGS}" ]; then
        sed -i -e "s|^custom_bootargs=.*|custom_bootargs=${KERNEL_BOOTARGS}|" ${S}/u-boot-env.txt
    fi

    # Override MTD partition sizes for 512Mb (64 MB) QSPI flash.
    # Default layout targets 256 MB; override to 12 MB u-boot + 52 MB root.
    sed -i -e "s/66m(u-boot),190m(root)/12m(u-boot),52m(root)/" ${S}/u-boot-env.txt
    sed -i -e "s/66m(qspi_uboot),190m(qspi_root)/12m(qspi_uboot),52m(qspi_root)/" ${S}/u-boot-env.txt
}
