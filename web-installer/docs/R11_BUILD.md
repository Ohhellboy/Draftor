# Rebuild R11

Use Node >=22, Python 3, and npm ci from the pinned package-lock.json. Run node tools/build-r11.mjs dist-r11-public, then node tools/validate-r11-pages.mjs dist-r11-public. Output must not already exist. In a source archive SOURCE_IDENTITY.json pins every input and the source commit; a Git checkout must be committed and clean. No private backups are inputs.

Eleven ROM/transport/installation/recovery core files remain locked to tools/r10-inherited-core.json. Firmware/factory/release inputs are verified before bundling. R11 changes capability guidance, UI messages, documented Windows beta scope and publishing metadata; it does not change the core writer.

Tests: node --test tests/r11-platform.test.mjs tests/r10-integration.test.mjs. Windows CI extracts this exact source archive and the frozen site, installs pinned dependencies, compares production scripts, and runs tests/r11-windows-browser.mjs in installed Chrome/Edge. All serial models are independent synthetic fixtures; no USB hardware, private data or browser profile is available. Native picker tests only cancel. FILE offline tests close the original browser and use a new browser with outbound networking blocked; synthetic ports never retain the native USB interface. CI artifacts record OS/browser versions, file hashes, captures and failures. A hosted Windows Server runner is not a physical Windows 10/11 device result.

Production has no synthetic route or port injection. Offline HTML includes all runtime code, styles, guide and licenses; its digest is pinned by the normal installer. Source and both prior release rollback materials remain independently available. R10 and R9 validators remain unchanged.
