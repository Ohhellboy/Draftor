// SPDX-License-Identifier: Apache-2.0
import {startRecoveryUI} from './recovery-ui.js';
import {StrictSerialTransport} from './serial.js';
import {ReadOnlyDriver} from './read-only-driver.js';
import {requestSerialPort} from './diagnostics.js';
import {browserIsQualified} from './release-platform.js';
import {requireSafe} from './safety.js';
import {publicRecoveryCopy} from './r10-copy.js';
// The builder produces two distinct entries. Online recovery never gets a writer.
const offline=false;
startRecoveryUI({build:__R10_BUILD__,sourceCommit:__R10_SOURCE__,recoveryEnabled:offline,
 copyOverrides:publicRecoveryCopy,supportedEnvironment:browserIsQualified(),embeddedGuide:offline,
 offlineHref:'offline/Draftor_Recovery_R10_Offline.html',
 requestDriver:async(mode,diagnostics)=>{
  requireSafe(browserIsQualified(),'PLATFORM');
  const wire=new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics});
  return new ReadOnlyDriver(wire,{diagnostics});
 }});
