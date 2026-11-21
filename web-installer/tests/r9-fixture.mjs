import { readFile } from 'node:fs/promises';
import { verifyReleaseInputs } from '../tools/verify-release.mjs';
import { SimulatedFlash } from '../src/simulator.js';
import { parseSecurity,S3Rom } from '../src/rom.js';
import { StrictSerialTransport } from '../src/serial.js';
import { Diagnostics } from '../src/diagnostics.js';
import { RomDevicePort } from './rom-device.mjs';
import { md5,le32 } from '../src/safety.js';
import { createRecoveryPackage } from '../src/recovery-package.js';
export const source='0e13454c93718159f5a116003f545611f425978f';
export const {release,inputs}=await verifyReleaseInputs({verifyDesignBrief:false});
const securityBytes=new Uint8Array(20);securityBytes.set(le32(9),12);
export const identity={...parseSecurity(securityBytes),mac:'02:00:00:00:00:01',flashBytes:16777216};
export function draftorFlash(){const b=new SimulatedFlash(release).flash;b.set(inputs['app.bin'],0x10000);b.set(inputs['bootloader.bin'],0);return b;}
export async function factoryFlash(){const b=new Uint8Array(16777216).fill(255);b.set(await readFile('release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin'));return b;}
export const packageFor=async b=>createRecoveryPackage(b,identity,{boardConfirmed:true,romMd5Before:md5(b),romMd5After:md5(b),method:'ROM_MD5_MATCH_EXISTING_V1',build:'R9_SYNTHETIC_TEST',sourceCommit:source});
export function realRomFixture(flash=draftorFlash(),options={},romOptions={allowRecovery:true}) {
 const diagnostics=new Diagnostics({simulation:true,mode:'recovery-test'});
 const port=new RomDevicePort(flash,{fullRecovery:true,reservedFF:true,emptySlip:true,...options});
 const wire=new StrictSerialTransport(port,{writeTimeout:100,closeTimeout:100,diagnostics});
 const rom=new S3Rom(wire,{...romOptions,diagnostics});return {port,wire,rom,diagnostics,flash};
}
