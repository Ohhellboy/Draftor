# R13.1 download revision

This revision changes file preparation only. Alpha 3 firmware, TF resources,
ROM protocol, hardware admission, backup and restore, erase/write/readback and
single-write failure rules remain unchanged.

The old implementation placed `AbortSignal.timeout(60000)` on the entire fetch.
It collapsed fetch and body errors into DOWNLOAD and the UI closed the session.
The Windows Edge report is consistent with that deadline, but the original
browser's underlying exception was not captured and is not claimed as proven.

The new ReleaseDownloader uses a 60-second **idle** deadline. Headers alone and
empty chunks do not reset it. Each nonempty decoded chunk does; there is no
whole-file deadline. Exact pinned byte count, SHA-256, binary MD5, manifest and
release-pair checks remain required. Compressed Content-Length is not mistaken
for the size of the decoded file. The existing auxiliary factory asset on the
reinstall page also uses cancellable, progress-aware, pinned downloading.

Only complete, individually verified files enter a private in-memory cache.
One installation dialog owns one cache. The user can explicitly retry failed
preparation; no timer retries automatically. Failed/partial data is cleared.
Closing the dialog or leaving the page aborts requests and clears the cache.
No serial request is possible until preparation succeeds. USB and writing
failures retain their existing terminal stop behavior.

Both installation pages show actual received bytes, verification status,
translated concise errors and a Retry download button. Sanitized receipts add
`downloadPreparation` with file, stage, category, expected/received bytes, HTTP
status and bounded attempt history. No browser exception text, URLs, private
files or device identity are included in these download details.

Failure categories: NETWORK, IDLE_TIMEOUT, HTTP_STATUS, INCOMPLETE, LENGTH,
SHA256, MD5, RELEASE and CANCELLED. A server can close a response with a transport
error instead of clean EOF; that is recorded as NETWORK, not guessed as a hash
failure. Files remain untrusted in either case.

Run `node --test tests/r13-download.test.mjs tests/r7-http.test.mjs` and `npm test`.
Run `node tools/test-r13-download-browser.mjs` for controlled real-time HTTP tests
and the complete UI flow to serial-selection readiness. The harness disables
real serial access. Slow app.bin takes more than 60 real seconds; stalled data
hits the production idle limit. HTTP failure, interruption, truncation, bad SHA,
manual retry/cache reuse, close/cancel, both pages and both languages are checked.
The optional fourth argument is a native Edge executable; an Edge user-agent
simulation is separately labeled and is never Windows device evidence.

Build from a clean source commit using
`node tools/build-r13-public.mjs dist-r13-download-public`.
The builder pins the previous published site's complete manifest, rebuilds the
actual installer and recovery programs, packages corresponding source and writes
a new BUILD.json and SITE_SHA256SUMS. Previous Pages commit
`b673e971da06cc4b8fc9d14a0ea656cac5634657` remains the rollback baseline.

Windows hardware installation, recovery and normal boot remain PENDING. These
software/browser tests neither access a physical board nor establish that result.
