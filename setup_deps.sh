#!/usr/bin/env sh

mkdir -p vendor

pushd vendor

SEC256K1_VERSION=0.3.1
rm -f secp256k1.tar.gz secp256k1-*

curl -L -O https://github.com/bitcoin-core/secp256k1/archive/refs/tags/v${SEC256K1_VERSION}.tar.gz
tar xf v${SEC256K1_VERSION}.tar.gz
rm v${SEC256K1_VERSION}.tar.gz

pushd secp256k1-${SEC256K1_VERSION}

./autogen.sh
./configure \
    --disable-benchmark \
    --disable-tests \
    --disable-module-ecdh \
    --disable-shared

make

popd

popd

