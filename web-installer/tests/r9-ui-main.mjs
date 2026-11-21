// SOFTWARE ONLY: no navigator.serial reference and no OS/USB enumeration.
import {startRecoveryUI} from '../src/recovery-ui.js';import {S3Rom} from '../src/rom.js';import {StrictSerialTransport} from '../src/serial.js';import {ReadOnlyDriver} from '../src/read-only-driver.js';
import {RomDevicePort} from './rom-device.mjs';import {le32,otaCrc} from '../src/safety.js';
import app from '../public/firmware/app.bin';import boot from '../public/firmware/bootloader.bin';import table from '../public/firmware/partitions.bin';
let flash;function original(){const b=new Uint8Array(16777216).fill(255),ota=new Uint8Array(8192).fill(255);ota.set(le32(1));ota.set(le32(2,otaCrc(ota.slice(0,4))),24);b.set(boot);b.set(table,0x8000);b.set(ota,0xe000);b.set(app,0x10000);b.set([78,86,83,192,219],0x9000);return b;}
startRecoveryUI({build:'R9-SOFTWARE-TEST-ONLY',sourceCommit:__R9_SOURCE__,simulation:true,recoveryEnabled:true,
 requestDriver:async(mode,diagnostics)=>{
  flash??=original();if(mode==='restore')flash.fill(0,0,0x420000);
  const port=new RomDevicePort(flash,{fullRecovery:true,emptySlip:true,reservedFF:true,yieldEvery:256});
  const wire=new StrictSerialTransport(port,{diagnostics});return mode==='restore'?new S3Rom(wire,{allowRecovery:true,diagnostics}):new ReadOnlyDriver(wire,{diagnostics});
 }});
