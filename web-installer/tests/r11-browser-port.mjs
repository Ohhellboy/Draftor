// Independent test environment only; never retains or calls native serial.
import {RomDevicePort} from './rom-device.mjs';
import app from '../public/firmware/app.bin';import boot from '../public/firmware/bootloader.bin';import table from '../public/firmware/partitions.bin';import ota from '../public/firmware/boot_app0.bin';import factory from '../release-inputs/factory/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin';
const query=new URL(location.href).searchParams,scenario=query.get('scenario')||'factory',fault=query.get('fault')||'';
let flash=new Uint8Array(16777216).fill(255);
if(scenario==='factory')flash.set(factory);else if(scenario==='update'){flash.set(boot);flash.set(table,32768);flash.set(ota,57344);flash.set(app,65536);}else if(scenario==='restore')flash.fill(0x5a);else if(scenario!=='blank')throw Error('Explicit synthetic scenario required');
if(fault==='factory-tail')flash[16777215]=0;if(fault==='unknown-app')flash[65580]^=1;
const opts={reservedFF:true,emptySlip:true,firstInstall:scenario==='factory',fullRecovery:scenario==='restore',yieldEvery:2048};
if(fault==='chip')opts.chipId=2;if(fault==='capacity')opts.jedec=0x1740ef;if(fault==='security')opts.securityFlags=1;if(fault==='mac')opts.macLo=2;if(fault==='partial')opts.throwOn={op:3,nth:5};if(fault==='corrupt-write')opts.corruptWrite=true;
const port=new RomDevicePort(flash,opts);if(fault==='wrong-usb')port.getInfo=()=>({usbVendorId:0x1234,usbProductId:0x1001});if(fault==='occupied')port.open=async()=>{port.openCount++;throw new DOMException('Synthetic occupied port','NetworkError');};
let requests=0,gesture=false;
Object.defineProperty(navigator,'serial',{configurable:false,value:{requestPort:async({filters})=>{requests++;gesture=navigator.userActivation.isActive;if(filters[0].usbVendorId!==0x303a||filters[0].usbProductId!==0x1001)throw Error('USB filter changed');if(fault==='permission')throw new DOMException('Synthetic policy denial','NotAllowedError');if(fault==='cancel')throw new DOMException('Synthetic cancel','NotFoundError');return port;},getPorts:async()=>[]}});
const box=document.createElement('section');box.id='test-only-banner';box.style.cssText='background:#fff3cd;border:3px solid #900;padding:8px';box.innerHTML='<h2>SOFTWARE TEST ONLY · 无真实 USB</h2><p>Native OS/browser; independent synthetic ROM. Production scripts unchanged.</p><pre id="test-wire"></pre>';document.body.prepend(box);
const show=()=>{document.getElementById('test-wire').textContent=JSON.stringify({synthetic:true,realUsbAvailable:false,scenario,fault,requests,userGesture:gesture,opens:port.openCount,closes:port.closeCount,resets:port.signalCount,readSlow:port.counts.get(14)||0,md5:port.counts.get(19)||0,flashBegin:port.counts.get(2)||0,flashData:port.counts.get(3)||0});};show();setInterval(show,200);
