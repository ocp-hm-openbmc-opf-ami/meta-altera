PACKAGECONFIG = "${@bb.utils.contains('ENABLE_MCTP_KERNEL_MODE', '1', 'libmctp-kernel-mode', '', d)}"
PACKAGECONFIG[systemd] = ""
PACKAGECONFIG[pcap] = ""
PACKAGECONFIG[libmctp-kernel-mode] = " -Dmctp-in-kernel-enable=enabled "

# Upstream libmctp recipe carries autotools-style PACKAGECONFIG options
# that are invalid for the meson-based AMI libmctp fork.
EXTRA_OECONF = ""