// SPDX-License-Identifier: Apache-2.0
import {startRecoveryUI} from './recovery-ui.js';
import {StrictSerialTransport} from './serial.js';
import {ReadOnlyDriver} from './read-only-driver.js';
import {S3Rom} from './rom.js';
import {requestSerialPort} from './diagnostics.js';
import {browserIsQualified,platformMessage,publicErrorCode,serialErrorHelp} from './release-platform.js';
import {requireSafe} from './safety.js';
import {publicRecoveryCopy} from './r11-copy.js';
const offlineName = typeof __INSTALLER_OFFLINE_NAME__ === 'undefined' ? 'Draftor_Recovery_R11_Offline.html' : __INSTALLER_OFFLINE_NAME__;
// The builder produces two distinct entries. Online recovery never gets a writer.
const offline=true;
startRecoveryUI({build:__R11_BUILD__,sourceCommit:__R11_SOURCE__,recoveryEnabled:offline,
 copyOverrides:Object.fromEntries(["zh","en"].map(l=>[l,{...publicRecoveryCopy[l],supported:platformMessage(l),unsupported:platformMessage(l)}])),mapError:publicErrorCode,errorHelp:serialErrorHelp,supportedEnvironment:browserIsQualified(),embeddedGuide:offline,
 offlineHref:'offline/'+offlineName,
 requestDriver:async(mode,diagnostics)=>{
  requireSafe(browserIsQualified(),'PLATFORM');
  const wire=new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics});
  return offline&&mode==='restore'?new S3Rom(wire,{allowRecovery:true,diagnostics}):new ReadOnlyDriver(wire,{diagnostics});
 }});
