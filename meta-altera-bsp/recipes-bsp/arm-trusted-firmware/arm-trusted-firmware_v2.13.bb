require arm-trusted-firmware.inc

LIC_FILES_CHKSUM = "file://docs/license.rst;md5=6ed7bace7b0bc63021c6eba7b524039e"

ATF_VERSION ?= "v2.13.0"
ATF_BRANCH ?= "socfpga_${ATF_VERSION}"
ATF_SRCREV ?= "4539d77c0f23035e2389cc91c82125fce0eb3daa"

SRCREV = "${ATF_SRCREV}"
