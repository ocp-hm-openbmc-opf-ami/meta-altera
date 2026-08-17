# Hostless BMC — network IPMI (RMCP) is not needed (no host to control).
# Stub all build tasks. populate_sysroot is left to run against the empty
# ${D} so a valid manifest exists for any further dependency resolution.
do_configure[noexec] = "1"
do_compile[noexec] = "1"
do_install[noexec] = "1"
# Keep packaging enabled so sstate manifests and (empty) packages are produced
# for image dependency resolution.
SYSTEMD_PACKAGES = ""
SYSTEMD_SERVICE:${PN} = ""
ALLOW_EMPTY:${PN} = "1"
