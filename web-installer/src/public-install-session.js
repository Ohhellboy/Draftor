// SPDX-License-Identifier: Apache-2.0
import {InstallSession} from './session.js';
import {RecoverySession} from './recovery-session.js';
import {validateRecoveryPackage,recoverySnapshot,destroyRecoveryPackage,checkedIdentity,sameOriginalIdentity} from './recovery-package.js';
import {requireSafe,sha256,deepFreeze} from './safety.js';

function publicPlan(plan) {
  if(!plan)return undefined;
  const {candidateOnly,publicApproval,blankHardwareQualification,deviceQualification,...p}=structuredClone(plan);
  p.releaseScope='WAVESHARE_ST7305_CLEAN_INSTALL_OR_ALPHA3_APP0_REINSTALL';
  p.qualificationEvidence=p.mode==='fresh'?'GENERIC_CLEAN_SOFTWARE_TESTED_HARDWARE_PENDING':'R4_APP0_SAME_VERSION_INHERITED';
  p.runtimeWritePermission='NOT_GRANTED_BY_PLAN';
  return deepFreeze(p);
}

// Integration only: the existing InstallSession owns the port and remains the
// sole installation executor. R9's read-only package session borrows reads from
// that prepared connection; it neither reconnects nor owns/disconnects the port.
export class PublicInstallSession {
  #driver;#install;#backup;#identity;#diag;#emit;#build;#source;#toolHash;
  #busy=false;#closed=false;#stopped=false;#phase;#saved=false;#tool=false;
  constructor(driver,release,{diagnostics,onState=()=>{},build,sourceCommit,offlineToolSha256,platformQualified=false}={}) {
    requireSafe(platformQualified===true,'PLATFORM');
    requireSafe(/^[0-9a-f]{64}$/.test(offlineToolSha256),'ASSET');
    this.#driver=driver;this.#diag=diagnostics;this.#emit=onState;this.#build=build;this.#source=sourceCommit;this.#toolHash=offlineToolSha256;this.#diag?.check('writeLock',true);
    this.#install=new InstallSession(driver,release,{candidate:true,bindSelectedIdentity:true,allowCleanInstall:true,diagnostics,
      onState:(phase,receipt)=>{this.#diag?.check('writeLock',!['ERASING','WRITING','VERIFYING'].includes(phase));if(this.#install?.plan)this.#diag?.check('plan',this.plan);this.#emit(phase,this.receipt(receipt));}});
  }
  get state(){return this.#stopped?'STOP':this.#phase||this.#install.state;}
  get plan(){return publicPlan(this.#install.plan);}
  get selectedIdentity(){return this.#closed?undefined:this.#install.selectedIdentity;}
  get backupVerified(){return !this.#closed&&!this.#stopped&&(this.#saved||this.plan?.initialState==='blank')&&this.#install.backupVerified;}
  get offlineToolVerified(){return !this.#closed&&!this.#stopped&&this.#tool;}
  receipt(existing) {
    const r=existing||this.#install?.receipt()||{};
    return structuredClone({...r,plan:this.plan,backup:this.plan?.initialState==='blank'?'PROVEN_BLANK_NO_BACKUP_REQUIRED':this.#saved?'R9_SAVED_PACKAGE_REIMPORTED_AND_CURRENT_FLASH_MATCHED':r.backup,
      offlineTool:this.#tool?'EXACT_DISTRIBUTED_HTML_HASH_VERIFIED':'NOT_VERIFIED',
      qualificationEvidence:this.plan?.qualificationEvidence,deviceQualification:this.plan?.qualificationEvidence,
      ...(this.#diag?{diagnostics:this.#diag.snapshot()}:{}),status:this.state});
  }
  #ready(){requireSafe(!this.#closed&&!this.#stopped,'STATE');requireSafe(!this.#busy,'BUSY');}
  #phaseChanged(phase,progress){this.#phase=phase;this.#diag?.stage(phase);this.#emit(phase,{...this.receipt(),...(progress===undefined?{}:{backupProgress:progress})});}
  async prepare({boardConfirmed,tfRemoved}={}) {
    this.#ready();requireSafe(tfRemoved===true,'BACKUP');this.#busy=true;
    try {
      const p=await this.#install.prepare({boardConfirmed});
      requireSafe(p.hardwareWriteAllowed,'DEVICE_WRITES_LOCKED');
      this.#identity=checkedIdentity(await this.#driver.identity());
      const original=this.#identity,driver=this.#driver,owner=this.#install;
      const borrowedReads={kind:driver.kind,
        connect:async()=>requireSafe(owner.state==='PLAN_READY','STATE'),
        identity:async()=>{const info=await driver.identity();sameOriginalIdentity(info,original);return info;},
        readFlash:(...args)=>driver.readFlash(...args),flashMd5:(...args)=>driver.flashMd5(...args),
        cancel:()=>driver.cancel(),disconnect:async()=>{}};
      this.#backup=new RecoverySession(borrowedReads,{mode:'backup',diagnostics:this.#diag,build:this.#build,sourceCommit:this.#source});
      const state=await this.#backup.prepare({boardConfirmed,tfRemoved});
      requireSafe(state.firmware===(p.mode==='fresh'?p.initialState:'draftor-alpha3-app0'),'STALE_PLAN');
      this.#diag?.check('originalIdentityMatch',true);this.#diag?.check('plan',this.plan);this.#diag?.check('writeLock',true);
      this.#phaseChanged('PLAN_READY');return this.plan;
    } catch(e){this.cancel(e.code||'IO');throw e;}finally{this.#busy=false;}
  }
  async captureBackup() {
    this.#ready();requireSafe(this.state==='PLAN_READY'&&!this.#saved,'STATE');this.#busy=true;
    try {this.#phaseChanged('BACKUP_READING');const bytes=await this.#backup.capture({progress:p=>this.#phaseChanged('BACKUP_READING',p)});
      this.#phaseChanged('PLAN_READY');return bytes;
    } catch(e){this.cancel(e.code||'IO');throw e;}finally{this.#busy=false;}
  }
  async importExistingRaw(input) {
    this.#ready();requireSafe(this.state==='PLAN_READY'&&!this.#saved,'STATE');this.#busy=true;
    try {const bytes=await this.#backup.useExistingRaw(input);this.#phaseChanged('PLAN_READY');return bytes;}
    catch(e){if(this.#backup.state==='STOP')this.cancel(e.code||'IO');throw e;}finally{this.#busy=false;}
  }
  invalidateSavedPackage() {
    this.#saved=false;this.#diag?.check('savedPackageReimported','NOT_RUN');
    if(this.#busy||!['PLAN_READY','BACKUP_VERIFYING'].includes(this.state))this.cancel('PACKAGE_CHANGED');
  }
  async verifySavedBackup(input) {
    this.invalidateSavedPackage();this.#ready();requireSafe(this.state==='PLAN_READY','STATE');this.#busy=true;let h,image,communicating=false;
    try {
      h=await validateRecoveryPackage(input);requireSafe(!this.#closed&&!this.#stopped,'CANCELLED');const snap=recoverySnapshot(h);image=snap.image;
      this.#phaseChanged('BACKUP_VERIFYING');communicating=true;
      const info=await this.#driver.identity();sameOriginalIdentity(info,this.#identity);sameOriginalIdentity(info,snap.manifest.device);
      requireSafe(snap.manifest.original.firmware===(this.plan.mode==='fresh'?this.plan.initialState:'draftor-alpha3-app0'),'PACKAGE_METADATA');
      // Existing installation metadata/current-full-Flash checks stay intact.
      await this.#install.verifySavedBackup(image);
      sameOriginalIdentity(await this.#driver.identity(),snap.manifest.device);
      this.#saved=true;this.#diag?.check('packageValidated',true);this.#diag?.check('savedPackageReimported',true);this.#diag?.check('fullFlashMd5Match',true);this.#diag?.check('originalIdentityMatch',true);
      this.#phaseChanged('PLAN_READY');return true;
    } catch(e){if(communicating)this.cancel(e.code||'IO');throw e;}
    finally{image?.fill(0);destroyRecoveryPackage(h);this.#busy=false;}
  }
  async verifyOfflineTool(bytes) {
    this.#ready();this.#tool=false;
    requireSafe(bytes instanceof Uint8Array&&await sha256(bytes)===this.#toolHash,'RECOVERY_TOOL');
    requireSafe(!this.#closed&&!this.#stopped,'STATE');this.#tool=true;return true;
  }
  invalidateOfflineTool(){this.#tool=false;}
  async execute({offlineToolSaved,...consent}={}) {
    this.#ready();requireSafe(this.backupVerified,'BACKUP_FILE');requireSafe(this.plan?.initialState==='blank'||(this.#tool&&offlineToolSaved===true),'RECOVERY_TOOL');this.#busy=true;this.#phase=undefined;
    try {sameOriginalIdentity(await this.#driver.identity(),this.#identity);
      // No new write path: R7's exclusive grant + R4/R6 exact writes and checks.
      const result=await this.#install.execute(consent);return this.receipt(result);
    } catch(e){this.cancel(e.code||'IO');throw e;}finally{this.#busy=false;this.#saved=false;this.#tool=false;}
  }
  cancel(code='CANCELLED'){this.#diag?.check('writeLock',true);this.#stopped=true;this.#saved=false;this.#tool=false;this.#install.cancel(code);}
  async close(){if(this.#closed)return;this.#diag?.check('writeLock',true);this.#closed=true;this.#saved=false;this.#tool=false;this.#identity=undefined;try{await this.#backup?.close();}finally{await this.#install.close();this.#backup=undefined;}}
}
