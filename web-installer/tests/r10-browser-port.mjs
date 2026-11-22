// TEST ENVIRONMENT ONLY. No real serial API is referenced or retained.
import {RomDevicePort} from './rom-device.mjs';
import app from '../public/firmware/app.bin';
import boot from '../public/firmware/bootloader.bin';
import table from '../public/firmware/partitions.bin';
import ota from '../public/firmware/boot_app0.bin';
import factory from '../release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin';
const scenario=new URL(location.href).searchParams.get('scenario')||'factory';
let flash=new Uint8Array(16777216).fill(255);
if(scenario==='factory')flash.set(factory);else if(scenario==='update'){flash.set(boot);flash.set(table,32768);flash.set(ota,57344);flash.set(app,65536);}else if(scenario==='restore')flash.fill(0x5a);else throw Error('Explicit synthetic scenario required');
const port=new RomDevicePort(flash,{reservedFF:true,emptySlip:true,firstInstall:scenario==='factory',fullRecovery:scenario==='restore',yieldEvery:256});
let requests=0;
Object.defineProperty(navigator,'serial',{configurable:false,value:{requestPort:async()=>{requests++;return port;},getPorts:async()=>[]}});
const section=document.createElement('section');section.id='test-only-banner';section.style.cssText='background:#fff3cd;border:4px solid #900;padding:12px';section.innerHTML='<h2>SOFTWARE TEST ONLY · 独立合成端口 · 无真实 USB</h2><p>生产脚本字节原样加载；navigator.serial 仅返回合成设备。不要操作真实微雪或 TF。</p><pre id="test-wire"></pre>';document.body.prepend(section);
setInterval(()=>{document.getElementById('test-wire').textContent=JSON.stringify({synthetic:true,scenario,realUsbAvailable:false,requests,opens:port.openCount,closes:port.closeCount,resets:port.signalCount,readSlow:port.counts.get(14)||0,md5:port.counts.get(19)||0,flashBegin:port.counts.get(2)||0,flashData:port.counts.get(3)||0});},300);
