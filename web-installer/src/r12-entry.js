// SPDX-License-Identifier: Apache-2.0
import {startProductUI} from './product-ui.js';import {requireSafe} from './safety.js';
// R12 is a local software candidate. Real USB is unavailable in both artifacts:
// review root locks requests; the test root supplies its own in-memory port.
import {softwareDriver} from 'r12-driver-adapter';
const software=__R12_SOFTWARE__;
startProductUI({build:__R12_BUILD__,sourceCommit:__R12_SOURCE__,reviewOnly:!software,
 requestDriver:async d=>{requireSafe(software,'REVIEW_ONLY');return softwareDriver(d,'installation');}});
