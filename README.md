# ddfb a DDF bundle command line tool

This standalone tool helps to create and sign DDF bundles with the following features:

* Create DDF bundles from base JSON DDF files.
* Create keys for signing DDF bundles.
* Sign DDF bundles.

DDF bundles are self contained **immutable** files with file extension `.ddf`. They provide device integration in deCONZ. They contain the full DDF JSON files as well as scripts and other files. To learn more about DDF bundles refer to the specification at https://github.com/deconz-community/ddf-tools/blob/main/packages/bundler/README.md



## Building on Posix environment

1. Compile dependencies

```
./setup_deps.sh
```

This downloads and compiles the Bitcoin secp256k1 static library used for signing.

2. Compile `ddfb` tool

```
make
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