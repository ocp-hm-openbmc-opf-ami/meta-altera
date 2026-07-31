# Fix the meta-common stub: it stubs do_install[noexec]=1 but does NOT clear
# SYSTEMD_SERVICE. When the recipe hash changes and do_package must re-run,
# it looks for service files that were never installed (because do_install was
# noexec) and fails with "Didn't find service unit 'obmc-read-eeprom@.service'".
# Clearing SYSTEMD_SERVICE and SYSTEMD_PACKAGES here makes do_package agree
# with the empty install produced by the stub.
SYSTEMD_SERVICE:${PN} = ""
SYSTEMD_PACKAGES = ""
