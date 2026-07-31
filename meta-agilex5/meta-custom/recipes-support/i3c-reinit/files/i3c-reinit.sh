#!/bin/sh
# i3c-reinit — re-run I3C bus initialization (RSTDAA + SETDASA + ENTDAA) from
# Linux and capture the transactions, without a reboot or a scope. This is the
# userspace equivalent of a "reset bus + start DAA" button: it unbinds and
# rebinds the DesignWare I3C master so the core re-enumerates every device.
#
# Use it to debug sensors that fail to appear (e.g. a TSC1641 power meter that
# never answers DAA): run it a few times and watch whether the device ever
# enumerates (intermittent => marginal bus timing) and what NACK/timeout the
# kernel logs.
#
# Usage:
#   i3c-reinit [device] [loops]
#     device  platform device of the I3C master (default 10da0000.i3c)
#     loops   number of reinit passes            (default 1)
#
# Requires CONFIG_DYNAMIC_DEBUG=y for the verbose i3c core/driver logs.

set -u

DEV="${1:-10da0000.i3c}"
LOOPS="${2:-1}"
DRV="/sys/bus/platform/drivers/dw-i3c-master"

if [ ! -d "$DRV" ]; then
    echo "i3c-reinit: driver dir $DRV not found" >&2
    exit 1
fi
if [ ! -e "$DRV/$DEV" ]; then
    echo "i3c-reinit: device $DEV not bound under $DRV" >&2
    echo "available:" >&2
    ls -1 "$DRV" 2>/dev/null | grep -E '\.i3c$' >&2
    exit 1
fi

# Verbose I3C logging (best effort — needs CONFIG_DYNAMIC_DEBUG)
echo 8 > /proc/sys/kernel/printk 2>/dev/null
mount -t debugfs none /sys/kernel/debug 2>/dev/null
DDBG="/sys/kernel/debug/dynamic_debug/control"
if [ -w "$DDBG" ]; then
    echo 'file drivers/i3c/master.c +p' > "$DDBG" 2>/dev/null
    echo 'module dw_i3c_master +p'      > "$DDBG" 2>/dev/null
    echo "i3c-reinit: dynamic_debug enabled"
else
    echo "i3c-reinit: WARNING no dynamic_debug (build needs CONFIG_DYNAMIC_DEBUG=y) — DAA detail will be limited"
fi

i=1
while [ "$i" -le "$LOOPS" ]; do
    echo "===== i3c-reinit pass $i/$LOOPS ($DEV) ====="
    dmesg -c >/dev/null 2>&1      # BusyBox uses lowercase -c to clear
    echo "$DEV" > "$DRV/unbind"; sleep 1
    echo "$DEV" > "$DRV/bind";   sleep 1
    echo "--- enumerated I3C devices ---"
    ls /sys/bus/i3c/devices/ 2>/dev/null | grep -E '^[0-9]+-' || echo "(none)"
    echo "--- kernel log ---"
    dmesg | grep -iE 'i3c|daa|setdasa|entdaa|rstdaa|nack|timeout|error'
    i=$((i + 1))
done
