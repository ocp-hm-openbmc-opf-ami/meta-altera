# phosphor-ipmi-net (netipmid / RMCP) is stubbed by meta-common for this
# "hostless" BSP build.  meta-common sets do_install[noexec]=1 and
# SYSTEMD_PACKAGES="" but does NOT clear SYSTEMD_SERVICE, which causes
# do_package to fail when the hash changes and the task must re-run.
# Clear SYSTEMD_SERVICE here so do_package does not look for files that
# were never installed.
SYSTEMD_SERVICE:${PN} = ""
SYSTEMD_PACKAGES = ""
