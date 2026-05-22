# Octra IDE

Native Qt desktop app for Octra wallet + contract dev tools.

Current targets:
- Linux desktop
- Windows desktop

Current native features:
- wallet create/unlock/import
- mainnet/devnet presets
- send OCT
- IDE-style contract workspace
- AML compile
- assembly compile
- multi-file project compile
- deploy / call / view / verify
- contract info / receipt / storage lookup
- FHE encrypt / decrypt

## Linux build

Requirements:
- `cmake >= 3.21`
- `ninja-build`
- `g++`
- `libssl-dev`
- `qt6-base-dev`
- `qt6-declarative-dev`
- `qt6-tools-dev-tools`
- `qml6-module-qtquick`
- `qml6-module-qtquick-controls`
- `qml6-module-qtquick-layouts`
- `qml6-module-qtqml-workerscript`
- `qml6-module-qtquick-templates`
- `qml6-module-qtquick-window`
- `qml6-module-qtquick-dialogs`
- `qml6-module-qt-labs-platform`

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ libssl-dev \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtqml-workerscript qml6-module-qtquick-templates \
  qml6-module-qtquick-window qml6-module-qtquick-dialogs \
  qml6-module-qt-labs-platform
```

Build:

```bash
cmake -S . -B build-native
cmake --build build-native -j2
```

Run:

```bash
./build-native/octra_wallet_native
```

### Devnet CLI

Build the CLI target:

```bash
cmake --build build-native --target octra_cli -j2
```

Deploy a contract on devnet:

```bash
./build-native/octra-cli deploy \
  --wallet data/wallet.oct \
  --pin 123456 \
  --source path/to/main.aml \
  --network devnet
```

Use `--bytecode` to skip compilation, or `--preview` to print the computed contract address without submitting.

Example templates include `vault`, `token`, `escrow`, `amm`, `multisig`, `empty`, and `dictionary`.

The native app also includes a `Dictionary DApp` tab for deploying the dictionary template and managing terms, definitions, translations, and metadata on devnet.

## Windows release

The repo includes:
- portable bundle packaging
- NSIS installer packaging
- GitHub Actions Windows build workflow for `main`
- GitHub Actions Windows release workflow for tags
- optional signing hook

Secrets for signed Windows releases:
- `WIN_SIGN_PFX_BASE64`
- `WIN_SIGN_PFX_PASSWORD`

Workflow file:
- `.github/workflows/windows-build.yml`
- `.github/workflows/windows-release.yml`

## Linux and macOS release

The repo also ships release workflows for the other desktop targets:
- `.github/workflows/linux-release.yml` creates a Linux tarball and publishes it on tag builds
- `.github/workflows/macos-release.yml` creates a macOS `.app` bundle zip and publishes it on tag builds

The non-release Linux CI build remains in:
- `.github/workflows/linux-build.yml`

## Linux CI

The repo also includes a Linux desktop build workflow that uploads the native app artifact from GitHub Actions.

## Notes

- Unsigned Windows builds may be flagged by Defender.
- Signed Windows releases require a real code-signing certificate.
- The legacy `main.cpp` localhost server app is still present in the repo, but `native/` is the active desktop app path.
