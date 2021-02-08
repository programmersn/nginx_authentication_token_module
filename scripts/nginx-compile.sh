#! /bin/bash

NGINX_VERSION=1.19.6

pushd "vendor/nginx-$NGINX_VERSION" >/dev/null 2>&1
CFLAGS="-g -O0" ./configure             \
    --with-debug                        \
    --prefix=$(pwd)/../../build/nginx   \
    --conf-path=conf/nginx.conf         \
    --error-log-path=logs/error.log     \
    --http-log-path=logs/access.log     \
    --add-module=../../
make
make install
popd >/dev/null 2>&1
