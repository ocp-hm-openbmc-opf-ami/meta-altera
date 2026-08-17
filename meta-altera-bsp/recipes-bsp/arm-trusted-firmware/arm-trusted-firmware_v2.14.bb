require arm-trusted-firmware.inc

LIC_FILES_CHKSUM = "file://docs/license.rst;md5=6ed7bace7b0bc63021c6eba7b524039e"

ATF_VERSION ?= "v2.14.0"
ATF_BRANCH ?= "socfpga_${ATF_VERSION}"
ATF_SRCREV ?= "${AUTOREV}"

SRCREV = "${ATF_SRCREV}"
