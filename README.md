# ddfb a DDF bundle command line tool

This standalone tool helps to create and sign DDF bundles with the following features:

* Create DDF bundles from base JSON DDF files.
* Create keys for signing DDF bundles.
* Sign DDF bundles.

DDF bundles are self contained **immutable** files with file extension `.ddf`. They provide device integration in deCONZ. They contain the full DDF JSON files as well as scripts and other files. To learn more about DDF bundles refer to the specification at https://github.com/deconz-community/ddf-tools/blob/main/packages/bundler/README.md



## Building with CMake

1. Checkout this repository

2. Compile `ddfb` tool

```
cmake -B build -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build --config MinSizeRel
```

This creates the `ddfb` command line cli tool with no external dependencies.

## Usage

### 1. Creating a DDF bundle

```
./ddfb create <path-to-ddf-file.json>
```

This command bundles up all files referenced in the base DDF JSON file and creates a standalone file ending in `.ddf` file extension. It is not signed yet.

### 2. Creating a singing key

```
./ddfb keygen <name-for-key>
```

The keygen command creates two files `name-for-key` and `name-for-key.pub` which contain the private and public keys for signing.

### 3. Sign a DDF bundle

```
./ddfb sign <bundle.ddf> <keyfile> 
```

The sign command adds the signature over a bundle to the `.ddf` file (if it isn't already signed by that key).

A bundle may contain multiple signatures, e. g. in order to raise the status of a bundle from beta to stable after testing.

## External Libraries

`ddfb` bundles several lightweight, header-only or single-file libraries under `utils/` and `vendor/`. All are vendored directly — no external dependencies are required at build time.

| Library | Location | License | Upstream |
|---------|----------|---------|----------|
| u_sstream | `utils/u_sstream.{c,h}` | BSD-3-Clause | [~cryo/u_sstream](https://git.sr.ht/~cryo/u_sstream) |
| u_bstream | `utils/u_bstream.{c,h}` | BSD-3-Clause | [~cryo/u_bstream](https://git.sr.ht/~cryo/u_bstream) |
| u_base64 | `utils/u_base64.{c,h}` | BSD-3-Clause | [~cryo/u_base64](https://git.sr.ht/~cryo/u_base64) |
| cj (JSON parser) | `utils/cj.{c,h}` | BSD-3-Clause | [~cryo/cj](https://git.sr.ht/~cryo/cj) |
| lonesha256 | `vendor/lonesha256.h` | CC0 / Public Domain | [lonesha256](https://github.com/ThomasBellemain/LoneSHA256) |
| micro-ecc (uECC) | fetched via CMake | MIT | [kmackay/micro-ecc](https://github.com/kmackay/micro-ecc) v1.1 |