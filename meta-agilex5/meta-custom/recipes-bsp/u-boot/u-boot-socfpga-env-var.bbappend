SUMMARY = "U-boot boot environment for Altera SoCFPGA devices"
DESCRIPTION = "Sets the boot source for Second stage boot loader"
LICENSE = "MIT"

do_configure() {
    if [ -n "${SSBL_BOOT_SRC}" ]; then
        sed -i -e "s/boot_targets=.*/boot_targets=${SSBL_BOOT_SRC}/" ${S}/u-boot-env.txt
    else
        bbwarn "SSBL_BOOT_SRC is empty!, configure the boot source.."
    fi

    if [ -n "${KERNEL_BOOTARGS}" ]; then
        sed -i -e "s|^custom_bootargs=.*|custom_bootargs=${KERNEL_BOOTARGS}|" ${S}/u-boot-env.txt
    fi

    # Override MTD partition sizes for 512Mb (64MB) QSPI flash
    # Change from default 66m(u-boot),190m(root) to 12m(u-boot),52m(root)
    sed -i -e "s/66m(u-boot),190m(root)/12m(u-boot),52m(root)/" ${S}/u-boot-env.txt
    sed -i -e "s/66m(qspi_uboot),190m(qspi_root)/12m(qspi_uboot),52m(qspi_root)/" ${S}/u-boot-env.txt

    # Pin the BMC NIC MAC so U-Boot stops generating a fresh random MAC each
    # boot. The port enumerates as eth2 (ethernet2 = &gmac2), so U-Boot reads
    # its address from eth2addr; with no MAC env it randomizes and fixes up the
    # kernel FDT with that random value, so the DHCP server treats every reboot
    # as a new client and hands out a different IP. ethaddr is set too as a
    # harmless fallback (there is no eth0). Keep in sync with the DTS gmac2
    # mac-address and the systemd .network ClientIdentifier=mac.
    if ! grep -q '^eth2addr=' ${S}/u-boot-env.txt; then
        printf 'eth2addr=02:00:00:a5:01:3b\nethaddr=02:00:00:a5:01:3b\n' >> ${S}/u-boot-env.txt
    fi
}
