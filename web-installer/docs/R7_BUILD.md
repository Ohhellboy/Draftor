# R7 locked public website

Node.js 22 or later and Python 3 are required for building. Unzip the corresponding source, run `npm ci`, then `node tools/build-r7.mjs`. An existing output is never overwritten. `SOURCE_IDENTITY.json` pins every source file; Git checkouts use their clean HEAD. ZIP timestamps and the release epoch are fixed, allowing byte-for-byte rebuilding.

Only `dist-r7-locked/` is approved for Pages. It has no simulation page, test injection entry point, hardware writer capability, factory maintenance image, device backup, telemetry or private identity. `BUILD.json` describes the artifact and is never consulted to grant permissions. `SITE_SHA256SUMS` covers every other file. Source maps and corresponding source include the actual runtime dependency code and license notices.

Deployment uses the independent `gh-pages` root with `.nojekyll` and `CNAME`. The firmware main branch is untouched. Public writing requires a separately reviewed candidate and explicit approval. Restore the frozen locked static tree to `gh-pages` to revoke future writes; already-open pages require reload and cannot be remotely terminated.
