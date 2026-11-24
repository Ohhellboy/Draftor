// SPDX-License-Identifier: Apache-2.0
import { requestSerialPort } from './diagnostics.js';
import { startProductUI } from './product-ui.js';
import { browserIsQualified } from './release-platform.js';
import { S3Rom } from './rom.js';
import { requireSafe } from './safety.js';
import { StrictSerialTransport } from './serial.js';

class App0ReinstallRom extends S3Rom {
  async grantExclusiveMode(grant) {
    requireSafe(grant.mode === 'update' && grant.initialState === 'compatible-draftor', 'BUILD_SCOPE');
    requireSafe(grant.protection !== undefined, 'PROTECTION_REQUIRED');
    return super.grantExclusiveMode(grant);
  }
}

const publicSite = typeof __R12_PUBLIC__ !== 'undefined' && __R12_PUBLIC__;
startProductUI({
  build: __R12_BUILD__,
  sourceCommit: __R12_SOURCE__,
  app0Only: true,
  publicSite,
  async requestDriver(diagnostics) {
    const allowedOrigin = publicSite
      ? location.origin === 'https://draftor.tcssn.org'
      : location.protocol === 'http:' && ['127.0.0.1', 'localhost'].includes(location.hostname);
    requireSafe(allowedOrigin, 'ORIGIN');
    requireSafe(browserIsQualified(), 'PLATFORM');
    const port = await requestSerialPort(navigator.serial, diagnostics);
    const wire = new StrictSerialTransport(port, { diagnostics });
    return new App0ReinstallRom(wire, { candidate: true, diagnostics });
  },
});
