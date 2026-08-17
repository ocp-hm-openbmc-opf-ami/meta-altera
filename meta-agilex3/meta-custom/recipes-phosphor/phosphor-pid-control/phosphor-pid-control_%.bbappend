FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Board-specific swampd configuration for the Agilex 5E 013B (Step 1 / D1).
#
# swampd reads /usr/share/swampd/config.json by default; when that file exists
# it is used in preference to any entity-manager (D-Bus) Pid config. This static
# file is intentionally transparent so it can be tuned live on the board during
# bring-up (edit + `systemctl restart phosphor-pid-control`).
#
# Open loop: Board_Temp (MikroE Thermo 10 / P3T1755) --stepwise--> PWM% which is
# written straight to pwm1 on the ADI axi-fan-control IP (drives the Geekworm
# X-FAN40). The Fan_Ctrl "fan" PID is a pure feed-forward pass-through
# (proportionalCoeff/integralCoeff = 0, feedFwdGainCoeff = 1), so the temperature-
# derived percent becomes the output and the fan tach is displayed but never fed
# back into the control output. See spec D1 / D4.4.
#
# WHY open loop and not RPM regulation: a true closed RPM loop oscillated
# 0<->255 bang-bang on this fan. Two causes: (a) the default gains were ~10-50x
# too hot for an error measured in thousands of RPM with a 0.1s sample, while the
# fan needs ~1-2s to physically respond, and (b) the tach measurement window
# cannot track a rapidly slewing PWM, so the feedback was stale. Driving PWM
# directly from temperature removes the inner feedback loop entirely so it cannot
# oscillate; thermal feedback (hotter board -> faster fan) is preserved. The
# fan + tach are good when the duty is held steady: measured 0%/50%/100% PWM ->
# 0/2448/5372 RPM, monotonic.
#
# Board_Temp uses "timeout": 0 to DISABLE swampd's staleness watchdog. Board_Temp
# is a dbus-sensors ExternalSensor pushed ~1 Hz by p3t1755-temp-bridge. When the
# board temperature is steady the bridge writes the same value, and sdbusplus
# does NOT emit PropertiesChanged for an unchanged property, so swampd's "updated"
# timestamp stops advancing. With the default 2s temp timeout (sensor.hpp
# getDefaultTimeout) swampd then declares "Sensor timeout" -> failsafe (pwm 100)
# every ~2s, flapping recovered<->timed-out. timeout:0 takes the zone.hpp
# "timeout != 0 && duration >= period" branch out of play. The safety net is
# intact: the ExternalSensor's own Timeout:30 (entity-manager) still marks it
# unavailable if the bridge actually dies (swampd getFailed -> sensor missing ->
# failsafe).
#
# writePath anchors to the axi-fan-control platform device and uses swampd's "**"
# glob to pick its sole hwmon child, so it is independent of the
# /sys/class/hwmon/hwmonN index:
#   /sys/devices/platform/soc@0/20000000.axi-fan-control/hwmon/**/pwm1
# The hwmonN index is NOT stable: with the Thermo 10 connected, p3t1755 enumerates
# first and takes hwmon0, pushing soc64hwmon -> hwmon1 and axi-fan-control ->
# hwmon2, so a hardcoded hwmon1/pwm1 writes to the wrong device (soc64hwmon) and
# the fan is never driven. The base before "**" must be the REAL nested path under
# soc@0; the flat /sys/devices/platform/20000000.axi-fan-control/... dir does not
# exist, so swampd's FixupPath (sysfs/util.cpp) throws on fs::directory_iterator
# and dead-loops in restartControlLoops. Confirm with:
#   readlink -f /sys/class/hwmon/hwmon*/device | grep axi-fan-control
#   ls -d /sys/devices/platform/soc@0/*axi-fan-control*/hwmon/hwmon*/pwm1
#
# TUNE: the only fan knob now is the stepwise reading/output table (temp -> PWM%).
# Output values are PWM percent (0-100); minThermalOutput 40 keeps a steady idle
# (~40% -> ~1900 RPM) so the fan never stalls; failsafePercent 100 -> full speed
# if Board_Temp drops off D-Bus. Verified on hardware: idle holds at pwm 40 with
# no flapping. TODO when a heat source is available: confirm the 40->60->80->100
# ramp as Board_Temp crosses 35/40/45 C.
# The upstream recipe sets SYSTEMD_SERVICE = phosphor-pid-control.service but
# (by design) does not ship the unit — each machine must provide it. Without
# it the build fails the systemd QA check and swampd never starts. Ship ours.
#
# handle-missing-object-paths makes swampd tolerate a configured sensor object
# (Board_Temp) not yet existing on D-Bus — true before the Thermo 10 is plugged
# in, when swampd should simply run the zone in fail-safe instead of erroring.
PACKAGECONFIG:append = " handle-missing-object-paths"

# Hostless BMC: there is no host IPMI, so phosphor-ipmi-host is stubbed and
# libipmid is not in the sysroot/rootfs. Stock phosphor-pid-control hard-requires
# libipmid and builds the libmanualcmds IPMI OEM provider, which breaks
# do_configure (meson can't find libipmid -> disabled wrap download). The 0001
# patch builds swampd only and drops the IPMI provider (swampd itself uses no
# ipmid symbols). Drop the build dep and the IPMI provider-symlink wiring to match.
DEPENDS:remove = "phosphor-ipmi-host"
HOSTIPMI_PROVIDER_LIBRARY = ""

SRC_URI:append = " file://config.json \
                  file://phosphor-pid-control.service"

do_configure:prepend() {
    # Hostless build: remove libipmid dependency and the manualcmds provider.
    sed -i \
        -e "/^ipmid_dep = dependency('libipmid')/d" \
        -e "/^[[:space:]]*ipmid_dep,[[:space:]]*$/d" \
        ${S}/meson.build

    sed -i \
        -e "/^libmanualcmds_sources = \[/,/^executable(/ { /^executable(/!d; }" \
        ${S}/meson.build
}

do_install:append() {
    install -d ${D}${datadir}/swampd
    install -m 0644 ${UNPACKDIR}/config.json ${D}${datadir}/swampd/config.json

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/phosphor-pid-control.service \
        ${D}${systemd_system_unitdir}/phosphor-pid-control.service
}

FILES:${PN}:append = " ${datadir}/swampd ${datadir}/swampd/config.json"
