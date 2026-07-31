# Append OpenBMC-required kernel config fragments.
# This bbappend is intentionally minimal — it adds only the configs that
# every Altera SoCFPGA OpenBMC machine needs regardless of board:
#   - config_openbmc.cfg : systemd mount-namespace flags, HWMON, I3C, SPI
#   - openbmc.scc        : wraps config_openbmc.cfg in a kconfig fragment set
#
# Board-specific configs (sensor drivers, custom DTS, TSC1641 out-of-tree
# driver, etc.) are added by the board sublayer bbappend.

FILESEXTRAPATHS:prepend := "${THISDIR}/linux-socfpga-lts/configs:"

SRC_URI:append = " file://openbmc.scc"
