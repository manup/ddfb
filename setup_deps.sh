#!/usr/bin/env sh

mkdir -p vendor

pushd vendor

# micro-ecc

UECC_VERSION=1.1

rm -fr micro-ecc
curl -L https://github.com/kmackay/micro-ecc/archive/refs/tags/v${UECC_VERSION}.zip > micro-ecc.zip
unzip micro-ecc.zip
rm -fr micro-ecc.zip
mv micro-ecc-${UECC_VERSION} micro-ecc

popd

