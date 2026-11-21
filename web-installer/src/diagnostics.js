// SPDX-License-Identifier: Apache-2.0
// Program-recorded control-point counts, NOT an independent wire capture.
const OPS = {2:'FLASH_BEGIN',3:'FLASH_DATA',8:'SYNC',9:'WRITE_REG',10:'READ_REG',11:'SPI_PARAMS',13:'SPI_ATTACH',14:'READ_FLASH_SLOW',19:'MD5',20:'SECURITY'};
const EVENTS = ['requestPortAttempts','requestPortCancelled','requestPortErrors','openAttempts','openSuccess','openErrors','closeAttempts','closeSuccess','closeErrors','manualReconnects','cancelRequests','stubRequests','resetRequests','blockedWriteRequests'];
const CHECKS = ['chip','flashBytes','security','originalIdentityMatch','partitionRead','partitionMd5Match','otaRead','otaMd5Match','fullFlashMd5Match','packageValidated','savedPackageReimported','originalState','plan','writeLock','cleanup'];
export class Diagnostics {
  #data;
  constructor({build='source-development',sourceCommit='uncommitted',originType='test',origin='TEST',mode='read-only',simulation=false}={}) {
    this.#data = {schema:'draftor-diagnostics/1',schemaVersion:1,build,sourceCommit,originType,origin,simulation,mode,stage:'IDLE',startedAt:new Date().toISOString(),
      evidenceType:'PROGRAM_CONTROL_POINTS_NOT_INDEPENDENT_WIRE_CAPTURE',events:Object.fromEntries(EVENTS.map(k=>[k,0])),
      commands:Object.fromEntries(Object.values(OPS).map(k=>[k,{attempted:0,sent:0,acceptedReplies:0}])),pendingCommands:0,
      checks:Object.fromEntries(CHECKS.map(k=>[k,'NOT_RUN'])),firstError:null,lastError:null,closeResult:'NOT_RUN'};
  }
  event(name) { if (!EVENTS.includes(name)) throw Error('Unknown diagnostic event'); this.#data.events[name]++; }
  stage(stage) { if (!/^[A-Z_]{2,64}$/.test(stage)) throw Error('Invalid stage'); this.#data.stage=stage; }
  check(name,value) { if (!CHECKS.includes(name)) throw Error('Unknown diagnostic check'); this.#data.checks[name]=structuredClone(value); }
  command(op,step) { const row=this.#data.commands[OPS[op]]; if (!row || !['attempted','sent','acceptedReplies'].includes(step)) throw Error('Invalid counter'); row[step]++; }
  pending(delta) { this.#data.pendingCommands+=delta; }
  error(error) { const entry={code:/^[A-Z_0-9]{1,48}$/.test(error?.code||'')?error.code:'IO',stage:this.#data.stage};this.#data.firstError??=entry;this.#data.lastError=entry; }
  closed(ok) { this.#data.closeResult=ok?'CLOSED':'FAILED_POSSIBLY_OPEN'; }
  snapshot() { const d=this.#data;return structuredClone({...d,recordedAt:new Date().toISOString(),nextStep:d.closeResult==='FAILED_POSSIBLY_OPEN'?'SAVE_RECEIPT_CLOSE_TAB_CHECK_PORT_OWNER':d.stage==='STOP'?'PRESERVE_STATE_NO_AUTOMATIC_RETRY':d.stage==='VERIFIED_BOOT_PENDING'?'CLOSE_THEN_MANUAL_POWER_OFF_TF_AND_NORMAL_BOOT':'FOLLOW_VISIBLE_READ_ONLY_OR_CONFIRMATION_STEP'}); }
}

export async function requestSerialPort(serial, diagnostics) {
  diagnostics.event('requestPortAttempts');
  try { return await serial.requestPort({filters:[{usbVendorId:0x303a,usbProductId:0x1001}]}); }
  catch(error) { diagnostics.event(error?.name==='NotFoundError'?'requestPortCancelled':'requestPortErrors'); throw error; }
}
