#!/bin/sh
# DEMO ONLY (pre-hardware bring-up): open-loop map of an SDM on-die temperature
# to the axi_fan_control PWM, so the temperature -> PWM path is visible on the
# dashboard before the MikroE Thermo 10 (Board_Temp) and Geekworm X-FAN40
# arrive.
#
# This is NOT the production control path. swampd (phosphor-pid-control) is the
# real controller; it needs the Thermo 10 (Board_Temp input) to drive the
# temperature -> PWM map (the fan tach is monitored for RPM only, not fed back
# into the control output). This helper just lets you see a
# temp-driven PWM on the bare board. The unit Conflicts with swampd and is
# disabled by default. Remove the sdm-pwm-demo recipe to revert.
#
# Linear map (override via the service Environment): LO_C -> 0%, HI_C -> 100%.

LO_C="${LO_C:-30}"
HI_C="${HI_C:-60}"

find_hwmon() {
    for h in /sys/class/hwmon/hwmon*; do
        if [ "$(cat "$h/name" 2>/dev/null)" = "$1" ]; then
            echo "$h"
            return 0
        fi
    done
    return 1
}

fan_hwmon="$(find_hwmon axi_fan_control)" || {
    echo "sdm-pwm-demo: axi_fan_control hwmon not found" >&2
    exit 1
}
temp_hwmon="$(find_hwmon soc64hwmon)" || {
    echo "sdm-pwm-demo: soc64hwmon hwmon not found" >&2
    exit 1
}

PWM="$fan_hwmon/pwm1"
TEMP="$temp_hwmon/temp1_input"

echo "sdm-pwm-demo: TEMP=$TEMP PWM=$PWM map ${LO_C}C..${HI_C}C -> 0..255" >&2

while true; do
    t=$(( $(cat "$TEMP") / 1000 ))
    if [ "$t" -le "$LO_C" ]; then
        p=0
    elif [ "$t" -ge "$HI_C" ]; then
        p=255
    else
        p=$(( (t - LO_C) * 255 / (HI_C - LO_C) ))
    fi
    echo "$p" > "$PWM"
    sleep 1
done
