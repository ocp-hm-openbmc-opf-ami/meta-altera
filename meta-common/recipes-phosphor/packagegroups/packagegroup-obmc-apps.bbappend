# Remove packages that are not applicable to this platform.
#
# trace-enable: enables Linux trace events via boot loader — not needed here;
#               its postinstall script also fails in walnascar do_rootfs.
# obmc-ikvm:    IP KVM daemon — no video/KVM hardware on this board;
#               its postinstall script also fails in walnascar do_rootfs.
#
# Note: packagegroup-obmc-apps.bb has no version in its filename so this
# bbappend uses no version separator (correct form for version-less recipes).

RDEPENDS:${PN}-devtools:remove = "trace-enable"
RDEPENDS:${PN}-ikvm:remove = "obmc-ikvm"
