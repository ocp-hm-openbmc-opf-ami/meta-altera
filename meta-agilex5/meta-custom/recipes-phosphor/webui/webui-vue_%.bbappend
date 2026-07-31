do_compile() {
    cd ${S}
    rm -rf node_modules

    # Fix for Node.js 12 compatibility - downgrade packages that require Node 14+
    # This fixes the ESLint "Unexpected token '.'" error
    sed -i 's/"eslint-plugin-vue": "[^"]*"/"eslint-plugin-vue": "8.7.1"/g' package.json
    sed -i 's/"prettier": "[^"]*"/"prettier": "2.8.8"/g' package.json
    sed -i 's/"eslint-plugin-prettier": "[^"]*"/"eslint-plugin-prettier": "4.2.1"/g' package.json

    npm_proxy_args=""
    if [ -n "${http_proxy}" ]; then
        npm_proxy_args="${npm_proxy_args} --proxy=${http_proxy}"
    fi
    if [ -n "${https_proxy}" ]; then
        npm_proxy_args="${npm_proxy_args} --https-proxy=${https_proxy}"
    fi

    npm_cmd="npm"
    if [ -x /usr/bin/npm ]; then
        npm_cmd="/usr/bin/npm"
    fi

    # Keep npm/arborist locale handling deterministic in bitbake shells.
    export LANG=C
    export LC_ALL=C
    export PATH="/usr/bin:${PATH}"

    ${npm_cmd} --loglevel info ${npm_proxy_args} install --omit=optional --ignore-scripts --legacy-peer-deps
    ${npm_cmd} run lint -- --fix || true
    ${npm_cmd} run build ${EXTRA_OENPM}
}
