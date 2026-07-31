# The upstream phosphor-led-manager repo moved led-group-config.json from the
# repo root into the example/ subdirectory.  Fix the install path and install
# the file under both expected names:
#   led.json             -- used by phosphor-led-manager base recipe
#   led-group-config.json -- expected by meta-core do_compile:prepend
do_install() {
    install -d ${D}${datadir}/phosphor-led-manager
    install -m 0644 ${S}/example/led-group-config.json \
        ${D}${datadir}/phosphor-led-manager/led.json
    install -m 0644 ${S}/example/led-group-config.json \
        ${D}${datadir}/phosphor-led-manager/led-group-config.json
}
