# Firmware distribution materials — v0.1.0-alpha.1

The Firmware archive retains the exact 2,061,808-byte device-tested app and original paired bootloader, partition table and boot_app0. No rebuild, merged image, device dump, ELF/map or private device log is included by this integration.

The original tested link map identified 71 libraries with contributing members ([component list](LINKED_COMPONENTS.json)). The existing BuildInputs/release-materials supplement provides fixed IDF/managed/submodule sources and notices, Arduino corresponding source/modifications, GCC Runtime Exception, Newlib declarations, CHIP/JSONCPP notices and MPL source data/generator. This is the already prepared scope, not a new audit or claim that every library uses one license.

For the Arduino LGPL portions, the distribution route uses LGPL-2.1 §6(a)/(d): matching library source and changes, complete application source, and required data/build/relink utilities are supplied together through Source and BuildInputs at the same release location. Recipients may modify the library and reverse engineer the combined work for debugging those modifications. A fabricated three-year written offer is not used. Fixed SDK/toolchain installation requirements remain in [BUILD](BUILD.md); the prior build evidence does not promise a bit-identical rebuild.

Specific outstanding distribution evidence remains in [RESOURCE_STATUS](RESOURCE_STATUS.md): mixed-data terms, the fixed CMake recipe whose inclusion the owner requested without an established grant, and the 18 MOE fact-record scope notes. Inclusion and supplied materials are not LICENSE_PASS. Older broad D02/D03/D05 labels inside unchanged companion metadata are superseded by this current scope statement; technical payload identity is unchanged.

The release tag identifies this integrated source and the unchanged tested product pair. [RELEASE_PAIR.json](../RELEASE_PAIR.json) and [RELEASE_ASSETS.json](RELEASE_ASSETS.json) distinguish version labels from the original build ID.
