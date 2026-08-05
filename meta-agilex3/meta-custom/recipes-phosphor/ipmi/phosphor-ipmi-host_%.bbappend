RRECOMMENDS:${PN}:remove = "phosphor-settings-manager"

# Fix ipmid issues on agilex3:
#
# 1. Startup SEGV — get-dbus-active-software (meson default: auto=enabled)
# 2. Enable dynamic sensor reading (ipmitool sensor)
#    dbus-sdr/storagecommands.cpp uses the old sdbusplus::object_path API.
# 3. StartLimitIntervalSec in [Service] (AMI service file) systemd warning fix.
# 4. Add humidity/percent to sensorTypes map so PercentRH sensors show as
#    threshold (numeric percentage) instead of discrete in ipmitool.
# 5. Set IPMI percentage bit (sensor_units_1 bit 0) for humidity sensors so
#    ipmitool shows "percent" instead of "unspecified" for Fan_Vibration/Slider.
#
do_configure:prepend() {
    sed -i 's/sdbusplus::object_path/sdbusplus::message::object_path/g' \
        ${S}/dbus-sdr/storagecommands.cpp
    # humidity/percent missing from sensorTypes → eventReadingType=0 → discrete in ipmitool
    sed -i 's/SensorEventTypeCodes::threshold)}}};/SensorEventTypeCodes::threshold)}, {"humidity", std::make_pair(SensorTypeCodes::other, SensorEventTypeCodes::threshold)}, {"percent", std::make_pair(SensorTypeCodes::other, SensorEventTypeCodes::threshold)}}};/' \
        ${S}/dbus-sdr/sdrutils.cpp
    # set IPMI percentage bit in sensor_units_1 for humidity (PercentRH) sensors
    sed -i 's/record\.body\.sensor_units_1 = (bSigned ? 1 : 0) << 7;/record.body.sensor_units_1 = ((bSigned ? 1 : 0) << 7) | (type == "humidity" ? 1 : 0);/' \
        ${S}/dbus-sdr/sensorcommands.cpp
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