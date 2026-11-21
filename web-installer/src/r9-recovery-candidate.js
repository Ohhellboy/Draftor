// SPDX-License-Identifier: Apache-2.0
// REVIEW ONLY. This entry is never an input of the locked production builder.
import { startRecoveryUI } from './recovery-ui.js';
import { StrictSerialTransport } from './serial.js';
import { ReadOnlyDriver } from './read-only-driver.js';
import { S3Rom } from './rom.js';
import { requestSerialPort } from './diagnostics.js';
startRecoveryUI({build:__R9_BUILD__,sourceCommit:__R9_SOURCE__,recoveryEnabled:true,
 requestDriver:async(mode,diagnostics)=>{
  const wire=new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics});
  return mode==='restore'?new S3Rom(wire,{allowRecovery:true,diagnostics}):new ReadOnlyDriver(wire,{diagnostics});
 }});
