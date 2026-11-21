// SPDX-License-Identifier: Apache-2.0
import { S3Rom } from './rom.js';
import { Stop } from './safety.js';
export class ReadOnlyDriver {
  #rom;#diagnostics;kind='hardware';
  constructor(transport,{diagnostics}={}) {this.#diagnostics=diagnostics;this.#rom=new S3Rom(transport,{diagnostics});}
  connect(){return this.#rom.connect();}
  identity(){return this.#rom.identity();}
  readFlash(offset,length){return this.#rom.readFlash(offset,length);}
  flashMd5(offset,length){return this.#rom.flashMd5(offset,length);}
  verifyRecoveryImage(handle){return this.#rom.verifyRecoveryImage(handle);}
  #locked(){this.#diagnostics?.event('blockedWriteRequests');throw new Stop('DEVICE_WRITES_LOCKED');}
  writeImage(){this.#locked();} eraseAll(){this.#locked();} grantRecovery(){this.#locked();} restoreRecovery(){this.#locked();}
  cancel(){this.#rom.cancel();} disconnect(){return this.#rom.disconnect();}
}
