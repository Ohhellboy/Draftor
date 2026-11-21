# R9 reproducible build and format

This branch adds private package schema `draftor-private-recovery/1`, actual one-use full recovery and locked diagnostics. It does not enable normal update/fresh installation or publish a writer. Hardware approval is separate from building.

## Rebuild

Use the supplied source archive, Node 26 (minimum 22), npm lockfile and Python 3 for packaging. Ordinary users need none of these development tools. Run `npm ci`, then:

```
node tools/r9-software-check.mjs delivery/R9_20261121/software-rebuild
node tools/build-r9.mjs locked dist-r9-locked
node tools/build-r9.mjs recovery dist-r9-recovery
node tools/validate-r9-pages.mjs dist-r9-locked
node tools/serve-r9.mjs dist-r9-locked 4199
```

The builder refuses an existing output directory. `--development` produces an explicitly UNFROZEN, unpublishable artifact. Frozen checkout builds require no tracked changes; source-archive builds verify every input against SOURCE_IDENTITY.json. Change the packaging Python path to your Python 3 executable on a different development host; it is not part of runtime code. Build epoch and ZIP timestamps are fixed for reproducibility, while actual checks have actual timestamps.

`dist-r9-locked` uses only private `ReadOnlyDriver` wrappers. None can grant recovery, update or fresh capability. `dist-r9-recovery` uses a separate reviewed recovery entry, and its ordinary installation entry is still locked. The publishing gate rejects candidate/experimental flags, writable entry patterns, inconsistent manifests, symlinks and mixed mock/private files. A valid gate result is not deployment authorization. Never upload a candidate or offline recovery writer under the guise of locked diagnostics.

The self-contained `offline/Draftor_Recovery_R9_Offline.html` embeds code and CSS with CSP hashes. No CDN, fonts, import fetch, worker cache, localhost service or development runtime is needed. Supplied guides and licenses accompany the offline ZIP. Only the candidate variant can restore after checks and explicit consent; the locked variant cannot. Source archives include exact public release inputs and the already pinned vendor factory image for synthetic tests, never personal Flash.

## Private package v1

ZIP STORE profile, exactly four entries in this order: `manifest.json`, `flash.bin`, `README.zh-CN.txt`, `README.en.txt`. No compression, descriptors, extras, comments, encryption, ZIP64, duplicates, path normalization or trailing bytes. Total size is bounded to 16 MiB + 32 KiB; image exactly 16 MiB, JSON at most 12 KiB, each readme at most 4 KiB. Local and central headers, offsets, CRCs and all lengths must agree. Unknown schemas fail closed. JSON uses the tool's canonical pretty encoding, rejecting duplicate keys and unsupported keys.

The manifest contains a random UUID, actual creation time, source/build, physical board confirmation, normalized original MAC, actual ROM chip/capacity/security fields and their observation sources, full SHA-256/MD5, before/after full ROM MD5 proof and rederived original partition/OTA/firmware classification. A handle in a module-private WeakMap holds validated immutable copies; callers cannot provide arbitrary JSON or addresses to the writer. Hashes/MAC are not signatures or malicious-forgery guarantees.

Supported originals: exact pinned Alpha 3 bootloader plus B table, unique app0 and exact App; or the exact FactoryOnly 20260120 three-range fingerprint, with only its original NVS/PHY mutable range. This qualification applies to the backup. Damaged current partitions/OTA/App do not block recovery; live identity/security/capacity still must match. Current Flash equality remains mandatory in the original installation path and when creating/reselecting an installation-time backup.

## Authority and verification

Recovery is mutually exclusive with update/fresh. `grantRecovery` only accepts a validated handle and separate board/TF/full-restore consent. `restoreRecovery` accepts no address or image argument. It rechecks the live original identity/security/capacity, sends exactly one full 16 MiB FLASH_BEGIN and 16,384 uncompressed 1024-byte FLASH_DATA commands. No erase-all opcode, stub, FLASH_END, reset or command retry is used. Any failure consumes the session. Cancellation after an attempted command may leave partial Flash.

Verification obtains actual ROM full MD5, then slow-reads the restored table and applicable OTA, compares ROM MD5 and original metadata SHA. Factory has no invented Draftor OTA requirement. An explicit fresh read-only session can check again. Independent Python esptool 5.3.1 is acceptance evidence only, not a user dependency.

Diagnostic counters are placed in existing command and port lifecycle paths; the SLIP/ROM parser is unchanged. Attempts, sent promises and accepted replies are separate, including eight replies to one SYNC. Unknown checks remain NOT_RUN. Error/close results do not prove independent wire capture. Close clears private DOM/references/controllable buffers and Blob URLs; JavaScript cannot guarantee physical memory erasure.

## Test distinctions

Node tests execute the real S3Rom/StrictSerialTransport against an independent synthetic port/Flash model; fault tests alter the destination, not the writer's source cache. `tests/r9-ui-main.mjs` is a separate SOFTWARE TEST entry using that same protocol model and the product UI. Its Node crypto/assert imports are adapted only for browser software tests. The builder cannot put this entry in a frozen site. User-reported disconnected-network file reopening and simulated restoration are software evidence, never hardware recovery evidence.

Technical references: [Chrome Web Serial](https://developer.chrome.com/docs/capabilities/serial/) requires a user gesture for choosing a port; [Web Serial specification](https://wicg.github.io/serial/) describes its permission and security model. R9's file-origin support is based on the actual target Chrome probe and offline software run, not inferred from these documents. ROM behavior follows the frozen esptool 5.3.1 source and the project's existing R6 implementation; see CAPABILITY_AUDIT.md and R9_REPORT for evidence limits.
