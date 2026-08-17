#!/bin/sh
# Set root user privilege to priv-admin on first boot if not already set.
# phosphor-user-manager initialises the privilege database lazily; without
# this the Roles field in Redfish session responses is empty and the web UI
# login silently fails.

SERVICE=xyz.openbmc_project.User.Manager
OBJECT=/xyz/openbmc_project/user/root
IFACE=xyz.openbmc_project.User.Attributes
PROP=UserPrivilege

current=$(busctl get-property "$SERVICE" "$OBJECT" "$IFACE" "$PROP" 2>/dev/null \
    | awk '{print $2}' | tr -d '"')

if [ -z "$current" ] || [ "$current" = "" ]; then
    echo "Setting root UserPrivilege to priv-admin"
    busctl set-property "$SERVICE" "$OBJECT" "$IFACE" "$PROP" s "priv-admin"
else
    echo "root UserPrivilege already set to: $current"
fi

# Disable password expiry so the web UI login does not force a password change.
busctl set-property "$SERVICE" "$OBJECT" "$IFACE" UserPasswordExpired b false
echo "root UserPasswordExpired cleared"
