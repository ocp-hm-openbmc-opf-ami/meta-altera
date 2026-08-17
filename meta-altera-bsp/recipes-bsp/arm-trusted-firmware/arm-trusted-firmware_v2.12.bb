require arm-trusted-firmware.inc

LIC_FILES_CHKSUM = "file://docs/license.rst;md5=83b7626b8c7a37263c6a58af8d19bee1"

ATF_VERSION ?= "v2.12.1"
ATF_BRANCH ?= "socfpga_${ATF_VERSION}"
ATF_SRCREV ?= "0f91af488e5ce466117709b912a68494ba3431ff"

SRCREV = "${ATF_SRCREV}"
