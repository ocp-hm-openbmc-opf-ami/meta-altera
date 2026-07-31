# meta-altera — Altera SoCFPGA OpenBMC Layer

OpenBMC platform layer for Altera SoCFPGA devices (Agilex 3 C-Series).

## Supported Machines

| Machine              | Device             | Board                          |
|----------------------|--------------------|--------------------------------|
| `agilex3_openbmc`    | A3CW135BM16AE6S    | DK-A3W135BM16AEA (C-Series DK) |

## Layer Dependencies

| Layer             | Collection name  | Source                                                                 |
|-------------------|------------------|------------------------------------------------------------------------|
| `meta-phosphor`   | `phosphor-layer` | OpenBMC tree (`##OEROOT##/meta-phosphor`)                             |
| `meta-altera-bsp` | `altera-bsp`     | External — **must be cloned separately** (see below)                   |
| `meta-arm`        | `arm`            | OpenBMC tree (`##OEROOT##/meta-arm/meta-arm`)                         |
| `meta-intel-openbmc` | `intel-openbmc` | OpenBMC tree (`##OEROOT##/meta-intel-openbmc`)                       |

### Cloning meta-altera-fpga (required external BSP layer)

`meta-altera-bsp` lives inside the **meta-altera-fpga** repository and provides the
kernel (`linux-socfpga-lts`), U-Boot (`u-boot-socfpga`), Trusted Firmware-A, and
`conf/machine/include/socfpga_armv8-2a.inc`.

```bash
# Clone alongside the openbmc tree (adjust path as needed)
git clone https://github.com/altera-collab/applications.fpga.soc.meta-altera-fpga.git \
    --branch scarthgap \
    /home/<user>/AMI_OCP_Altera/meta-altera-fpga
```

Then update `meta-altera/conf/templates/default/bblayers.conf.sample`, replacing
`##META_ALTERA_FPGA_PATH##` with the actual clone path.

## Quick Start

### Using the `setup` script

```bash
cd /home/<user>/AMI_OCP_Altera/openbmc
# Update bblayers.conf.sample first (replace ##META_ALTERA_FPGA_PATH##)
source setup agilex3_openbmc build-agilex3
bitbake obmc-phosphor-image
```

### Manual bblayers.conf

Add the following to `build/conf/bblayers.conf`:

```
BBLAYERS += " \
    /path/to/meta-altera-fpga/meta-altera-bsp \
    ##OEROOT##/meta-altera \
    "
```

## Layer Structure

```
meta-altera/
  conf/
    layer.conf                          — Layer metadata (collection: altera-layer, priority: 7)
    machine/
      include/
        socfpga_armv8-2a.inc            — Stub; canonical version from meta-altera-bsp
      agilex3_openbmc.conf              — MACHINE = "agilex3_openbmc"
    templates/default/
      bblayers.conf.sample              — Used by the openbmc/setup script
      local.conf.sample                 — BSP versions, MACHINE, DISTRO defaults
  recipes-core/
    dbus-broker-config/                 — Systemd sandbox overrides (CONFIG_MNT_NS workaround)
  recipes-extended/
    pam/
      libpam_%.bbappend                 — Remove pam_pwquality to allow simple passwords
  recipes-phosphor/
    first-boot-set-priv/                — Set root Redfish priv-admin on first boot
    interfaces/
      bmcweb_%.bbappend                 — Enable Redfish BMC journal
    ipmi/
      phosphor-ipmi-fru_%.bbappend      — Stub (hostless BMC)
      phosphor-ipmi-host_%.bbappend     — Keep libipmid for dependents
      phosphor-ipmi-net_%.bbappend      — Stub (no RMCP needed)
    packagegroups/
      packagegroup-obmc-apps.bbappend   — Remove trace-enable, obmc-ikvm
    sensors/
      dbus-sensors_%.bbappend           — Apply Altera-specific sensor patches
      dbus-sensors/                     — Patch files (soc64-hwmon, AXI fan, P3T1755, TSC1641)
```

## BSP Versions (walnascar / scarthgap)

| Component            | Version         | Repository                                             |
|----------------------|-----------------|--------------------------------------------------------|
| Linux kernel         | 6.18 LTS        | `github.com/altera-fpga/linux-socfpga`                 |
| U-Boot               | v2026.01        | `github.com/altera-fpga/u-boot-socfpga`               |
| ARM Trusted Firmware | v2.14           | `github.com/altera-fpga/arm-trusted-firmware`          |
| Yocto release        | walnascar       | OpenBMC tree                                           |

## Key Variables to Set in local.conf / kas.yml

```bitbake
MACHINE             = "agilex3_openbmc"
DISTRO              = "openbmc-phosphor"
LINUX_DTS_FILE      = "socfpga_agilex3_socdk.dts"
UBOOT_DEFCONFIG     = "socfpga_agilex3_defconfig"
UBOOT_DEVICE_TREE   = "socfpga_agilex3_socdk.dtb"
SSBL_BOOT_SOURCE    = "mmc0"    # or "qspi"
FPGA_RBF_FILE       = "baseline.core.rbf"   # place in recipes-fpga/fpga-bitstream/files/
```
