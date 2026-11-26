# Build the public installer

Run `npm ci`. Export the repository root of Pages commit `505e2e597faa441a14b0cf6adb48d6240226ad8f` to `web-installer/dist-r13-baseline`. From web-installer run `npm run build:r13-public -- dist-r13-public`. The builder checks the complete pinned baseline and official target assets, generates current native Web Serial bundles and the self-contained offline recovery HTML, includes corresponding source and regenerates SITE_SHA256SUMS.

Publish the output contents to the existing Pages branch with the current remote commit as parent. The baseline preserves current bgIME documentation, branding and other site pages. The source package contains installer code, tests, lockfiles, firmware and reproducible build inputs. A baseline export is available through GitHub at the pinned Pages commit.

New unknown/blank full-image packages use draftor-private-recovery/2 with all 16 MiB of original bytes. Legacy /1 packages keep their exact metadata and readme validation. Unknown layouts are represented as raw metadata ranges. The reinstall route continues to require the known compatible image and layout.

The existing Node tests use a synthetic independent ROM port through the actual S3Rom driver. Hardware-test results must be recorded separately.
