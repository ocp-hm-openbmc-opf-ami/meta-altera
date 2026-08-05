write_flash_size_to_file_meta_custom() {
    flash_size_kb="${FLASH_SIZE}"

    if [ -z "${flash_size_kb}" ]; then
        bbnote "FLASH_SIZE is unset; skipping FWSize generation"
        return 0
    fi

    case "${flash_size_kb}" in
        ''|*[!0-9]*)
            bbwarn "FLASH_SIZE='${flash_size_kb}' is not numeric; skipping FWSize generation"
            return 0
            ;;
    esac

    image_size_bytes=$(expr "${flash_size_kb}" \* 1024)
    image_size_hex=$(printf "0x%x" "${image_size_bytes}")
    fw_size_file="${IMAGE_ROOTFS}/etc/FWSize"

    if [ ! -d "${IMAGE_ROOTFS}/etc" ]; then
        mkdir -p "${IMAGE_ROOTFS}/etc"
    fi

    echo "${image_size_hex}" > "${fw_size_file}"
}

# Disable systemd's predictable network interface naming
# This prevents "end2" and uses traditional "eth0", "eth1", "eth2" names
mask_predictable_naming() {
    if [ ! -d "${IMAGE_ROOTFS}/lib/systemd/network" ]; then
        mkdir -p "${IMAGE_ROOTFS}/lib/systemd/network"
    fi
    ln -sf /dev/null "${IMAGE_ROOTFS}/lib/systemd/network/99-default.link"
}

ROOTFS_POSTPROCESS_COMMAND:remove = "write_flash_size_to_file; "
ROOTFS_POSTPROCESS_COMMAND += "write_flash_size_to_file_meta_custom; "
ROOTFS_POSTPROCESS_COMMAND += "mask_predictable_naming; "

IMAGE_INSTALL:append = " \
    phosphor-ipmi-host \
    intel-ipmi-oem \
    ipmitool \
    bmcweb \
    phosphor-user-manager \
    session-management \
    tzdata \
    phosphor-certificate-manager \
    webui-vue \
    srvcfg-manager \
    entity-manager \
    entity-config \
    dbus-sensors \
    p3t1755-temp-bridge \
    tsc1641-current-bridge \
    iis2dulpx-accel \
    qtpy-uart-bridge \
    "