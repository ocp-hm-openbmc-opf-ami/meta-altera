#@TYPE: Machine Include
#@NAME: Altera SoCFPGA ARMv8-2A (Agilex 3 / Agilex 5 Cortex-A55 core)
#@DESCRIPTION: Common SoC settings for Altera SoCFPGA devices with ARMv8-2A CPU
#
# NOTE: This stub is intentionally minimal.  The canonical version of this file
# lives in meta-altera-bsp (inside the meta-altera-fpga repo) and is reached via
# BBPATH when that layer is present.  This copy only covers fields not supplied
# by meta-altera-bsp so the machine conf is usable even if the include path is
# resolved here first.
#
# For the full set of SoCFPGA BSP variables (KERNEL_REPO, UBOOT_REPO, ATF_REPO,
# fit_image.its templates, device-tree, fpga-bitstream recipes, etc.) you MUST
# add meta-altera-fpga/meta-altera-bsp to BBLAYERS.
# See README.md for cloning instructions.

# ---------------------------------------------------------------------------
# CPU tune — dual Cortex-A55, ARMv8.2-A, 64-bit little-endian
# ---------------------------------------------------------------------------
require conf/machine/include/arm/armv8-2a/tune-cortexa55.inc

# ---------------------------------------------------------------------------
# SoC family override (used by COMPATIBLE_MACHINE guards in BSP recipes)
# ---------------------------------------------------------------------------
SOC_FAMILY = "socfpga"
include conf/machine/include/soc-family.inc
MACHINEOVERRIDES .= ":socfpga:agilex"

# ---------------------------------------------------------------------------
# Mandatory BSP variable stubs (overridden by meta-altera-bsp values when
# that layer is present and takes priority)
# ---------------------------------------------------------------------------
PREFERRED_PROVIDER_virtual/kernel       ?= "linux-socfpga-lts"
PREFERRED_PROVIDER_virtual/bootloader   ?= "u-boot-socfpga"

# ---------------------------------------------------------------------------
# OpenBMC phosphor machine features
# ---------------------------------------------------------------------------
MACHINE_FEATURES += " \
    obmc-phosphor-chassis-mgmt \
    obmc-phosphor-flash-mgmt \
    obmc-phosphor-fan-mgmt \
    "

# ---------------------------------------------------------------------------
# Image types — wic (SD boot) + tar.xz for rootfs archive
# ---------------------------------------------------------------------------
IMAGE_FSTYPES += "wic tar.xz"
WKS_FILE ?= "sdimage-agilex3-openbmc.wks"
