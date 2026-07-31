# Remove the pam_pwquality module from the PAM password stack.
#
# pam_pwquality rejects passwords shorter than 8 characters or lacking
# complexity. Without this removal, changing the root password via Redfish
# AccountService or local 'passwd' will be silently rejected by bmcweb.
#
# The awk pass strips "use_authtok" from the first remaining password line.
# That flag tells the module to reuse a password provided by a previous
# stacked module (which was pam_pwquality). Without it the next module
# prompts for the password itself, which is the correct behaviour.

do_install:append() {
    sed -i '/pam_pwquality.so/d' ${D}${sysconfdir}/pam.d/common-password

    awk '/^password/ && !f{sub(/ use_authtok/, ""); f=1} 1' \
        ${D}${sysconfdir}/pam.d/common-password \
        > ${D}${sysconfdir}/pam.d/common-password.new
    mv ${D}${sysconfdir}/pam.d/common-password.new \
        ${D}${sysconfdir}/pam.d/common-password
}

RDEPENDS:${PN}-runtime:remove = "libpwquality"
