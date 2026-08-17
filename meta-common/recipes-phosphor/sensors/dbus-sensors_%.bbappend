FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"

# Support the soc64-hwmon platform device in hwmontempsensor.
# soc64-hwmon exposes Agilex 3/5 SDM temperature sensors via ATF SMC calls.
# It registers as a platform (not I2C) device, so the standard
# getDeviceBusAddr() path fails. This patch falls back to reading the hwmon
# "name" file and, when it reads "soc64hwmon", assigns sentinel Bus=65535 /
# Address=0 matching the entity-manager JSON Exposes entry.
# Upstream-Status: Pending
#
# 0002 adds the ADI AXI fan-control IP to the compatible fan types so
# fansensor can discover it via /sys/class/hwmon/hwmonN/fan1_input and pwm1.
#
# 0003 adds the NXP P3T1755 (MikroE Thermo 10 Click) to the hwmontempsensor
# type map so an entity-manager "P3T1755" Exposes entry is instantiated
# (driver "p3t1755", handled by the in-tree lm75 driver).
#
# 0004 adds the ST TSC1641 (MikroE Current 12 Click) to the psusensor type map
# so an entity-manager "TSC1641" Exposes entry is instantiated (driver
# "tsc1641", backported into the kernel hwmon tree by linux-socfpga-lts).
#
# NOTE: Patch 0005 (PercentRH allowlist) is intentionally NOT applied here.
# AMI's dbus-sensors already includes PercentRH -> "humidity" in unitsMap
# (SensorPaths.hpp). Applying 0005 would fail because AMI uses a unitsMap loop,
# not the if-chain style the patch targets.
SRC_URI:append = " \
    file://0001-hwmon-temp-support-soc64-hwmon-platform-device.patch \
    file://0002-fan-add-adi-axi-fan-control-to-compatibleFanTypes.patch \
    file://0003-hwmon-temp-add-p3t1755.patch \
    file://0004-psu-add-tsc1641.patch \
    "

# Agilex3/Agilex5 is ARM/aarch64 — the Intel CPU PECI sensor is x86-only.
# Remove it to avoid build failures from missing peci_i3c_chardev_to_cpu().
PACKAGECONFIG:remove = "intelcpusensor"
