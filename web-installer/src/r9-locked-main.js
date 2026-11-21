// SPDX-License-Identifier: Apache-2.0
import { startUI } from './ui.js';
import { StrictSerialTransport } from './serial.js';
import { ReadOnlyDriver } from './read-only-driver.js';
import { Diagnostics,requestSerialPort } from './diagnostics.js';
// Replaced with constants by the frozen builder. They do not grant write access.
const build=__R9_BUILD__,sourceCommit=__R9_SOURCE__;
startUI({publicRelease:true,bindSelectedIdentity:true,recoveryLink:true,
 diagnosticsFactory:()=>new Diagnostics({build,sourceCommit,originType:location.protocol==='https:'?'HTTPS':'LOOPBACK',origin:location.origin,mode:'installation-plan'}),
 requestDriver:async(_,diagnostics)=>new ReadOnlyDriver(new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics}),{diagnostics})});
