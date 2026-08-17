# Provide the machine-specific entity-config.json from this meta-custom layer.
# The base recipe (in meta-common) installs entity-config.json; this bbappend
# prepends the meta-custom files/ directory so the machine-specific version wins.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
