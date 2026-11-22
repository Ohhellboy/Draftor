// SPDX-License-Identifier: Apache-2.0
import {startPublicUI} from './public-ui.js';
import {StrictSerialTransport} from './serial.js';
import {S3Rom} from './rom.js';
import {requestSerialPort} from './diagnostics.js';
import {browserIsQualified,platformMessage,publicErrorCode,serialErrorHelp} from './release-platform.js';
import {requireSafe} from './safety.js';
startPublicUI({build:__R11_BUILD__,sourceCommit:__R11_SOURCE__,offlineToolSha256:__R11_OFFLINE_HASH__,offlineToolHref:"../offline/Draftor_Recovery_R11_Offline.html",offlineToolName:"Draftor_Recovery_R11_Offline.html",
 requestDriver:async diagnostics=>{
  requireSafe(browserIsQualified(),'PLATFORM');
  return new S3Rom(new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics}),{candidate:true,diagnostics});
 }});
