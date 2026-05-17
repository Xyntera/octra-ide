# Windows Signing

Provide these GitHub secrets for signed releases:

- `WIN_SIGN_PFX_BASE64`
- `WIN_SIGN_PFX_PASSWORD`

The workflow will:

1. build the MSVC Windows app
2. create the portable zip
3. create the NSIS installer
4. sign the exe and installer if secrets are present

Without the secrets, artifacts are still built but unsigned.
