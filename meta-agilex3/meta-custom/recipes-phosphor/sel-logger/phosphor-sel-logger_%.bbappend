DEPENDS:agilex3_openbmc += "intel-ipmi-oem"
RDEPENDS:${PN}:agilex3_openbmc += "intel-ipmi-oem"
RDEPENDS:${PN}:agilex3_openbmc:remove = "intel-ipmi-oem-ext"
