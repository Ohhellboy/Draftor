import test from 'node:test';
import assert from 'node:assert/strict';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { InstallSession } from '../src/session.js';
import { SimulatedFlash, SIM_MAC } from '../src/simulator.js';
const { release } = await verifyReleaseInputs({verifyDesignBrief:false});
const approvals = {backupConfirmed:true,tfRemoved:true,planConfirmed:true};
function setup(options={}) {const d=new SimulatedFlash(release);return[d,new InstallSession(d,release,{bindSelectedIdentity:true,...options})];}
test('selected USB identity binds after checks, remains private and clears on close',async()=>{
 const[d,s]=setup();const p=await s.prepare({boardConfirmed:true});assert.equal(s.selectedIdentity,SIM_MAC.replaceAll(':',''));assert.equal(p.hardwareWriteAllowed,false);assert(!JSON.stringify(s.receipt()).includes(SIM_MAC));await s.close();assert.equal(s.selectedIdentity,undefined);
});
test('identity replacement after planning stops before any write',async()=>{
 const[d,s]=setup();await s.prepare({boardConfirmed:true});d.info.mac='02:00:00:00:00:02';await assert.rejects(s.execute(approvals),{code:'IDENTITY'});assert(!d.trace.some(t=>['write','eraseAll'].includes(t.op)));
});
for(const[field,value,code]of[['mac','invalid','IDENTITY'],['flashBytes',8388608,'FLASH'],['securityKnown',false,'SECURITY']])test(`auto binding does not weaken ${field}`,async()=>{
 const[d,s]=setup();d.info[field]=value;await assert.rejects(s.prepare({boardConfirmed:true}),{code});assert(!d.trace.some(t=>['write','eraseAll'].includes(t.op)));
});
test('real hardware default remains locked after binding',async()=>{
 const[d,s]=setup();d.kind='hardware';await s.prepare({boardConfirmed:true});await assert.rejects(s.execute(approvals),{code:'DEVICE_WRITES_LOCKED'});assert(!d.trace.some(t=>['write','eraseAll'].includes(t.op)));
});
test('original mutually exclusive capabilities remain enforced',()=>{const d=new SimulatedFlash(release);assert.throws(()=>new InstallSession(d,release,{hardwareWritesEnabled:true,hardwareFreshWritesEnabled:true}),{code:'BUILD_SCOPE'});});
