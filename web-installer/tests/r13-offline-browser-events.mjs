// SPDX-License-Identifier: Apache-2.0
// Synthetic test environment only. Records control order, never touches native USB.
import {RomDevicePort} from './rom-device.mjs';
const events=[];globalThis.__r13OfflineEvents=events;
const close=RomDevicePort.prototype.close;
RomDevicePort.prototype.close=function(...args){events.push({event:'SERIAL_CLOSE',time:performance.now()});return close.apply(this,args);};
const originalFetch=globalThis.fetch;
globalThis.fetch=function(url,options){
 if(new URL(url,location.href).pathname.includes('/offline/')){
  events.push({event:'HTTP_START',time:performance.now(),signalPresent:!!options?.signal});
  options?.signal?.addEventListener('abort',()=>events.push({event:'HTTP_ABORT',time:performance.now()}),{once:true});
 }
 return originalFetch.call(globalThis,url,options);
};
