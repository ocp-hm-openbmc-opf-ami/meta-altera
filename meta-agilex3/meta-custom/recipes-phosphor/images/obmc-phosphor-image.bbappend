# Add IPMI host daemon, ipmitool, bmcweb, and the sensor stack to the
# agilex3 image.
#
# LF_CORE_FEATURES is cleared in local.conf.sample so obmc-bmcweb is NOT
# added via EXTRA_IMAGE_FEATURES; bmcweb must be included explicitly.
#
# srvcfg-manager: bmcweb reads /etc/srvcfg-manager/srvcfg.json at startup
# (sessions.hpp) for session limits and service config. Without this package
# the file is missing and bmcweb logs "Error opening config file" on every
# boot (non-fatal but noisy). The AMI bbappend installs a default srvcfg.json.
#
# Sensor stack (entity-manager + entity-config + dbus-sensors):
#   entity-manager   — reads entity-config.json and publishes hardware
#                      descriptions on D-Bus for dbus-sensors to discover
#   entity-config    — Altera SoCFPGA static chassis/sensor JSON config
#   dbus-sensors     — reads entity-manager D-Bus objects, instantiates
#                      sensor daemons (hwmontempsensor, fansensor, etc.)
#                      and publishes xyz.openbmc_project.Sensor.* objects
#   ipmid then reads those D-Bus sensor objects via libdynamiccmds.so
#   (built with -Ddynamic-sensors=enabled in phosphor-ipmi-host bbappend)
#
# phosphor-ipmi-net (netipmid/RMCP) is excluded — its meta-common stub
# makes it complex to enable and it is not required for local use.
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
    first-boot-set-priv \
    "
