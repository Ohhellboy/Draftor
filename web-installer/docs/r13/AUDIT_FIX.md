# R13.2 audit corrections

Base: published source `651ff0719ac8a89b5163fedf6b76b763516e13c6`.

The clean-install modal's recovery HTML download now uses the same bounded,
progress-aware stream reader as firmware preparation. It has a 60-second idle
deadline, reset only by nonempty decoded data; no whole-file deadline. Its
published SHA-256 is checked by the network reader and independently by the
unchanged PublicInstallSession before the saved-file checkbox can be accepted.

Pure network failure (connection, HTTP status, idle, incomplete response) leaves
the read-only PLAN_READY device session intact and invalidates the tool grant.
Only that file can be manually retried; no serial reconnect, erase, write or
reset is performed. SHA/size failure is terminal and closes the session. Stop,
modal X, Escape and pagehide abort HTTP before releasing the device connection.
Receipts separately retain offlineToolDownload with sanitized error and progress.
Existing backup/readback/identity/target/erase/write checks are unchanged.

Three tests (r6-first-install, safety and rom) explicitly pass
`verifyDesignBrief:false`, just like the existing public tests. The private task
Brief remains excluded from the source archive. All firmware/archive hashes,
release identity, paired manifests and binary checks remain enabled. Acceptance
must extract the actual generated source ZIP, confirm BRIEF.md is absent,
install its locked dependencies and run its own unmodified `npm test`.

Artifact browser evidence must identify the formal directory, BUILD version,
source commit, manifest hash, app/reinstall/offline hashes and actually loaded
script hashes. `tools/test-r13-download-browser.mjs` records those when given a
formal build directory; its no-argument development mode remains explicitly
DEVELOPMENT_BUILD. Native macOS Edge uses a visible isolated window (the prior
headless automation stability issue is not treated as a product result).

`tools/test-r13-offline-browser.mjs ROOT RECEIPT [EDGE_EXECUTABLE|chrome] [fixed|baseline]`
uses unchanged formal app.js plus an independently injected synthetic ROM.
It covers hanging-body stop/X/Escape, HTTP/network/idle retry without reconnect,
wrong hash with terminal close, and a continuous download longer than 60 seconds.
It records HTTP-abort-before-synthetic-serial-close order and zero erase/write/
reset counters. Baseline mode reproduces the original hanging-close defect.
Native browser, artifact identity, synthetic serial, UA simulation and true
Windows device qualification are separate fields. Windows 11 Edge hardware
installation and recovery remain PENDING.

Build: `node tools/build-r13-public.mjs dist-r13-audit-public`.
The pinned prior Pages baseline is
`33258cb0eaf70669b6569ecfc749be6d617d20cb`; preserve it for rollback.
