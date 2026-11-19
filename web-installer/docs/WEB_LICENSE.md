# Web installer source and licenses

The original Draftor release, firmware and resource notices retain their existing licenses. The unmodified exact firmware source archive is self-hosted at `sources/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip`, SHA-256 `7cca7a13b685ab74f22150f0ed6521081bb9fcceec078da49ff457c85f67f69e`.

`src/rom.js` adapts the ROM protocol/control sequence from Espressif esptool 5.3.1, which is **GPL-2.0-or-later**, not Apache-2.0. Its copyright and license are preserved. The combined browser program is distributed under **GPL-3.0**, an allowed later GPL version compatible with its Apache-2.0 components. New independent application modules carry Apache-2.0 notices; this does not remove the combined program's GPL obligations.

Corresponding installer source, tests, build scripts, dependency lock, source maps, original input records and notices are provided in `sources/Draftor_Web_Installer_R1_source.zip`. The delivery also provides this archive separately. `npm ci && npm run build` reproduces the default locked site; the exact system tool requirements and optional TF verification procedure are in its README.

Runtime components:

| Component | Exact version | License | Use |
| --- | --- | --- | --- |
| esptool-js Transport | 0.7.1 | Apache-2.0 | SLIP encoder only; no main/runStub/default flash/reset/retry paths |
| spark-md5 | 3.0.2 | MIT | Local digest computation for the ROM MD5 comparison |
| esptool ROM sequence | 5.3.1 | GPL-2.0-or-later | Explicit S3 ROM protocol adaptation |

Build/test components are pinned in package-lock: esbuild 0.25.12 (MIT) and Playwright 1.56.1 (Apache-2.0). Package-lock also records resolved transitive versions and registry integrity values. No dependency is fetched by the running website.

See `LICENSE`, `NOTICE`, `LICENSES/`, `Notices/`, the release's `THIRD_PARTY.md` / `LICENSE_INVENTORY.md`, and the checked-in upstream source excerpts under `reference/`. Browser source maps retain original runtime library contents. The firmware distribution source and licenses are not replaced by this web application's license.
