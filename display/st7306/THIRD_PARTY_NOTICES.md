# Third-party notices and provenance

## OSPTEK initialization reference

Original publisher: **Osptek / OSPTEK**. The individual file author is not stated
in the inspected file; no individual attribution is inferred.

The fixed provenance revision is `a21ee6a42118ac198b2fa97941f95c5359a1a07d` in
[OSPTEK's repository](https://github.com/osptek/lcd-4.2-300x400-spi-st7306).
The historical repository name was `osptek/4.2-lcd-300x400-spi-st7306` and now redirects.
This identifier selects the existing historical reference; no file hashes were generated.

Original file:
[st7306_color4.cc](https://github.com/osptek/lcd-4.2-300x400-spi-st7306/blob/a21ee6a42118ac198b2fa97941f95c5359a1a07d/versions/YDP420H003-V3/examples/ESP32S3-4p2Q300400-ST7306/main/st7306/st7306_color4.cc),
under `versions/YDP420H003-V3/examples/ESP32S3-4p2Q300400-ST7306/main/st7306/`.

Copyright (c) 2026 Osptek. The fixed revision's root
[LICENSE](https://github.com/osptek/lcd-4.2-300x400-spi-st7306/blob/a21ee6a42118ac198b2fa97941f95c5359a1a07d/LICENSE)
grants MIT permissions, including modification, publication and redistribution,
subject to retaining its notice. Its exact text is in [licenses/OSPTEK-MIT.txt](licenses/OSPTEK-MIT.txt).
The complete fixed repository tree lists no other LICENSE/COPYING/NOTICE file.

The same revision's
[README](https://github.com/osptek/lcd-4.2-300x400-spi-st7306/blob/a21ee6a42118ac198b2fa97941f95c5359a1a07d/README.md)
additionally labels repository materials **CC BY 4.0**. Both upstream statements
are retained here; this package does not assert an undocumented code/document
split or that the entire package is MIT-only. For that notice, the attribution is
OSPTEK, the work and source are linked above, and the license is
[Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
No endorsement by OSPTEK is claimed. Both stated licenses permit modified public
distribution; downstream release terms must preserve the applicable notices and attribution.

## Existing downstream changes

The current driver was copied from Draftor, not regenerated from upstream.
The initialization provenance passes through the existing Pico donor and its
Arduino product port. Relative to the referenced upstream initializer, the
existing LPM variant sets B2 to 0x15 instead of 0x12 and selects 0x39 instead of
0x38. Reset remains high/low/high for 10 ms each. Draftor's transaction table
preserves its donor transaction groups; it is not a new translation of the
vendor's per-parameter calls.

Draftor uses a static 30,000-byte framebuffer, reverses both logical axes and
maps logical Red/White to wire codes 2/1. The retained transports use Arduino
SPI with software CS/DC; they do not use the vendor ESP-IDF DMA allocation and
bus/device ownership. Pico retains its full-width native-row method. Existing
fill, pixel, rectangle, full-frame and CRC helper APIs remain unchanged.

The original vendor driver, example application, images and documents are not
bundled. New geometry demos and staging instructions are downstream additions.

## Other dependencies

Arduino-ESP32 / Arduino-Pico and their SPI implementations are external build
dependencies, not redistributed in this package. Their own notices continue to
apply if somebody later bundles those cores or distributes linked binaries.
No font data or font license is bundled because the demos use no font.

Draftor-owned code and new files in this ST7306 module are released under the
MIT License provided in the module directory. Third-party material retains its
original copyright and license terms; the OSPTEK notice is not treated as a
blanket downstream grant.
