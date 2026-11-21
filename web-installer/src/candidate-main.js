// SPDX-License-Identifier: Apache-2.0
// REVIEW ONLY. Never deployed by tools/build-r7.mjs or the Pages publisher.
import { startUI } from './ui.js';
import { StrictSerialTransport } from './serial.js';
import { S3Rom } from './rom.js';
startUI({ publicRelease: true, bindSelectedIdentity: true, candidate: true,
  requestDriver: async () => new S3Rom(new StrictSerialTransport(await navigator.serial.requestPort()), { candidate: true }),
});
