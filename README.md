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

## Windows release

The repo includes:
- portable bundle packaging
- NSIS installer packaging
- GitHub Actions Windows release workflow
- optional signing hook

Secrets for signed Windows releases:
- `WIN_SIGN_PFX_BASE64`
- `WIN_SIGN_PFX_PASSWORD`

Workflow file:
- `.github/workflows/windows-release.yml`

## Linux CI

The repo also includes a Linux desktop build workflow that uploads the native app artifact from GitHub Actions.

## Notes

- Unsigned Windows builds may be flagged by Defender.
- Signed Windows releases require a real code-signing certificate.
- The legacy `main.cpp` localhost server app is still present in the repo, but `native/` is the active desktop app path.
