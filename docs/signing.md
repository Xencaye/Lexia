# Signing Lexia (optional)

An Authenticode signature tells Windows (and SmartScreen) who built `lexia.exe`. It is **off by
default**: the default build, CI and the zip are unsigned. Nothing about signing is stored in the
repo or in the CMake cache.

## What gets signed

With `-DLEXIA_SIGN=ON` the package step (`cmake/package.cmake`, running `lexia_sign_files` from
`cmake/sign.cmake`) signs these files **in the package folder** `build\package\Lexia\`:

- `lexia.exe`
- `piper.dll` (built by Lexia's own build from piper1-gpl and espeak-ng)

Signing happens after all files are copied and before `Lexia.zip` is made. Third-party DLLs (ONNX
Runtime, OpenCV, the Visual C++ runtime ...) are never re-signed: they keep their vendors' signatures.
The built `build\Release\lexia.exe` is not touched. The installer (task H4) must be built from the
signed package folder, and its own `.exe` signed with the same `lexia_sign_files` function.

Each file gets a SHA-256 file digest and an RFC 3161 timestamp (also SHA-256), so the signature
stays valid after the certificate expires. The default timestamp server is
`http://timestamp.digicert.com`; change it with `-DLEXIA_SIGN_TIMESTAMP_URL=<url>`.

## Getting a code-signing certificate

A certificate from a public CA (DigiCert, Sectigo, SSL.com ...; an OV or EV code-signing certificate,
or Microsoft Trusted Signing) makes the signature trusted on other PCs. EV certificates build
SmartScreen reputation fastest. Most CAs now ship the key on a hardware token or in a cloud HSM, so
you use the thumbprint route below. A self-signed certificate (see the end) is only good for testing:
other PCs do not trust it.

## Giving the build the certificate

Pick **one**; set it in the environment of the shell that runs `cmake --build` (or on the configure
line for the non-secret values). The thumbprint wins when both are set.

1. **Certificate in your certificate store (preferred).** Find the thumbprint (not a secret):

   ```powershell
   Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Format-Table Thumbprint, Subject, NotAfter
   ```

   ```powershell
   $env:LEXIA_SIGN_THUMBPRINT = "<thumbprint>"     # or -DLEXIA_SIGN_THUMBPRINT=<thumbprint>
   ```

2. **A .pfx file.** The path is not a secret; the password is read **only** from an environment
   variable, at build time, and is never cached or logged:

   ```powershell
   $env:LEXIA_SIGN_PFX = "C:\keys\lexia.pfx"       # or -DLEXIA_SIGN_PFX=...
   $env:LEXIA_SIGN_PFX_PASSWORD = "<password>"     # environment only
   ```

   signtool takes the password as a command-line argument, so another process on the same PC can see
   it for the second or so that signtool runs. Use the thumbprint route on a shared machine.

   Every program the build starts inherits the environment. The Piper and espeak-ng builds
   (third-party code) are run without `LEXIA_SIGN_PFX_PASSWORD` (CMake 4.2 or newer); vcpkg and
   MSBuild still see it. So set it only in the shell that runs `cmake --build`, not for the
   configure step (which builds the vcpkg packages), and clear it afterwards:

   ```powershell
   $env:LEXIA_SIGN_PFX_PASSWORD = "<password>"; cmake --build build --config Release; Remove-Item Env:LEXIA_SIGN_PFX_PASSWORD
   ```

## Build

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DLEXIA_SIGN=ON
cmake --build build --config Release
```

- `signtool.exe` comes from the Windows SDK ("Windows SDK Signing Tools for Desktop Apps"). If CMake
  cannot find it, configure stops with an error; point `-DLEXIA_SIGNTOOL_EXE=<full path>` at it.
- With no certificate set, the Release build stops at the package step with a message that names
  the variables above.

## Verify

```powershell
signtool verify /pa /v build\package\Lexia\lexia.exe
```

`/pa` uses the Authenticode policy, as Windows does when you run the file. You can also look at
Properties, Digital Signatures in Explorer. A test certificate fails `verify /pa` with "a certificate
chain processed, but terminated in a root certificate which is not trusted": that is expected.

## Testing with a throwaway certificate

```powershell
$c = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=Lexia test" -CertStoreLocation Cert:\CurrentUser\My
$env:LEXIA_SIGN_THUMBPRINT = $c.Thumbprint
# ... build, signtool verify ...
Remove-Item "Cert:\CurrentUser\My\$($c.Thumbprint)"     # remove it afterwards
```
