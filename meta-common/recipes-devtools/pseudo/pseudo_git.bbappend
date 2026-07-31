# Ubuntu 24.04 uses libc/tooling paths that require pseudo openat2 support.
# Keep this local override until upstream pins a newer pseudo SRCREV.
SRCREV = "490e339c48059cf89303ea9844fdacbca81a228f"

# This patch only applies to the older pinned pseudo revision.
SRC_URI:remove = "file://0001-configure-Prune-PIE-flags.patch"

# Newer pseudo no longer needs the old native glibc symbol backport patch.
SRC_URI:remove:class-native = "file://older-glibc-symbols.patch"
SRC_URI:remove:class-nativesdk = "file://older-glibc-symbols.patch"
