// SPDX-License-Identifier: Apache-2.0
import {requireSafe,sha256} from './safety.js';
// Picker must be invoked by the click handler before any await. Closing the
// writable commits; only a new getFile() read constitutes saved-file evidence.
export function chooseProtectionFile(picker=globalThis.showSaveFilePicker){
 requireSafe(typeof picker==='function','SAVE_API');
 return picker.call(globalThis,{suggestedName:`Draftor-install-protection-${new Date().toISOString().replace(/[:.]/g,'-')}.json`,types:[{description:'Draftor private installation protection',accept:{'application/json':['.json']}}],excludeAcceptAllOption:true});
}
export async function saveAndReadBack(handle,bytes,{alive=()=>{}}={}){
 let stream,closed=false;try{alive();stream=await handle.createWritable();alive();await stream.write(bytes);alive();await stream.close();closed=true;alive();
  const file=await handle.getFile();requireSafe(file.size===bytes.length,'SAVE_READBACK');const saved=new Uint8Array(await file.arrayBuffer());alive();
  requireSafe(await sha256(saved)===await sha256(bytes),'SAVE_READBACK');return saved;
 }finally{if(stream&&!closed)try{await stream.abort();}catch{/* A failed/cancelled save never grants a writer. */}}
}
