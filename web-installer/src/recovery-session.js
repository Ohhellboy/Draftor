// SPDX-License-Identifier: Apache-2.0
import { PINS } from './pins.js';
import { inspectOriginalFlash } from './first-install.js';
import { requireSafe, Stop, sha256, md5, activeSlot, checkPartitions } from './safety.js';
import { checkedIdentity, sameOriginalIdentity, createRecoveryPackage, validateRecoveryPackage, recoverySnapshot, recoverySummary, destroyRecoveryPackage } from './recovery-package.js';

export async function inspectSupportedOriginal(driver, diagnostics) {
  const table = await driver.readFlash(0x8000, 4096);
  const ota = await driver.readFlash(0xe000, 8192);
  requireSafe(table.length === 4096 && ota.length === 8192, 'READBACK');
  for (const [label, offset, bytes] of [['partition', 0x8000, table], ['ota', 0xe000, ota]]) {
    diagnostics?.check(label + 'Read', true);
    const match = md5(bytes) === await driver.flashMd5(offset, bytes.length);
    diagnostics?.check(label + 'Md5Match', match);
    requireSafe(match, 'READBACK');
  }
  try { return await inspectOriginalFlash(driver, table, ota); }
  finally { table.fill(0); ota.fill(0); }
}
export class RecoverySession {
  #driver;#diagnostics;#build;#source;#handle;#identity;#romInfo;#beforeMd5;#capture;#busy=false;#closed=false;#consent;#mode;
  state='IDLE';
  constructor(driver,{diagnostics,build,sourceCommit,mode='backup'}={}) {
    requireSafe(['backup','restore','verify'].includes(mode),'STATE');this.#driver=driver;this.#diagnostics=diagnostics;this.#build=build;this.#source=sourceCommit;this.#mode=mode;
  }
  #stage(s) {this.state=s;this.#diagnostics?.stage(s);}
  #available() {requireSafe(!this.#closed&&!this.#busy&&this.state!=='STOP','STATE');}
  async selectPackage(input) {
    this.#available();
    if(!['IDLE','PACKAGE_VALIDATED'].includes(this.state)){this.cancel('PACKAGE_CHANGED');destroyRecoveryPackage(this.#handle);this.#handle=undefined;throw new Stop('STATE');}
    this.#busy=true;
    destroyRecoveryPackage(this.#handle);this.#handle=undefined;this.#stage('IDLE');
    try {this.#handle=await validateRecoveryPackage(input);requireSafe(!this.#closed,'CANCELLED');this.#diagnostics?.check('packageValidated',true);this.#stage('PACKAGE_VALIDATED');return recoverySummary(this.#handle);}
    finally {this.#busy=false;if(this.#closed){destroyRecoveryPackage(this.#handle);this.#handle=undefined;}}
  }
  async prepare({boardConfirmed,tfRemoved}={}) {
    this.#available();requireSafe(boardConfirmed===true&&tfRemoved===true,'BOARD_CONFIRM');
    requireSafe(this.#mode==='backup'?this.state==='IDLE':this.state==='PACKAGE_VALIDATED','STATE');this.#busy=true;this.#consent={boardConfirmed,tfRemoved};
    try {
      this.#stage('CONNECTING');await this.#driver.connect();this.#romInfo=await this.#driver.identity();this.#identity=checkedIdentity(this.#romInfo);
      if(this.#mode==='backup') {
        this.#beforeMd5=await this.#driver.flashMd5(0,PINS.flashBytes);
        const original=await inspectSupportedOriginal(this.#driver,this.#diagnostics);this.#diagnostics?.check('originalState',original);this.#stage('DEVICE_VERIFIED');return original;
      }
      const {manifest,image}=recoverySnapshot(this.#handle);image.fill(0);
      sameOriginalIdentity(this.#romInfo,manifest.device);this.#diagnostics?.check('originalIdentityMatch',true);
      const plan={mode:this.#mode,offset:0,bytes:PINS.flashBytes,original:manifest.original.firmware,currentImageMayDiffer:true,noReset:true,noStub:true};
      this.#diagnostics?.check('plan',plan);this.#diagnostics?.check('originalState',{firmware:manifest.original.firmware,activeApp:manifest.original.activeApp});
      this.#stage(this.#mode==='restore'?'READY_FOR_CONFIRMATION':'DEVICE_VERIFIED');return plan;
    } catch(error){this.#stop(error);throw error;}finally{this.#busy=false;}
  }
  async #package(bytes,method) {
    sameOriginalIdentity(await this.#driver.identity(),this.#identity);
    const after=await this.#driver.flashMd5(0,PINS.flashBytes);
    const equal=after===this.#beforeMd5&&md5(bytes)===after;this.#diagnostics?.check('fullFlashMd5Match',equal);requireSafe(equal,'BACKUP_STALE');
    const result=await createRecoveryPackage(bytes,this.#romInfo,{...this.#consent,romMd5Before:this.#beforeMd5,romMd5After:after,method,build:this.#build,sourceCommit:this.#source});
    requireSafe(!this.#closed&&this.state!=='STOP','CANCELLED');this.#stage('SAVED_FILE_REIMPORT_REQUIRED');return result;
  }
  async useExistingRaw(input) {
    this.#available();requireSafe(this.#mode==='backup'&&this.state==='DEVICE_VERIFIED','STATE');
    requireSafe(input instanceof Uint8Array&&input.length===PINS.flashBytes&&md5(input)===this.#beforeMd5,'BACKUP_STALE');
    this.#busy=true;this.#capture=input.slice();
    try {return await this.#package(this.#capture,'ROM_MD5_MATCH_EXISTING_V1');}
    catch(error){this.#stop(error);throw error;}finally{this.#capture?.fill(0);this.#capture=undefined;this.#busy=false;}
  }
  async capture({progress=()=>{}}={}) {
    this.#available();requireSafe(this.#mode==='backup'&&this.state==='DEVICE_VERIFIED','STATE');this.#busy=true;this.#stage('READING_BACKUP');
    this.#capture=new Uint8Array(PINS.flashBytes);
    try {
      requireSafe(await this.#driver.flashMd5(0,PINS.flashBytes)===this.#beforeMd5,'BACKUP_STALE');
      for(let at=0;at<PINS.flashBytes;at+=65536) {
        const part=await this.#driver.readFlash(at,65536);requireSafe(part.length===65536,'READBACK');
        requireSafe(!this.#closed&&this.state!=='STOP','CANCELLED');this.#capture.set(part,at);part.fill(0);progress((at+65536)/PINS.flashBytes);
      }
      return await this.#package(this.#capture,'ROM_READ_SLOW_MD5_V1');
    } catch(error){this.#stop(error);throw error;}finally{this.#capture?.fill(0);this.#capture=undefined;this.#busy=false;}
  }
  async verifySavedPackage(input) {
    this.#available();requireSafe(this.#mode==='backup'&&['SAVED_FILE_REIMPORT_REQUIRED','DEVICE_VERIFIED'].includes(this.state),'STATE');this.#busy=true;let handle,communicating=false;
    try {
      handle=await validateRecoveryPackage(input);const {manifest,image}=recoverySnapshot(handle);image.fill(0);
      communicating=true;sameOriginalIdentity(await this.#driver.identity(),manifest.device);sameOriginalIdentity(this.#romInfo,manifest.device);
      requireSafe(await this.#driver.flashMd5(0,PINS.flashBytes)===manifest.image.md5&&manifest.image.md5===this.#beforeMd5,'BACKUP_STALE');
      this.#diagnostics?.check('packageValidated',true);this.#diagnostics?.check('savedPackageReimported',true);this.#diagnostics?.check('fullFlashMd5Match',true);this.#stage('BACKUP_SAVED_VERIFIED');
      return recoverySummary(handle);
    } catch(error){if(communicating)this.#stop(error);throw error;}finally {destroyRecoveryPackage(handle);this.#busy=false;}
  }
  async restore({restoreConfirmed,progress}={}) {
    this.#available();requireSafe(this.#mode==='restore'&&this.state==='READY_FOR_CONFIRMATION','STATE');requireSafe(restoreConfirmed===true,'RESTORE_CONFIRM');this.#busy=true;
    try {
      this.#stage('RECHECKING');await this.#driver.grantRecovery(this.#handle,{...this.#consent,restoreConfirmed});
      const result=await this.#driver.restoreRecovery({progress});this.#stage('VERIFIED_BOOT_PENDING');return result;
    } catch(error){this.#stop(error);throw error;}finally{this.#busy=false;}
  }
  async verifyAgain() {
    this.#available();requireSafe(this.#mode==='verify'&&this.state==='DEVICE_VERIFIED','STATE');this.#busy=true;
    try {this.#stage('VERIFYING');const result=await this.#driver.verifyRecoveryImage(this.#handle);this.#stage('VERIFIED_BOOT_PENDING');return result;}
    catch(error){this.#stop(error);throw error;}finally{this.#busy=false;}
  }
  #stop(error) {this.#diagnostics?.error(error);this.#stage('STOP');this.#driver.cancel();}
  cancel(code='CANCELLED') {if(!this.#closed&&this.state!=='STOP')this.#stop(new Stop(code));}
  async close() {
    if(this.#busy)this.cancel();this.#closed=true;
    try {await this.#driver.disconnect();}
    finally {destroyRecoveryPackage(this.#handle);this.#handle=undefined;this.#capture?.fill(0);this.#capture=undefined;this.#identity=undefined;this.#romInfo=undefined;this.#beforeMd5=undefined;this.#consent=undefined;this.#diagnostics?.check('cleanup',{sessionPrivateReferencesCleared:true,physicalMemoryErasureGuaranteed:false});}
  }
}
