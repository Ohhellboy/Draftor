// SPDX-License-Identifier: Apache-2.0
import {startRecoveryUI} from './recovery-ui.js';import {softwareDriver} from '../tests/r12-browser-environment.mjs';import {requireSafe} from './safety.js';
import {browserIsQualified,platformMessage,publicErrorCode,serialErrorHelp} from './release-platform.js';import {publicRecoveryCopy} from './r11-copy.js';
startRecoveryUI({build:__R12_BUILD__,sourceCommit:__R12_SOURCE__,recoveryEnabled:__R12_OFFLINE__&&__R12_SOFTWARE__,embeddedGuide:__R12_OFFLINE__,
 copyOverrides:Object.fromEntries(['zh','en'].map(l=>[l,{...publicRecoveryCopy[l],supported:platformMessage(l),unsupported:platformMessage(l)}])),mapError:publicErrorCode,errorHelp:serialErrorHelp,supportedEnvironment:browserIsQualified(),offlineHref:'offline/Draftor_Recovery_R12_Offline.html',
 requestDriver:async(mode,d)=>{requireSafe(__R12_SOFTWARE__,'REVIEW_ONLY');return softwareDriver(d,__R12_OFFLINE__&&mode==='restore'?'restore':'read-only');}});
