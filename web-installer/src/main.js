import { startUI } from './ui.js';
import { StrictSerialTransport } from './serial.js';
import { S3Rom } from './rom.js';

// Compile-time gate, never a URL flag or editable runtime manifest field.
const allowUpdate = typeof DEVICE_TEST_UPDATE !== 'undefined' && DEVICE_TEST_UPDATE;
startUI({ hardwareWritesEnabled: allowUpdate,
  requestDriver: async () => {
    const port = await navigator.serial.requestPort();
    return new S3Rom(new StrictSerialTransport(port), { allowUpdate });
  },
});
