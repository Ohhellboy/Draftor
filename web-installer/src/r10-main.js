// SPDX-License-Identifier: Apache-2.0
import {startPublicUI} from './public-ui.js';
import {StrictSerialTransport} from './serial.js';
import {S3Rom} from './rom.js';
import {requestSerialPort} from './diagnostics.js';
import {browserIsQualified} from './release-platform.js';
import {requireSafe} from './safety.js';
startPublicUI({build:__R10_BUILD__,sourceCommit:__R10_SOURCE__,offlineToolSha256:__R10_OFFLINE_HASH__,
 requestDriver:async diagnostics=>{
  requireSafe(browserIsQualified(),'PLATFORM');
  return new S3Rom(new StrictSerialTransport(await requestSerialPort(navigator.serial,diagnostics),{diagnostics}),{candidate:true,diagnostics});
 }});
