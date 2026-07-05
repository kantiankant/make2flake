# make2flake

It converts makefiles into flakes. 


## Features

- Parses Makefiles and config.mk
- Detects dependencies using `nix-locate`
- Generates complete flake.nix with:
  - Packages and devShells
  - Build and install phases
  - Meta information

## Dependencies

* gcc/clang (a compiler)
* gnumake
* glibc/musl (a libc)
* nix
* nix-locate

## Installation (imperative)

```bash
git clone https://github.com/kantiankant/make2flake
cd make2flake
make
doas/sudo make install PREFIX=/usr/ (or your path of choice)
```
## Installation (Nix flake)

Add this to your flake.nix:

```bash
make2flake = {
    url = "github:kantiankant/make2flake";
    inputs.nixpkgs.follows = "nixpkgs";
 };
```

and this to your configuration.nix

```bash
inputs.make2flake.packages.${pkgs.system}.default
```

Then rebuild your configuration.


Usage

```bash
Usage: make2flake [OPTIONS]

Options:
  -i, --input <file>     Input Makefile (default: Makefile)
  -c, --config <file>    Additional config.mk file
  -o, --output <file>    Output flake.nix (default: stdout)
  -n, --name <name>      Project name (auto-detected if not set)
  -h, --help             Show this help
  --version              Show version
  --verbose              Verbose output
 ```

> Note: you still have to patch a few bits and pieces up (License info, Meta description, postPatch, and versions-specific stuff. 

## LICENSE

GPL-v3
