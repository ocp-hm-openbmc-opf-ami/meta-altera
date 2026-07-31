# Enable Redfish BMC journal access (off by default upstream) so the WebUI and
# Redfish clients can read the BMC system journal via
# /redfish/v1/Managers/bmc/LogServices/Journal.
PACKAGECONFIG:append = " redfish-bmc-journal"
