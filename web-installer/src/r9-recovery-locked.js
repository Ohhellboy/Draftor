// SPDX-License-Identifier: Apache-2.0
import { startRecoveryUI } from './recovery-ui.js';
import { StrictSerialTransport } from './serial.js';
import { ReadOnlyDriver } from './read-only-driver.js';
import { requestSerialPort } from './diagnostics.js';
startRecoveryUI({build:__R9_BUILD__,sourceCommit:__R9_SOURCE__,recoveryEnabled:false,
 requestDriver:async(_,diagnostics)=>new ReadOnlyDriver(new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics}),{diagnostics})});
