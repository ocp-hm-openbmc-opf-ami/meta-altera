# phosphor-ipmi-host is already fully built by default (meta-common only keeps
# a comment here, it does not stub any tasks). Just remove the circular
# RRECOMMENDS that would pull in phosphor-settings-manager.
RRECOMMENDS:${PN}:remove = "phosphor-settings-manager"

# Fix two ipmid issues on agilex3:
#
# 1. Startup SEGV — get-dbus-active-software (meson default: auto=enabled)
#    ipmid calls getActiveSoftwareVersionInfo() on every Get-Device-ID request.
#    getAllDbusObjects() for xyz.openbmc_project.Software.RedundancyPriority
#    returns EINVAL (no objects) because phosphor-software-manager is absent.
#    elog<InternalFailure>() causes SIGSEGV. Disable the feature so ipmid
#    reads the BMC firmware version from the static dev_id.json instead.
#
# 2. Enable dynamic sensor reading (ipmitool sensor)
#    The default dynamic-sensors=disabled uses a static YAML-based sensor map.
#    The agilex3 sensor.yaml is 0 bytes (no static sensors), causing the static
#    Get-SDR handler to crash. Enabling dynamic-sensors builds libdynamiccmds.so
#    which reads sensor objects from dbus-sensors at runtime.
#    dbus-sdr/storagecommands.cpp uses the old sdbusplus::object_path API (now
#    sdbusplus::message::object_path). Fix all four occurrences with sed before
#    meson configure so the file compiles cleanly.
#
# 3. StartLimitIntervalSec in [Service] (AMI service file) generates a systemd
#    warning in walnascar. Drop-in 20-fix-startlimit.conf moves it to [Unit].
#
do_configure:prepend() {
    sed -i 's/sdbusplus::object_path/sdbusplus::message::object_path/g' \
        ${S}/dbus-sdr/storagecommands.cpp
}

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://20-fix-startlimit.conf \
    "

do_install:append() {
    install -d ${D}${systemd_system_unitdir}/phosphor-ipmi-host.service.d
    install -m 0644 ${UNPACKDIR}/20-fix-startlimit.conf \
        ${D}${systemd_system_unitdir}/phosphor-ipmi-host.service.d/
}

FILES:${PN}:append = " ${systemd_system_unitdir}/phosphor-ipmi-host.service.d/20-fix-startlimit.conf"

EXTRA_OEMESON:append = " \
    -Dget-dbus-active-software=disabled \
    -Ddynamic-sensors=enabled \
    "