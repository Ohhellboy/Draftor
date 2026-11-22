# Rebuild R10

Use Node >=22 and Python 3. Install exact package-lock.json dependencies with `npm ci`. In the corresponding source archive, SOURCE_IDENTITY.json pins every included file to the source commit.

`node tools/build-r10.mjs dist-r10-public`

The builder verifies firmware inputs, the eleven unchanged R4/R6/R9 core modules, and a finite release profile. It builds the self-contained offline recovery HTML first, embeds its SHA-256 in the normal installer, then generates the online read-only recovery entry, complete source archive and site file manifest. No live device or network is needed after dependencies are installed.

`node tools/validate-r10-pages.mjs dist-r10-public`

`node --test --test-concurrency=1 tests/r10-integration.test.mjs`

Tests use independent synthetic ROM ports and public firmware only. Tests, injected synthetic ports and preview routes must not be served as part of the public website. They appear only inside the corresponding source archive for reproduction. Test artifacts under .test-cache are not public inputs.

Development: add `--development` and use a new `dist-r10-public-devN` output. Such builds cannot pass the publisher. Existing output is never overwritten. R7/R9 publishers remain unchanged and continue rejecting writers.
