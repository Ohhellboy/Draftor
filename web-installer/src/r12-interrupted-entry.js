// SPDX-License-Identifier: Apache-2.0
import {startInterruptedUI} from './interrupted-ui.js';import {softwareDriver} from '../tests/r12-browser-environment.mjs';import {requireSafe} from './safety.js';
startInterruptedUI({build:__R12_BUILD__,sourceCommit:__R12_SOURCE__,reviewOnly:!__R12_SOFTWARE__,requestDriver:async d=>{requireSafe(__R12_SOFTWARE__,'REVIEW_ONLY');return softwareDriver(d,'read-only');}});
