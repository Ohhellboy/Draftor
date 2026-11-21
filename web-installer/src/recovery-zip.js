// SPDX-License-Identifier: Apache-2.0
// Deliberately restricted ZIP profile: four ordered, stored entries, no extras,
// descriptors, compression, comments, encryption, ZIP64 or trailing bytes.
import { requireSafe, concat, bytesEqual } from './safety.js';
export const PACKAGE_NAMES=Object.freeze(['manifest.json','flash.bin','README.zh-CN.txt','README.en.txt']);
export const MAX_PACKAGE_BYTES=16777216+32768;
const te=new TextEncoder(), td=new TextDecoder('utf-8',{fatal:true});
const crcTable=Uint32Array.from({length:256},(_,n)=>{for(let i=0;i<8;i++)n=(n>>>1)^((n&1)?0xedb88320:0);return n>>>0;});
export function zipCrc(bytes) { let n=0xffffffff;for(const b of bytes)n=(n>>>8)^crcTable[(n^b)&255];return (n^0xffffffff)>>>0; }
const limit=(name,n)=>requireSafe(name==='flash.bin'?n===16777216:n>0&&n<=(name==='manifest.json'?12288:4096),'PACKAGE_SIZE');
export function storePackage(entries) {
  const local=[],central=[];let at=0;
  for(const name of PACKAGE_NAMES) {
    const data=entries[name];requireSafe(data instanceof Uint8Array,'PACKAGE_FORMAT');limit(name,data.length);
    const n=te.encode(name),crc=zipCrc(data),h=new Uint8Array(30+n.length),v=new DataView(h.buffer);
    v.setUint32(0,0x04034b50,true);v.setUint16(4,20,true);v.setUint16(12,33,true);
    v.setUint32(14,crc,true);v.setUint32(18,data.length,true);v.setUint32(22,data.length,true);v.setUint16(26,n.length,true);h.set(n,30);
    const c=new Uint8Array(46+n.length),w=new DataView(c.buffer);
    w.setUint32(0,0x02014b50,true);w.setUint16(4,20,true);w.setUint16(6,20,true);w.setUint16(14,33,true);
    w.setUint32(16,crc,true);w.setUint32(20,data.length,true);w.setUint32(24,data.length,true);w.setUint16(28,n.length,true);w.setUint32(42,at,true);c.set(n,46);
    local.push(h,data);central.push(c);at+=h.length+data.length;
  }
  const dir=concat(...central),end=new Uint8Array(22),v=new DataView(end.buffer);
  v.setUint32(0,0x06054b50,true);v.setUint16(8,4,true);v.setUint16(10,4,true);v.setUint32(12,dir.length,true);v.setUint32(16,at,true);
  return concat(...local,dir,end);
}
export function readPackageArchive(input) {
  requireSafe(input instanceof Uint8Array&&input.length>16777216&&input.length<=MAX_PACKAGE_BYTES,'PACKAGE_SIZE');
  const b=input,v=new DataView(b.buffer,b.byteOffset,b.byteLength),end=b.length-22;
  const u16=p=>v.getUint16(p,true),u32=p=>v.getUint32(p,true);
  requireSafe(u32(end)===0x06054b50&&u16(end+4)===0&&u16(end+6)===0&&u16(end+8)===4&&u16(end+10)===4&&u16(end+20)===0,'PACKAGE_FORMAT');
  const dir=u32(end+16),dirSize=u32(end+12);
  requireSafe(dir>0&&dir+dirSize===end&&dirSize<1024,'PACKAGE_FORMAT');
  let c=dir,at=0;const entries={};
  for(const expected of PACKAGE_NAMES) {
    requireSafe(c+46<=end&&u32(c)===0x02014b50,'PACKAGE_FORMAT');
    requireSafe(u16(c+4)===20&&u16(c+6)===20&&u16(c+8)===0&&u16(c+10)===0&&u16(c+12)===0&&u16(c+14)===33&&u16(c+30)===0&&u16(c+32)===0&&u16(c+34)===0&&u16(c+36)===0&&u32(c+38)===0,'PACKAGE_FORMAT');
    const size=u32(c+24),crc=u32(c+16),n=u16(c+28),off=u32(c+42);
    requireSafe(n===expected.length&&c+46+n<=end&&td.decode(b.subarray(c+46,c+46+n))===expected&&off===at,'PACKAGE_PATH');
    limit(expected,size);requireSafe(u32(c+20)===size&&at+30+n+size<=dir,'PACKAGE_SIZE');
    requireSafe(u32(at)===0x04034b50&&u16(at+4)===20&&u16(at+6)===0&&u16(at+8)===0&&u16(at+10)===0&&u16(at+12)===33&&u32(at+14)===crc&&u32(at+18)===size&&u32(at+22)===size&&u16(at+26)===n&&u16(at+28)===0&&bytesEqual(b.subarray(at+30,at+30+n),te.encode(expected)),'PACKAGE_FORMAT');
    const data=b.slice(at+30+n,at+30+n+size);requireSafe(zipCrc(data)===crc,'PACKAGE_CRC');entries[expected]=data;
    at+=30+n+size;c+=46+n;
  }
  requireSafe(at===dir&&c===end,'PACKAGE_FORMAT');return entries;
}
