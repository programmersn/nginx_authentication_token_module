#! /bin/bash

NGINX_VERSION=1.19.6

compile_nginx() {
    pushd "vendor/nginx-$NGINX_VERSION" >/dev/null 2>&1
    CFLAGS="-g -O0" ./configure \
        --with-debug \
        --prefix=$(pwd)/../../build/nginx \
        --conf-path=conf/nginx.conf \
        --error-log-path=logs/error.log \
        --http-log-path=logs/access.log \
    make
    make install
    popd >/dev/null 2>&1
}

compile_dynamic_module() {
    pushd "vendor/nginx-$NGINX_VERSION" >/dev/null 2>&1
    CFLAGS="-g -O0" ./configure \
        --with-debug \
        --prefix=$(pwd)/../../build/nginx \
        --conf-path=conf/nginx.conf \
        --error-log-path=logs/error.log \
        --http-log-path=logs/access.log \
        --add-dynamic-module=../../
    make modules    
    make install
    popd >/dev/null 2>&1
}

if [[ "$#" -eq 0 ]]; then
    compile_nginx
elif [[ "$#" -eq 1 ]]; then
    if [[ "$1" == "modules" ]]; then
        compile_dynamic_module
    else
        echo "option not recognized"
    fi
else
    printf "Command line malformed"
fi
