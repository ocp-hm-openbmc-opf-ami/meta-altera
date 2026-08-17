FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

RDEPENDS:${PN}:remove = "clear-once"

# meta-core adds an unconditional do_compile:prepend install from
# ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml. Ensure that file exists before task
# body executes so we can keep meta-core untouched and avoid hard failures.
do_compile[prefuncs] += "prepare_led_yaml_for_staging"

prepare_led_yaml_for_staging() {
    install -d ${STAGING_DATADIR_NATIVE}/${PN}

    if [ -f ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml ]; then
        return 0
    fi

    if [ -f ${S}/led.yaml ]; then
        cp -f ${S}/led.yaml ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml
    elif [ -n "${LED_YAML_PATH}" ] && [ -f ${LED_YAML_PATH}/led.yaml ]; then
        cp -f ${LED_YAML_PATH}/led.yaml ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml
    elif [ -f ${WORKDIR}/led.yaml ]; then
        cp -f ${WORKDIR}/led.yaml ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml
    else
        bbwarn "No led.yaml found for staging bootstrap; creating empty fallback to satisfy prepend install"
        : > ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml
    fi
}

do_compile:prepend(){
    if [ -f ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml ]; then
        install -m 0644 ${STAGING_DATADIR_NATIVE}/${PN}/led.yaml ${S}
    else
        bbwarn "No native led.yaml found in ${STAGING_DATADIR_NATIVE}/${PN}; using source/default configuration"
    fi
}

do_install:append(){
    rm -f ${S}/led.yaml
}
