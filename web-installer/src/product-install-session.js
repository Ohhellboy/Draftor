// SPDX-License-Identifier: Apache-2.0
import {InstallSession} from './session.js';
import {captureProtection,protectionBytes,confirmProtectionSaved,destroyProtection,savedProtection} from './install-protection.js';
import {requireSafe} from './safety.js';
export class ProductInstallSession {
 #install;#driver;#point;#stopped=false;#closed=false;#busy=false;#diag;#emit;#phase;#saved=false;
 constructor(driver,release,{diagnostics,onState=()=>{},platformQualified=false}={}){
  requireSafe(platformQualified,'PLATFORM');this.#driver=driver;this.#diag=diagnostics;this.#emit=onState;
  this.#install=new InstallSession(driver,release,{candidate:true,bindSelectedIdentity:true,productProtection:true,diagnostics,
   onState:(s,r)=>{this.#diag?.check('writeLock',!['ERASING','WRITING','VERIFYING'].includes(s));this.#emit(s,this.receipt(r));}});
 }
 get state(){return this.#stopped?'STOP':this.#phase||this.#install.state;}
 get plan(){return this.#install.plan;}
 receipt(r){return {...(r||this.#install?.receipt()||{}),simulation:this.#diag?.snapshot().simulation??this.#driver.kind==='simulation',status:this.state,protection:this.#saved?'SMALL_FILE_SAVED_READBACK_VERIFIED':'NOT_SAVED',fullBackup:'NOT_REQUIRED_NOT_CAPTURED',deviceQualification:'R12_DEVICE_PENDING',...(this.#diag?{diagnostics:this.#diag.snapshot()}: {})};}
 #alive(){requireSafe(!this.#closed&&!this.#stopped,'CANCELLED');}
 async #run(fn){this.#alive();requireSafe(!this.#busy,'BUSY');this.#busy=true;try{return await fn();}catch(e){this.cancel(e.code||'IO');throw e;}finally{this.#busy=false;}}
 async prepare({boardConfirmed,tfRemoved,factory}={}){return this.#run(async()=>{
  requireSafe(boardConfirmed===true&&tfRemoved===true,'BOARD_CONFIRM');await this.#install.prepare({boardConfirmed});
  requireSafe(this.plan.initialState!=='blank','UNQUALIFIED_BLANK');this.#alive();this.#phase='PROTECTING';this.#emit(this.#phase,this.receipt());
  this.#point=await captureProtection(this.#driver,this.plan,{factory,alive:()=>this.#alive()});this.#alive();
  this.#phase=undefined;this.#diag?.check('writeLock',true);this.#emit('PLAN_READY',this.receipt());return this.plan;
 });}
 protectionFile(){this.#alive();requireSafe(this.state==='PLAN_READY','STATE');return protectionBytes(this.#point);}
 async verifySavedProtection(bytes){return this.#run(async()=>{requireSafe(this.state==='PLAN_READY','STATE');this.#saved=false;await confirmProtectionSaved(this.#point,bytes);this.#alive();this.#saved=true;});}
 async execute({tfRemoved,planConfirmed,eraseConfirmed=false}={}){return this.#run(async()=>{
  this.#alive();requireSafe(this.state==='PLAN_READY'&&this.#saved,'PROTECTION_SAVE');
  savedProtection(this.#point,{mode:this.plan.mode,mac:this.#install.selectedIdentity});
  return this.#install.execute({tfRemoved,planConfirmed,eraseConfirmed,protection:this.#point});
 });}
 cancel(code='CANCELLED'){if(this.#closed)return;this.#stopped=true;this.#diag?.check('writeLock',true);destroyProtection(this.#point);this.#point=undefined;this.#install.cancel(code);}
 async close(){if(this.#closed)return;this.#closed=true;destroyProtection(this.#point);this.#point=undefined;this.#diag?.check('writeLock',true);await this.#install.close();}
}
