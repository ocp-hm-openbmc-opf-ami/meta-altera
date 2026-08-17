SUMMARY = "Minimal SPI loopback/transfer test utility (board bring-up)"
DESCRIPTION = "Compact, self-contained spidev_test for verifying the SPI signal \
path (HPS SPIM -> FPGA fabric -> header pins) without a sensor attached. Jumper \
MOSI to MISO at the connector and run 'spidev_test -D /dev/spidev1.0 -v' — if RX \
echoes TX the pinmux/fabric/IO path is proven good. Replaces the kernel \
tools/spi/spidev_test.c which is not packaged in this image."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# file:// sources are available from ${WORKDIR} for this simple single-file recipe.
S = "${UNPACKDIR}"

SRC_URI = "file://spidev_test.c"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o spidev_test ${UNPACKDIR}/spidev_test.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 spidev_test ${D}${bindir}/spidev_test
}

FILES:${PN} = "${bindir}/spidev_test"
