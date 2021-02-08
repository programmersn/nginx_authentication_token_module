#!/bin/bash

set -o nounset
set -o errexit

DIR=$(pwd)
BUILDDIR=$DIR/build
NGINX_DIR=nginx
NGINX_VERSION=1.19.6

clean() {
    rm -rfd build vendor
}

setup_local_directories() {
    if [ ! -d $BUILDDIR ]; then
        mkdir $BUILDDIR >/dev/null 2>&1
        mkdir $BUILDDIR/$NGINX_DIR >/dev/null 2>&1
    fi

    if [ ! -d "vendor" ]; then
        mkdir vendor >/dev/null 2>&1
    fi
}

fetch_nginx() {
    if [ ! -d "vendor/nginx-$NGINX_VERSION" ]; then
        pushd vendor >/dev/null 2>&1
        curl -s -L -O "http://nginx.org/download/nginx-$NGINX_VERSION.tar.gz"
        tar xzf "nginx-$NGINX_VERSION.tar.gz"
        popd >/dev/null 2>&1
    else
        printf "NGINX already fetched\n"
    fi
}

install_nginx() {
    if [ -e "$BUILDDIR/$NGINX_DIR/sbin/nginx" ]; then
        printf "Nginx already installed"
        exit 1
    fi

    if [ ! -d "vendor/nginx-$NGINX_VERSION" ]; then
        printf "Fetch nginx sources first !"
        exit 1
    fi

    pushd "vendor/nginx-$NGINX_VERSION" >/dev/null 2>&1
    ./configure \
        --with-debug \
        --prefix=$(pwd)/../../build/nginx \
        --conf-path=conf/nginx.conf \
        --error-log-path=logs/error.log \
        --http-log-path=logs/access.log
    make
    make install
    popd >/dev/null 2>&1
    ln -sf $(pwd)/nginx.conf $(pwd)/build/nginx/conf/nginx.conf
 }

if [[ "$#" -eq 0 ]]; then
    setup_local_directories
    fetch_nginx
    install_nginx
elif [[ "$#" -eq 1 ]]; then
    if [[ "$1" == "clean" ]]; then
        clean
    elif [[ "$1" == "install" ]]; then
        install_nginx
    elif [[ "$1" == "fetch" ]]; then
        fetch_nginx
    else
        echo "option not recognized"
    fi
else
    printf "Command line malformed"
fi
