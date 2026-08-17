SUMMARY = "OpenBMC package groups for Agilex 3 HPS BMC"
DESCRIPTION = "Satisfies virtual-obmc-* providers required by obmc-phosphor-image. \
Phase 0: stubs only — no hardware-specific software yet. \
Phase 1+ will add fan control, sensor management, host control, and IPMI OEM extensions here."
PR = "r1"

inherit packagegroup
inherit obmc-phosphor-utils

PROVIDES = "${PACKAGES}"
PACKAGES = " \
    ${PN}-chassis \
    ${PN}-flash \
    ${PN}-system \
    ${PN}-fans \
    "

PROVIDES += "virtual/obmc-chassis-mgmt"
PROVIDES += "virtual/obmc-flash-mgmt"
PROVIDES += "virtual/obmc-system-mgmt"
PROVIDES += "virtual/obmc-fan-mgmt"

RPROVIDES:${PN}-chassis += "virtual-obmc-chassis-mgmt"
RPROVIDES:${PN}-flash   += "virtual-obmc-flash-mgmt"
RPROVIDES:${PN}-system  += "virtual-obmc-system-mgmt"
RPROVIDES:${PN}-fans    += "virtual-obmc-fan-mgmt"

SUMMARY:${PN}-chassis = "Agilex 3 Chassis management (Phase 0 stub)"
RDEPENDS:${PN}-chassis = ""

SUMMARY:${PN}-flash = "Agilex 3 Flash management (Phase 0 stub)"
RDEPENDS:${PN}-flash = ""

SUMMARY:${PN}-system = "Agilex 3 System management"
RDEPENDS:${PN}-system = " \
    dbus-broker-config \
    first-boot-set-priv \
    phase0-masks \
    network-config \
    ipmitool \
    curl \
    i2c-tools \
    spidev-test \
    i3c-reinit \
    dashboard \
    "

SUMMARY:${PN}-fans = "Agilex 3 Fan management"
RDEPENDS:${PN}-fans = " \
    entity-manager \
    entity-config \
    dbus-sensors \
    kernel-module-axi-fan-control \
    phosphor-pid-control \
    sdm-pwm-demo \
    fan-pwm-manual \
    iis2dulpx-accel \
    qtpy-uart-bridge \
    p3t1755-temp-bridge \
    tsc1641-current-bridge \
    "
