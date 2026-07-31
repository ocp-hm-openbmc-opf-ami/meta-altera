LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"

LINUX_VERSION ?= "6.18.2"
LINUX_VERSION_SUFFIX = "-lts"
LINUX_SRCREV ?= "${AUTOREV}"
SRCREV = "${LINUX_SRCREV}"

do_kernel_configcheck[noexec] = "1"

include linux-socfpga.inc
