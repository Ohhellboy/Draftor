// Only the browser UI fixture uses this adapter. Node fault tests use node:crypto.
import {md5} from '../src/safety.js';
export function createHash(kind){if(kind!=='md5')throw Error('Fixture digest');let bytes;return {update(b){bytes=b;return this;},digest(format){if(format!=='hex')throw Error('Fixture digest format');return md5(bytes);}};}
