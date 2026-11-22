# R11 Windows compatibility matrix

| Environment | Public access | Evidence / status |
| --- | --- | --- |
| Windows 10 desktop Chrome | Capability checks permit connection; informed beta | WINDOWS_DEVICE_FIELD_TEST_PENDING; exact Windows 10 hardware/browser combination not physically tested |
| Windows 11 desktop Chrome | Same | WINDOWS_DEVICE_FIELD_TEST_PENDING; exact Windows 11 hardware/browser combination not physically tested |
| Windows 10/11 desktop Microsoft Edge | Same; Edg brand does not block | WINDOWS_DEVICE_FIELD_TEST_PENDING; enterprise Serial policy may restrict access |
| Windows hosted runner, Chrome / Edge | Software verification in actual browser processes | Independent synthetic ROM/Flash, native capability/chooser, file roundtrip and FILE offline reopen; exact runner OS/browser versions and execution evidence recorded in R11_REPORT and CI artifacts; no USB passthrough |
| macOS desktop Chrome | Retained | R4/R6/R9 physical evidence inherited; R10/R11 software integration |
| Other desktop environments | Capability detected; not an OS whitelist | Not physically qualified; recommended environments above |
| Mobile / tablet | Read and download | USB operations refused |
| Missing secure context / Web Serial / serial permission | Read and download | Explicit reason; no fake enabled device workflow |

WINDOWS_SOFTWARE_PASS is never WINDOWS_DEVICE_PASS. Hosted Windows Server runner evidence is not mislabeled as testing every Windows 10/11 build. Physical Windows first-install and original-board offline recovery remain WINDOWS_DEVICE_PENDING until observed.

No hardware rule changed: USB VID/PID, chip/capacity/security, supported original state, exact release hashes, partition/OTA checks, saved package reimport, one-shot mutually exclusive write grants and protected-range/post-write checks all remain mandatory.
