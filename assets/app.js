var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));

// node_modules/spark-md5/spark-md5.js
var require_spark_md5 = __commonJS({
  "node_modules/spark-md5/spark-md5.js"(exports, module) {
    (function(factory) {
      if (typeof exports === "object") {
        module.exports = factory();
      } else if (typeof define === "function" && define.amd) {
        define(factory);
      } else {
        var glob;
        try {
          glob = window;
        } catch (e) {
          glob = self;
        }
        glob.SparkMD5 = factory();
      }
    })(function(undefined2) {
      "use strict";
      var add32 = function(a, b) {
        return a + b & 4294967295;
      }, hex_chr = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"];
      function cmn(q, a, b, x, s, t) {
        a = add32(add32(a, q), add32(x, t));
        return add32(a << s | a >>> 32 - s, b);
      }
      function md5cycle(x, k) {
        var a = x[0], b = x[1], c = x[2], d = x[3];
        a += (b & c | ~b & d) + k[0] - 680876936 | 0;
        a = (a << 7 | a >>> 25) + b | 0;
        d += (a & b | ~a & c) + k[1] - 389564586 | 0;
        d = (d << 12 | d >>> 20) + a | 0;
        c += (d & a | ~d & b) + k[2] + 606105819 | 0;
        c = (c << 17 | c >>> 15) + d | 0;
        b += (c & d | ~c & a) + k[3] - 1044525330 | 0;
        b = (b << 22 | b >>> 10) + c | 0;
        a += (b & c | ~b & d) + k[4] - 176418897 | 0;
        a = (a << 7 | a >>> 25) + b | 0;
        d += (a & b | ~a & c) + k[5] + 1200080426 | 0;
        d = (d << 12 | d >>> 20) + a | 0;
        c += (d & a | ~d & b) + k[6] - 1473231341 | 0;
        c = (c << 17 | c >>> 15) + d | 0;
        b += (c & d | ~c & a) + k[7] - 45705983 | 0;
        b = (b << 22 | b >>> 10) + c | 0;
        a += (b & c | ~b & d) + k[8] + 1770035416 | 0;
        a = (a << 7 | a >>> 25) + b | 0;
        d += (a & b | ~a & c) + k[9] - 1958414417 | 0;
        d = (d << 12 | d >>> 20) + a | 0;
        c += (d & a | ~d & b) + k[10] - 42063 | 0;
        c = (c << 17 | c >>> 15) + d | 0;
        b += (c & d | ~c & a) + k[11] - 1990404162 | 0;
        b = (b << 22 | b >>> 10) + c | 0;
        a += (b & c | ~b & d) + k[12] + 1804603682 | 0;
        a = (a << 7 | a >>> 25) + b | 0;
        d += (a & b | ~a & c) + k[13] - 40341101 | 0;
        d = (d << 12 | d >>> 20) + a | 0;
        c += (d & a | ~d & b) + k[14] - 1502002290 | 0;
        c = (c << 17 | c >>> 15) + d | 0;
        b += (c & d | ~c & a) + k[15] + 1236535329 | 0;
        b = (b << 22 | b >>> 10) + c | 0;
        a += (b & d | c & ~d) + k[1] - 165796510 | 0;
        a = (a << 5 | a >>> 27) + b | 0;
        d += (a & c | b & ~c) + k[6] - 1069501632 | 0;
        d = (d << 9 | d >>> 23) + a | 0;
        c += (d & b | a & ~b) + k[11] + 643717713 | 0;
        c = (c << 14 | c >>> 18) + d | 0;
        b += (c & a | d & ~a) + k[0] - 373897302 | 0;
        b = (b << 20 | b >>> 12) + c | 0;
        a += (b & d | c & ~d) + k[5] - 701558691 | 0;
        a = (a << 5 | a >>> 27) + b | 0;
        d += (a & c | b & ~c) + k[10] + 38016083 | 0;
        d = (d << 9 | d >>> 23) + a | 0;
        c += (d & b | a & ~b) + k[15] - 660478335 | 0;
        c = (c << 14 | c >>> 18) + d | 0;
        b += (c & a | d & ~a) + k[4] - 405537848 | 0;
        b = (b << 20 | b >>> 12) + c | 0;
        a += (b & d | c & ~d) + k[9] + 568446438 | 0;
        a = (a << 5 | a >>> 27) + b | 0;
        d += (a & c | b & ~c) + k[14] - 1019803690 | 0;
        d = (d << 9 | d >>> 23) + a | 0;
        c += (d & b | a & ~b) + k[3] - 187363961 | 0;
        c = (c << 14 | c >>> 18) + d | 0;
        b += (c & a | d & ~a) + k[8] + 1163531501 | 0;
        b = (b << 20 | b >>> 12) + c | 0;
        a += (b & d | c & ~d) + k[13] - 1444681467 | 0;
        a = (a << 5 | a >>> 27) + b | 0;
        d += (a & c | b & ~c) + k[2] - 51403784 | 0;
        d = (d << 9 | d >>> 23) + a | 0;
        c += (d & b | a & ~b) + k[7] + 1735328473 | 0;
        c = (c << 14 | c >>> 18) + d | 0;
        b += (c & a | d & ~a) + k[12] - 1926607734 | 0;
        b = (b << 20 | b >>> 12) + c | 0;
        a += (b ^ c ^ d) + k[5] - 378558 | 0;
        a = (a << 4 | a >>> 28) + b | 0;
        d += (a ^ b ^ c) + k[8] - 2022574463 | 0;
        d = (d << 11 | d >>> 21) + a | 0;
        c += (d ^ a ^ b) + k[11] + 1839030562 | 0;
        c = (c << 16 | c >>> 16) + d | 0;
        b += (c ^ d ^ a) + k[14] - 35309556 | 0;
        b = (b << 23 | b >>> 9) + c | 0;
        a += (b ^ c ^ d) + k[1] - 1530992060 | 0;
        a = (a << 4 | a >>> 28) + b | 0;
        d += (a ^ b ^ c) + k[4] + 1272893353 | 0;
        d = (d << 11 | d >>> 21) + a | 0;
        c += (d ^ a ^ b) + k[7] - 155497632 | 0;
        c = (c << 16 | c >>> 16) + d | 0;
        b += (c ^ d ^ a) + k[10] - 1094730640 | 0;
        b = (b << 23 | b >>> 9) + c | 0;
        a += (b ^ c ^ d) + k[13] + 681279174 | 0;
        a = (a << 4 | a >>> 28) + b | 0;
        d += (a ^ b ^ c) + k[0] - 358537222 | 0;
        d = (d << 11 | d >>> 21) + a | 0;
        c += (d ^ a ^ b) + k[3] - 722521979 | 0;
        c = (c << 16 | c >>> 16) + d | 0;
        b += (c ^ d ^ a) + k[6] + 76029189 | 0;
        b = (b << 23 | b >>> 9) + c | 0;
        a += (b ^ c ^ d) + k[9] - 640364487 | 0;
        a = (a << 4 | a >>> 28) + b | 0;
        d += (a ^ b ^ c) + k[12] - 421815835 | 0;
        d = (d << 11 | d >>> 21) + a | 0;
        c += (d ^ a ^ b) + k[15] + 530742520 | 0;
        c = (c << 16 | c >>> 16) + d | 0;
        b += (c ^ d ^ a) + k[2] - 995338651 | 0;
        b = (b << 23 | b >>> 9) + c | 0;
        a += (c ^ (b | ~d)) + k[0] - 198630844 | 0;
        a = (a << 6 | a >>> 26) + b | 0;
        d += (b ^ (a | ~c)) + k[7] + 1126891415 | 0;
        d = (d << 10 | d >>> 22) + a | 0;
        c += (a ^ (d | ~b)) + k[14] - 1416354905 | 0;
        c = (c << 15 | c >>> 17) + d | 0;
        b += (d ^ (c | ~a)) + k[5] - 57434055 | 0;
        b = (b << 21 | b >>> 11) + c | 0;
        a += (c ^ (b | ~d)) + k[12] + 1700485571 | 0;
        a = (a << 6 | a >>> 26) + b | 0;
        d += (b ^ (a | ~c)) + k[3] - 1894986606 | 0;
        d = (d << 10 | d >>> 22) + a | 0;
        c += (a ^ (d | ~b)) + k[10] - 1051523 | 0;
        c = (c << 15 | c >>> 17) + d | 0;
        b += (d ^ (c | ~a)) + k[1] - 2054922799 | 0;
        b = (b << 21 | b >>> 11) + c | 0;
        a += (c ^ (b | ~d)) + k[8] + 1873313359 | 0;
        a = (a << 6 | a >>> 26) + b | 0;
        d += (b ^ (a | ~c)) + k[15] - 30611744 | 0;
        d = (d << 10 | d >>> 22) + a | 0;
        c += (a ^ (d | ~b)) + k[6] - 1560198380 | 0;
        c = (c << 15 | c >>> 17) + d | 0;
        b += (d ^ (c | ~a)) + k[13] + 1309151649 | 0;
        b = (b << 21 | b >>> 11) + c | 0;
        a += (c ^ (b | ~d)) + k[4] - 145523070 | 0;
        a = (a << 6 | a >>> 26) + b | 0;
        d += (b ^ (a | ~c)) + k[11] - 1120210379 | 0;
        d = (d << 10 | d >>> 22) + a | 0;
        c += (a ^ (d | ~b)) + k[2] + 718787259 | 0;
        c = (c << 15 | c >>> 17) + d | 0;
        b += (d ^ (c | ~a)) + k[9] - 343485551 | 0;
        b = (b << 21 | b >>> 11) + c | 0;
        x[0] = a + x[0] | 0;
        x[1] = b + x[1] | 0;
        x[2] = c + x[2] | 0;
        x[3] = d + x[3] | 0;
      }
      function md5blk(s) {
        var md5blks = [], i;
        for (i = 0; i < 64; i += 4) {
          md5blks[i >> 2] = s.charCodeAt(i) + (s.charCodeAt(i + 1) << 8) + (s.charCodeAt(i + 2) << 16) + (s.charCodeAt(i + 3) << 24);
        }
        return md5blks;
      }
      function md5blk_array(a) {
        var md5blks = [], i;
        for (i = 0; i < 64; i += 4) {
          md5blks[i >> 2] = a[i] + (a[i + 1] << 8) + (a[i + 2] << 16) + (a[i + 3] << 24);
        }
        return md5blks;
      }
      function md51(s) {
        var n = s.length, state = [1732584193, -271733879, -1732584194, 271733878], i, length, tail, tmp, lo, hi;
        for (i = 64; i <= n; i += 64) {
          md5cycle(state, md5blk(s.substring(i - 64, i)));
        }
        s = s.substring(i - 64);
        length = s.length;
        tail = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        for (i = 0; i < length; i += 1) {
          tail[i >> 2] |= s.charCodeAt(i) << (i % 4 << 3);
        }
        tail[i >> 2] |= 128 << (i % 4 << 3);
        if (i > 55) {
          md5cycle(state, tail);
          for (i = 0; i < 16; i += 1) {
            tail[i] = 0;
          }
        }
        tmp = n * 8;
        tmp = tmp.toString(16).match(/(.*?)(.{0,8})$/);
        lo = parseInt(tmp[2], 16);
        hi = parseInt(tmp[1], 16) || 0;
        tail[14] = lo;
        tail[15] = hi;
        md5cycle(state, tail);
        return state;
      }
      function md51_array(a) {
        var n = a.length, state = [1732584193, -271733879, -1732584194, 271733878], i, length, tail, tmp, lo, hi;
        for (i = 64; i <= n; i += 64) {
          md5cycle(state, md5blk_array(a.subarray(i - 64, i)));
        }
        a = i - 64 < n ? a.subarray(i - 64) : new Uint8Array(0);
        length = a.length;
        tail = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        for (i = 0; i < length; i += 1) {
          tail[i >> 2] |= a[i] << (i % 4 << 3);
        }
        tail[i >> 2] |= 128 << (i % 4 << 3);
        if (i > 55) {
          md5cycle(state, tail);
          for (i = 0; i < 16; i += 1) {
            tail[i] = 0;
          }
        }
        tmp = n * 8;
        tmp = tmp.toString(16).match(/(.*?)(.{0,8})$/);
        lo = parseInt(tmp[2], 16);
        hi = parseInt(tmp[1], 16) || 0;
        tail[14] = lo;
        tail[15] = hi;
        md5cycle(state, tail);
        return state;
      }
      function rhex(n) {
        var s = "", j;
        for (j = 0; j < 4; j += 1) {
          s += hex_chr[n >> j * 8 + 4 & 15] + hex_chr[n >> j * 8 & 15];
        }
        return s;
      }
      function hex2(x) {
        var i;
        for (i = 0; i < x.length; i += 1) {
          x[i] = rhex(x[i]);
        }
        return x.join("");
      }
      if (hex2(md51("hello")) !== "5d41402abc4b2a76b9719d911017c592") {
        add32 = function(x, y) {
          var lsw = (x & 65535) + (y & 65535), msw = (x >> 16) + (y >> 16) + (lsw >> 16);
          return msw << 16 | lsw & 65535;
        };
      }
      if (typeof ArrayBuffer !== "undefined" && !ArrayBuffer.prototype.slice) {
        (function() {
          function clamp(val, length) {
            val = val | 0 || 0;
            if (val < 0) {
              return Math.max(val + length, 0);
            }
            return Math.min(val, length);
          }
          ArrayBuffer.prototype.slice = function(from, to) {
            var length = this.byteLength, begin = clamp(from, length), end = length, num, target, targetArray, sourceArray;
            if (to !== undefined2) {
              end = clamp(to, length);
            }
            if (begin > end) {
              return new ArrayBuffer(0);
            }
            num = end - begin;
            target = new ArrayBuffer(num);
            targetArray = new Uint8Array(target);
            sourceArray = new Uint8Array(this, begin, num);
            targetArray.set(sourceArray);
            return target;
          };
        })();
      }
      function toUtf8(str) {
        if (/[\u0080-\uFFFF]/.test(str)) {
          str = unescape(encodeURIComponent(str));
        }
        return str;
      }
      function utf8Str2ArrayBuffer(str, returnUInt8Array) {
        var length = str.length, buff = new ArrayBuffer(length), arr = new Uint8Array(buff), i;
        for (i = 0; i < length; i += 1) {
          arr[i] = str.charCodeAt(i);
        }
        return returnUInt8Array ? arr : buff;
      }
      function arrayBuffer2Utf8Str(buff) {
        return String.fromCharCode.apply(null, new Uint8Array(buff));
      }
      function concatenateArrayBuffers(first, second, returnUInt8Array) {
        var result = new Uint8Array(first.byteLength + second.byteLength);
        result.set(new Uint8Array(first));
        result.set(new Uint8Array(second), first.byteLength);
        return returnUInt8Array ? result : result.buffer;
      }
      function hexToBinaryString(hex3) {
        var bytes = [], length = hex3.length, x;
        for (x = 0; x < length - 1; x += 2) {
          bytes.push(parseInt(hex3.substr(x, 2), 16));
        }
        return String.fromCharCode.apply(String, bytes);
      }
      function SparkMD52() {
        this.reset();
      }
      SparkMD52.prototype.append = function(str) {
        this.appendBinary(toUtf8(str));
        return this;
      };
      SparkMD52.prototype.appendBinary = function(contents) {
        this._buff += contents;
        this._length += contents.length;
        var length = this._buff.length, i;
        for (i = 64; i <= length; i += 64) {
          md5cycle(this._hash, md5blk(this._buff.substring(i - 64, i)));
        }
        this._buff = this._buff.substring(i - 64);
        return this;
      };
      SparkMD52.prototype.end = function(raw) {
        var buff = this._buff, length = buff.length, i, tail = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], ret;
        for (i = 0; i < length; i += 1) {
          tail[i >> 2] |= buff.charCodeAt(i) << (i % 4 << 3);
        }
        this._finish(tail, length);
        ret = hex2(this._hash);
        if (raw) {
          ret = hexToBinaryString(ret);
        }
        this.reset();
        return ret;
      };
      SparkMD52.prototype.reset = function() {
        this._buff = "";
        this._length = 0;
        this._hash = [1732584193, -271733879, -1732584194, 271733878];
        return this;
      };
      SparkMD52.prototype.getState = function() {
        return {
          buff: this._buff,
          length: this._length,
          hash: this._hash.slice()
        };
      };
      SparkMD52.prototype.setState = function(state) {
        this._buff = state.buff;
        this._length = state.length;
        this._hash = state.hash;
        return this;
      };
      SparkMD52.prototype.destroy = function() {
        delete this._hash;
        delete this._buff;
        delete this._length;
      };
      SparkMD52.prototype._finish = function(tail, length) {
        var i = length, tmp, lo, hi;
        tail[i >> 2] |= 128 << (i % 4 << 3);
        if (i > 55) {
          md5cycle(this._hash, tail);
          for (i = 0; i < 16; i += 1) {
            tail[i] = 0;
          }
        }
        tmp = this._length * 8;
        tmp = tmp.toString(16).match(/(.*?)(.{0,8})$/);
        lo = parseInt(tmp[2], 16);
        hi = parseInt(tmp[1], 16) || 0;
        tail[14] = lo;
        tail[15] = hi;
        md5cycle(this._hash, tail);
      };
      SparkMD52.hash = function(str, raw) {
        return SparkMD52.hashBinary(toUtf8(str), raw);
      };
      SparkMD52.hashBinary = function(content, raw) {
        var hash = md51(content), ret = hex2(hash);
        return raw ? hexToBinaryString(ret) : ret;
      };
      SparkMD52.ArrayBuffer = function() {
        this.reset();
      };
      SparkMD52.ArrayBuffer.prototype.append = function(arr) {
        var buff = concatenateArrayBuffers(this._buff.buffer, arr, true), length = buff.length, i;
        this._length += arr.byteLength;
        for (i = 64; i <= length; i += 64) {
          md5cycle(this._hash, md5blk_array(buff.subarray(i - 64, i)));
        }
        this._buff = i - 64 < length ? new Uint8Array(buff.buffer.slice(i - 64)) : new Uint8Array(0);
        return this;
      };
      SparkMD52.ArrayBuffer.prototype.end = function(raw) {
        var buff = this._buff, length = buff.length, tail = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], i, ret;
        for (i = 0; i < length; i += 1) {
          tail[i >> 2] |= buff[i] << (i % 4 << 3);
        }
        this._finish(tail, length);
        ret = hex2(this._hash);
        if (raw) {
          ret = hexToBinaryString(ret);
        }
        this.reset();
        return ret;
      };
      SparkMD52.ArrayBuffer.prototype.reset = function() {
        this._buff = new Uint8Array(0);
        this._length = 0;
        this._hash = [1732584193, -271733879, -1732584194, 271733878];
        return this;
      };
      SparkMD52.ArrayBuffer.prototype.getState = function() {
        var state = SparkMD52.prototype.getState.call(this);
        state.buff = arrayBuffer2Utf8Str(state.buff);
        return state;
      };
      SparkMD52.ArrayBuffer.prototype.setState = function(state) {
        state.buff = utf8Str2ArrayBuffer(state.buff, true);
        return SparkMD52.prototype.setState.call(this, state);
      };
      SparkMD52.ArrayBuffer.prototype.destroy = SparkMD52.prototype.destroy;
      SparkMD52.ArrayBuffer.prototype._finish = SparkMD52.prototype._finish;
      SparkMD52.ArrayBuffer.hash = function(arr, raw) {
        var hash = md51_array(new Uint8Array(arr)), ret = hex2(hash);
        return raw ? hexToBinaryString(ret) : ret;
      };
      return SparkMD52;
    });
  }
});

// src/content.js
var dict = {
  zh: {
    tfDownload: "\u4E0B\u8F7D TF \u5361\u8D44\u6E90",
    tfVersion: "\u9002\u7528\u4E8E\u5FAE\u96EA ESP32-S3-RLCD-4.2 \xB7 \u56FA\u4EF6 v0.1.0-alpha.3",
    tfIntro: "\u9996\u6B21\u51C6\u5907 TF \u5361\uFF0C\u8BF7\u4E0B\u8F7D\u4E0B\u65B9\u4E24\u4EFD\u8D44\u6E90\u5305\u3002\u5DF2\u7ECF\u5907\u597D\u5BF9\u5E94\u8D44\u6E90\u7684 TF \u5361\u53EF\u7EE7\u7EED\u4F7F\u7528\u3002",
    tfSizes: "\u57FA\u7840\u8D44\u6E90\u5305\u7EA6 60.5 MB\uFF0C\u5B66\u4E60\u7D22\u5F15\u5305\u7EA6 1.8 MB\uFF1B\u8FD0\u884C\u8D44\u6E90\u89E3\u538B\u540E\u5171\u7EA6 138.3 MB\u3002",
    tfPreserve: "\u4E0B\u8F7D\u540E\u6309\u76EE\u5F55\u8BF4\u660E\u590D\u5236\u5230 TF \u5361\uFF0C\u4FDD\u7559\u5DF2\u6709\u6587\u7A3F\u548C\u4E2A\u4EBA\u5B66\u4E60\u6570\u636E\u3002",
    tfBase: "\u4E0B\u8F7D\u57FA\u7840\u8D44\u6E90\u5305",
    tfIndex: "\u4E0B\u8F7D\u5B66\u4E60\u7D22\u5F15\u5305",
    tfDetails: "\u5B89\u88C5\u8BF4\u660E\u4E0E\u6821\u9A8C\u4FE1\u606F",
    tfVersions: "\u57FA\u7840\u5305\u6CBF\u7528 alpha.2.1\uFF0C\u4E24\u5305\u5171\u540C\u5BF9\u5E94\u5F53\u524D alpha.3 \u56FA\u4EF6\u3002\u5B66\u4E60\u7D22\u5F15\u662F\u516C\u5F00\u901A\u7528\u8D44\u6E90\uFF0C\u4E0D\u662F\u4E2A\u4EBA\u5B66\u4E60\u8BB0\u5F55\u3002\u4E0B\u8F7D\u6765\u81EA GitHub Release\uFF0C\u901F\u5EA6\u548C\u53EF\u7528\u6027\u53D6\u51B3\u4E8E\u7F51\u7EDC\u3002",
    tfMerge: "\u4EC5\u5408\u5E76\u9700\u8981\u7684\u8D44\u6E90\u5B50\u76EE\u5F55\uFF1B\u4E0D\u66FF\u6362\u6574\u4E2A HANIME\uFF0C\u4E0D\u683C\u5F0F\u5316\u3001\u4E0D\u6E05\u7A7A\u5361\u3002\u540C\u540D\u6587\u4EF6\u6709\u5DEE\u5F02\u65F6\uFF0C\u5148\u4FDD\u7559\u65E7\u6587\u4EF6\u5E76\u6838\u5BF9\u7248\u672C\uFF0C\u53EA\u5904\u7406\u53D1\u884C\u6E05\u5355\u4E2D\u7684\u8D44\u6E90\u3002",
    tfBasePath: "\u57FA\u7840\u5305\uFF1AResources/HANIME/ \u2192 \u5361\u6839 HANIME/",
    tfBaseBytes: "60,546,074 \u5B57\u8282",
    tfIndexPath: "\u5B66\u4E60\u7D22\u5F15\u5305\uFF1AResources/WAVESHARE_R7_L1_AREV1_RES_20261112/ \u2192 \u5361\u6839\u540C\u540D\u76EE\u5F55\uFF1Brelations.bgr \u548C relations.hnv \u5747\u9700\u8981\u3002",
    tfIndexBytes: "1,831,828 \u5B57\u8282",
    tfRoot: "\u4E0D\u8981\u628A ZIP \u5916\u5C42\u6587\u4EF6\u5939\u6216 Resources/ \u590D\u5236\u6210\u989D\u5916\u7236\u76EE\u5F55\u3002HANIME \u662F\u56FA\u4EF6\u517C\u5BB9\u8DEF\u5F84\uFF0C\u8BF7\u4FDD\u7559\u539F\u540D\u3002\u8FD0\u884C\u6E05\u5355\u5171 18 \u9879\u3001138,323,109 \u5B57\u8282\uFF0C\u4E0D\u542B\u6587\u7A3F\u548C\u4E2A\u4EBA\u5B66\u4E60\u6570\u636E\u3002",
    tfNoMigration: "\u4E0B\u8F7D\u8D44\u6E90\u4E0D\u4F1A\u81EA\u52A8\u8FC1\u79FB\u65E7\u6587\u7A3F\u6216\u5B66\u4E60\u8BB0\u5F55\uFF0C\u8BF7\u4FDD\u7559\u65E7\u547D\u540D\u7A7A\u95F4\u53CA\u6240\u6709\u5176\u4ED6\u6587\u4EF6\u3002\u7F51\u9875\u4E0D\u8BBF\u95EE\u6216\u6539\u5199 TF \u5361\u3002",
    tfFullGuide: "\u5B8C\u6574\u76EE\u5F55\u8BF4\u660E\u4E0E 18 \u9879\u6587\u4EF6\u6E05\u5355",
    tfManifest: "\u53D1\u884C\u8D44\u6E90\u6E05\u5355",
    hero: "\u8349\u7A3F\u673A",
    positioning: "\u65E0\u6253\u6270\u53E3\u888B\u4E2D\u6587\u5199\u4F5C",
    values: "\u5F00\u6E90\uFF0C\u65E0\u7F51\u7EDC\uFF0C\u65E0 AI\uFF0C\u65E0\u5E7F\u544A",
    installTitle: "\u5B89\u88C5 Draftor \u56FA\u4EF6",
    intro: "\u901A\u8FC7 USB \u8FDE\u63A5\u8BBE\u5907\uFF0C\u5373\u53EF\u5728\u6D4F\u89C8\u5668\u4E2D\u5B89\u88C5\u56FA\u4EF6\u3002\u65E0\u9700\u989D\u5916\u4E0B\u8F7D\u8F6F\u4EF6\u3002",
    boardLabel: "\u9002\u7528\u8BBE\u5907\uFF1A",
    change: "\u66F4\u6362",
    hide: "\u6536\u8D77",
    wsName: "\u5FAE\u96EA\u96C6\u6210\u7248",
    protoName: "Draftor S3 \u539F\u578B\u7248\uFF08\u6682\u672A\u5F00\u653E\uFF09",
    installBtn: "\u8FDE\u63A5\u8BBE\u5907\u5E76\u5B89\u88C5",
    browserHint: "\u8BF7\u4F7F\u7528\u7535\u8111\u7AEF Chrome \u6216 Edge \u6D4F\u89C8\u5668",
    previewHint: "\u7F51\u9875\u9884\u89C8\u7248 \xB7 \u5C1A\u672A\u63A5\u5165\u6B63\u5F0F\u70E7\u5F55\u7A0B\u5E8F",
    whatDraftor: "\u4EC0\u4E48\u662F\u8349\u7A3F\u673A",
    whatBgime: "\u4EC0\u4E48\u662F\u5927\u9E45\u8F93\u5165\u6CD5",
    help: "\u5B89\u88C5\u5E2E\u52A9",
    advanced: "\u9AD8\u7EA7\u9009\u9879",
    dialogs: {
      draftor: { title: "\u4EC0\u4E48\u662F\u8349\u7A3F\u673A", body: "<p><strong>Draftor\uFF08\u8349\u7A3F\u673A\uFF09</strong>\u662F\u4E00\u53F0\u4EE5\u4E2D\u6587\u8F93\u5165\u548C\u6301\u7EED\u5199\u4F5C\u4E3A\u4E2D\u5FC3\u7684\u8F7B\u91CF\u5199\u4F5C\u8BBE\u5907\u3002</p><p>\u5B83\u5F3A\u8C03<strong>\u65E0\u6253\u6270\u53E3\u888B\u4E2D\u6587\u5199\u4F5C</strong>\uFF1A\u5F00\u6E90\u3001\u5199\u4F5C\u65F6\u4E0D\u4F9D\u8D56\u7F51\u7EDC\u3001\u4E0D\u63A5\u5165 AI\uFF0C\u4E5F\u4E0D\u63D0\u4F9B\u5E7F\u544A\u3002</p><p>\u8FD9\u662F\u4E00\u53F0\u5199\u4F5C\u5DE5\u5177\uFF0C\u4E0D\u662F\u9700\u8981\u767B\u5F55\u8D26\u6237\u7684\u4E91\u7AEF\u5199\u4F5C\u5E73\u53F0\u3002</p>" },
      bgime: { title: "\u4EC0\u4E48\u662F\u5927\u9E45\u8F93\u5165\u6CD5", body: '<p><strong>bgIME\uFF08\u5927\u9E45\u8F93\u5165\u6CD5\uFF09</strong>\u662F Draftor \u4F7F\u7528\u7684\u5F00\u6E90\u4E2D\u6587\u8F93\u5165\u6CD5\u5F15\u64CE\u3002</p><p>\u5B83\u9762\u5411\u8D44\u6E90\u6709\u9650\u7684\u8BBE\u5907\uFF0C\u652F\u6301\u5168\u62FC\u3001\u53CC\u62FC\u548C\u4E94\u7B1486\u7B49\u8F93\u5165\u65B9\u6848\uFF0C\u5E76\u5C06\u8F93\u5165\u6CD5\u6838\u5FC3\u4E0E\u5177\u4F53\u8BBE\u5907\u754C\u9762\u5206\u5F00\u3002</p><p>\u9879\u76EE\u6E90\u7801\u53EF\u5728 <a href="https://github.com/Ohhellboy/Draftor" target="_blank" rel="noopener noreferrer">GitHub</a> \u67E5\u770B\u3002</p>' },
      help: { title: "\u5B89\u88C5\u5E2E\u52A9", body: '<p>\u51C6\u5907\u4E00\u53F0\u7535\u8111\u3001\u4E00\u6761\u652F\u6301\u6570\u636E\u4F20\u8F93\u7684 USB \u7EBF\uFF0C\u4EE5\u53CA\u5BF9\u5E94\u7684 Draftor \u8BBE\u5907\u3002</p><ul><li>\u8BF7\u4F7F\u7528\u652F\u6301 Web Serial \u7684\u684C\u9762\u7248 Chrome \u6216 Edge\u3002</li><li>\u672A\u6765\u6B63\u5F0F\u7248\u672C\u4F1A\u5728\u5199\u5165\u524D\u6838\u5BF9\u6240\u9009\u786C\u4EF6\u4E0E\u56FA\u4EF6\u8D44\u6E90\u3002</li><li>TF \u5361\u5185\u7684\u6587\u7A3F\u548C\u8BCD\u5E93\u4E0D\u5E94\u56E0\u56FA\u4EF6\u66F4\u65B0\u800C\u88AB\u6E05\u7A7A\uFF1B\u5B9E\u9645\u5199\u5165\u7B56\u7565\u987B\u5728\u70E7\u5F55\u7A0B\u5E8F\u63A5\u5165\u540E\u9A8C\u8BC1\u3002</li></ul><div class="notice">\u5F53\u524D\u53EA\u662F\u7F51\u9875\u754C\u9762\u9884\u89C8\uFF0C\u8FD8\u4E0D\u80FD\u70E7\u5199\u56FA\u4EF6\u3002</div>' },
      advanced: { title: "\u9AD8\u7EA7\u9009\u9879", body: "<p>\u6B64\u533A\u57DF\u9884\u7559\u7ED9\u56FA\u4EF6\u7248\u672C\u3001\u8BBE\u5907\u8BCA\u65AD\u4E0E\u6545\u969C\u6062\u590D\u8BF4\u660E\u3002\u666E\u901A\u7528\u6237\u4E0D\u9700\u8981\u8C03\u6574 Flash \u53C2\u6570\u3002</p><p>\u6B63\u5F0F\u70E7\u5F55\u5C06\u4F7F\u7528\u6BCF\u79CD\u786C\u4EF6\u5BF9\u5E94\u7684\u53D7\u63A7\u53D1\u5E03\u6E05\u5355\u3002\u5F53\u524D\u5C1A\u672A\u63D0\u4F9B\u5DF2\u9A8C\u8BC1\u6E05\u5355\uFF0C\u56E0\u6B64\u7981\u6B62\u76F4\u63A5\u5199\u5165\u8BBE\u5907\u3002</p>" },
      install: { title: "\u70E7\u5F55\u529F\u80FD\u5C1A\u672A\u63A5\u5165", body: '<p>\u8FD9\u662F Draftor Web Installer \u7684<strong>\u9996\u7248\u7F51\u9875\u754C\u9762</strong>\uFF0C\u5E76\u975E\u53EF\u7528\u7684\u56FA\u4EF6\u70E7\u5F55\u7A0B\u5E8F\u3002</p><p>\u9700\u8981\u5148\u53D6\u5F97\u5FAE\u96EA\u96C6\u6210\u677F\u7684\u6B63\u5F0F\u56FA\u4EF6\u3001\u5206\u533A\u5E03\u5C40\u3001\u53EF\u4FE1\u53D1\u5E03\u6E05\u5355\u548C\u6062\u590D\u6D41\u7A0B\uFF0C\u624D\u4F1A\u542F\u7528\u6D4F\u89C8\u5668\u70E7\u5F55\u3002</p><div class="notice">\u672A\u8FDE\u63A5\u8BBE\u5907\uFF0C\u4E5F\u6CA1\u6709\u8BFB\u53D6\u6216\u5199\u5165\u4EFB\u4F55 Flash\u3002</div>' }
    }
  },
  en: {
    tfDownload: "Download TF card resources",
    tfVersion: "Waveshare ESP32-S3-RLCD-4.2 \xB7 Firmware v0.1.0-alpha.3",
    tfIntro: "For a new TF card, download both archives below. A card with the matching resources can stay in use.",
    tfSizes: "Base resources: about 60.5 MB. Learning index: about 1.8 MB. Extracted runtime resources: about 138.3 MB.",
    tfPreserve: "Follow the directory guide when copying to TF. Keep existing documents and personal learning data.",
    tfBase: "Download base resources",
    tfIndex: "Download learning index",
    tfDetails: "Installation and checksums",
    tfVersions: "The base archive is retained from alpha.2.1; both archives together support the current alpha.3 firmware. The learning index is public generic data, not personal learning records. Downloads use GitHub Release; speed and availability depend on your network.",
    tfMerge: "Merge only the required resource subdirectories. Do not replace the entire HANIME directory, format or clear the card. If a same-name file differs, preserve it and check versions first; change only resources listed in the release manifest.",
    tfBasePath: "Base: Resources/HANIME/ \u2192 HANIME/ at the card root",
    tfBaseBytes: "60,546,074 bytes",
    tfIndexPath: "Learning index: Resources/WAVESHARE_R7_L1_AREV1_RES_20261112/ \u2192 the same directory at the card root; both relations.bgr and relations.hnv are required.",
    tfIndexBytes: "1,831,828 bytes",
    tfRoot: "Do not add the ZIP wrapper folder or Resources/ as an extra parent. HANIME is a firmware compatibility path; keep its name. The runtime manifest lists 18 files totaling 138,323,109 bytes, excluding documents and personal learning data.",
    tfNoMigration: "Downloading does not migrate old writing or learning records. Keep old namespaces and all other files. This page never accesses or writes to your TF card.",
    tfFullGuide: "Full directory guide and 18-file list",
    tfManifest: "Release resource manifest",
    hero: "Draftor",
    positioning: "Distraction-free pocket Chinese writing",
    values: "Open source. No network. No AI. No ads.",
    installTitle: "Install Draftor Firmware",
    intro: "Connect your device by USB and install firmware in your browser. No additional app needed.",
    boardLabel: "Device:",
    change: "Change",
    hide: "Hide",
    wsName: "Waveshare integrated board",
    protoName: "Draftor S3 prototype (not available yet)",
    installBtn: "Connect & Install",
    browserHint: "Use desktop Chrome or Edge",
    previewHint: "Website preview \xB7 Firmware flashing not yet connected",
    whatDraftor: "What is Draftor?",
    whatBgime: "What is bgIME?",
    help: "Installation help",
    advanced: "Advanced",
    dialogs: {
      draftor: { title: "What is Draftor?", body: "<p><strong>Draftor</strong> is a lightweight device focused on Chinese text input and uninterrupted writing.</p><p>Its emphasis is <strong>distraction-free, pocket-sized Chinese writing</strong>: open source, no network required while writing, no AI, and no advertisements.</p><p>It is a writing tool, not an account-based cloud writing platform.</p>" },
      bgime: { title: "What is bgIME?", body: '<p><strong>bgIME</strong> is the open-source Chinese input engine used by Draftor.</p><p>It is designed for resource-constrained devices and supports full Pinyin, double Pinyin, and Wubi 86.</p><p>Explore the source on <a href="https://github.com/Ohhellboy/Draftor" target="_blank" rel="noopener noreferrer">GitHub</a>.</p>' },
      help: { title: "Installation help", body: '<p>You will need a computer, a USB data cable, and a compatible Draftor device.</p><ul><li>Use desktop Chrome or Edge with Web Serial support.</li><li>A future release will verify firmware and device compatibility.</li><li>Document and dictionary data on the microSD card must be kept separate from firmware writes; the final procedure still requires validation.</li></ul><div class="notice">This is a website preview; firmware flashing is not available yet.</div>' },
      advanced: { title: "Advanced", body: "<p>This area is reserved for firmware versions, diagnostics, and recovery instructions. Normal users will not need Flash settings.</p><p>Flashing will require a verified board-specific release manifest. No manifest is available in this preview.</p>" },
      install: { title: "Firmware flashing not available yet", body: '<p>This is the <strong>first Draftor installer website preview</strong>, not a working firmware flasher.</p><p>The installer will be enabled after verified board-specific firmware, partition layout, release manifest, and recovery procedure are available.</p><div class="notice">No device has been connected or written.</div>' }
    }
  }
};

// src/r11-copy.js
var installationCopy = {
  "zh": {
    "browserHint": "Windows 10/11 \u684C\u9762 Chrome / Edge \u6D4B\u8BD5\u6027\u5F00\u653E\uFF1BmacOS Chrome \u4FDD\u7559\u652F\u6301\u3002",
    "wrongOriginal": "\u5907\u4EFD\u4E2D\u7684\u539F\u72B6\u6001\u4E0E\u5F53\u524D\u8BBE\u5907\u4E0D\u540C\u3002\u8BF7\u9009\u62E9\u672C\u6B21\u4ECE\u539F\u8BBE\u5907\u4FDD\u5B58\u7684\u5B8C\u6574 ZIP\u3002",
    "badPackage": "\u6062\u590D ZIP \u7684\u5185\u5BB9\u3001\u683C\u5F0F\u6216\u6458\u8981\u672A\u901A\u8FC7\u68C0\u67E5\uFF0C\u672A\u6388\u4E88\u5B89\u88C5\u6743\u9650\u3002\u8BF7\u4FDD\u7559\u539F\u6587\u4EF6\u5E76\u6838\u5BF9\u672C\u6B21\u751F\u6210\u7684\u5B8C\u6574\u5305\u3002",
    "stoppedHelp": "\u64E6\u5199\u5DF2\u5F00\u59CB\u540E\u4E2D\u65AD\u3002\u4FDD\u7559\u8BBE\u5907\u73B0\u72B6\u548C\u8131\u654F\u56DE\u6267\uFF0C\u5173\u95ED\u8FDE\u63A5\u3002\u7528\u521A\u4FDD\u5B58\u7684\u5B8C\u6574 ZIP \u548C\u79BB\u7EBF HTML \u6062\u590D\u539F\u8BBE\u5907\u3002",
    "previewHint": "v0.1.0-alpha.3 \xB7 \u5FAE\u96EA ST7305 \xB7 Windows Chrome / Edge \xB7 macOS Chrome",
    "intro": "\u8FDE\u63A5\u540E\u6838\u5BF9\u786C\u4EF6\u548C\u539F\u673A\u72B6\u6001\u3002\u53EF\u4FDD\u7559\u6570\u636E\u65F6\u63D0\u4F9B\u540C\u7248\u91CD\u88C5\uFF1B\u5176\u4ED6\u72B6\u6001\u53EF\u5907\u4EFD\u540E\u6E05\u7A7A\u5B89\u88C5\u3002",
    "publicScope": "\u652F\u6301\u5B9E\u7269\u786E\u8BA4\u7684\u5FAE\u96EA ESP32-S3-RLCD-4.2 / ST7305\u3001ESP32-S3 \u548C 16 MiB Flash\u3002\u5B89\u5168\u542F\u52A8\u6216\u52A0\u5BC6\u72B6\u6001\u4E0D\u652F\u6301\u65F6\u505C\u6B62\u3002\u65E7\u56FA\u4EF6\u672A\u8BC6\u522B\u3001\u672A\u77E5\u5206\u533A\u6216\u7A7A\u767D\u677F\u53EF\u6E05\u7A7A\u5B89\u88C5\u3002",
    "heading": "\u8FDE\u63A5\u8BBE\u5907\u5E76\u5B89\u88C5",
    "prepare": "\u4FDD\u5B58\u6587\u7A3F\u5E76\u7ED3\u675F\u4F20\u8F93\uFF0C\u5F7B\u5E95\u65AD\u7535\uFF08\u542B\u7535\u6C60\uFF09\u540E\u53D6\u51FA TF\uFF0C\u53EA\u8FDE\u63A5\u8FD9\u5757\u5FAE\u96EA\u3002Windows \u5DF2\u6210\u529F\u7684\u64CD\u4F5C\u662F\uFF1A\u8FDE\u63A5 USB \u2192 \u957F\u6309 PWR \u5173\u673A \u2192 \u6309\u4F4F BOOT \u2192 \u77ED\u6309 PWR \u5F00\u673A \u2192 \u7B49\u7EA6 2 \u79D2\u540E\u677E\u5F00 BOOT\u3002\u4FDD\u6301\u4E0B\u8F7D\u6A21\u5F0F\uFF0C\u5173\u95ED\u5176\u4ED6\u4E32\u53E3\u7A0B\u5E8F\u3002\u6D4F\u89C8\u5668\u9009\u62E9\u5BF9\u5E94 USB \u4E32\u884C\u8BBE\u5907\uFF08COM\u2026\uFF09\u6216 USB JTAG/serial debug unit\u3002",
    "board": "\u6211\u5DF2\u6838\u5BF9\u5B9E\u7269\u662F\u5FAE\u96EA ESP32-S3-RLCD-4.2 / ST7305\uFF0C\u4E14\u4EC5\u8BE5\u677F\u8FDE\u63A5\u3002",
    "tf": "\u6587\u7A3F\u5DF2\u4FDD\u5B58\u3001\u4F20\u8F93\u5DF2\u7ED3\u675F\uFF0CTF \u5DF2\u5728\u5F7B\u5E95\u65AD\u7535\u540E\u53D6\u51FA\u3002",
    "connect": "\u9009\u62E9\u4E32\u53E3\u5E76\u53EA\u8BFB\u68C0\u67E5",
    "unsupported": "\u9700\u8981\u684C\u9762\u5B89\u5168\u4E0A\u4E0B\u6587\u3001Web Serial \u4E0E\u7528\u6237\u6388\u6743\uFF1B\u8BF7\u67E5\u770B\u5F53\u524D\u80FD\u529B\u63D0\u793A\u3002\u672A\u8FDE\u63A5\u8BBE\u5907\u3002",
    "loading": "\u6B63\u5728\u6838\u9A8C\u56DB\u4EFD\u56FA\u5B9A\u53D1\u884C\u56FA\u4EF6\u2026",
    "ready": "\u56FA\u4EF6\u6587\u4EF6\u5DF2\u6838\u9A8C\uFF0C\u53EF\u4EE5\u9009\u62E9\u4E32\u53E3\u3002\u5C1A\u672A\u64E6\u9664\u6216\u5199\u5165\uFF0C\u539F\u673A Flash \u5185\u5BB9\u672A\u88AB\u672C\u6B21\u64CD\u4F5C\u4FEE\u6539\u3002",
    "update": "\u91CD\u65B0\u5B89\u88C5\u5F53\u524D\u7248\u672C",
    "fresh": "\u5907\u4EFD\u540E\u6E05\u7A7A\u5B89\u88C5 Draftor",
    "blankFresh": "\u5728\u7A7A\u767D\u8BBE\u5907\u4E0A\u5B89\u88C5 Draftor",
    "blankImpact": "\u5C06\u6E05\u7A7A\u5168\u90E8 16 MiB \u5185\u90E8 Flash \u5E76\u5B89\u88C5 Draftor\u3002\u6574\u7247\u5DF2\u6838\u5B9E\u4E3A\u7A7A\u767D\uFF0C\u6CA1\u6709\u9700\u8981\u4FDD\u7559\u7684\u539F\u673A\u5185\u5BB9\uFF1B\u4E0D\u8981\u6C42\u4FDD\u5B58\u5907\u4EFD\u3002TF \u5361\u72EC\u7ACB\u4FDD\u7BA1\u3002",
    "updateImpact": "\u53EA\u5199\u5DF2\u786E\u8BA4\u7684\u6D3B\u52A8 App \u53CA\u5176\u8986\u76D6\u6247\u533A\uFF0C\u4FDD\u7559\u5176\u4ED6 App\u3001\u5206\u533A\u3001OTA\u3001\u8BBE\u7F6E\u548C\u914D\u5BF9\u3002\u4E2D\u65AD\u53EF\u80FD\u65E0\u6CD5\u542F\u52A8\uFF1B\u4E0D\u4F1A\u81EA\u52A8\u91CD\u8BD5\u3002",
    "freshImpact": "\u6E05\u7A7A\u5168\u90E8 16 MiB \u5185\u90E8 Flash\uFF0C\u518D\u5B89\u88C5 Draftor\u3002\u539F\u56FA\u4EF6\u3001\u5185\u90E8\u6587\u7A3F\u3001\u8BBE\u7F6E\u548C\u914D\u5BF9\u4F1A\u88AB\u8986\u76D6\u3002\u5148\u4FDD\u5B58\u5E76\u56DE\u9009\u539F\u673A\u5B8C\u6574 ZIP\uFF0C\u7559\u5B58\u5BF9\u5E94\u79BB\u7EBF\u5DE5\u5177\u3002TF \u5361\u4E2D\u7684\u6587\u4EF6\u72EC\u7ACB\u4FDD\u7BA1\u3002",
    "plan": "\u5B8C\u6574\u8BA1\u5212\u4E0E\u68C0\u67E5\u8303\u56F4",
    "backupHelp": "\u6CA1\u6709\u65E7\u6587\u4EF6\u4E5F\u53EF\u7EE7\u7EED\uFF1A\u53EA\u8BFB\u6355\u83B7\u5B8C\u6574 16 MiB\uFF0C\u4FDD\u5B58\u751F\u6210\u7684\u79C1\u4EBA ZIP\uFF0C\u518D\u56DE\u9009\u5B9E\u9645\u4FDD\u5B58\u7684\u6587\u4EF6\u3002ROM \u6162\u8BFB\u9700\u8981\u8F83\u957F\u65F6\u95F4\uFF0C\u8BF7\u4FDD\u6301\u4F9B\u7535\u3001USB \u548C\u6B64\u9875\u3002\u5B8C\u6574 Flash \u5305\u542B\u8BBE\u7F6E\u3001\u914D\u5BF9\u53CA\u53EF\u80FD\u7684\u5185\u90E8\u6587\u7A3F\uFF1BTF \u6587\u7A3F\u4E0D\u5728\u5305\u5185\uFF0C\u987B\u5355\u72EC\u4FDD\u7BA1\u3002\u6587\u4EF6\u53EA\u5728\u672C\u673A\u5904\u7406\uFF0C\u4E0D\u4E0A\u4F20\u3002",
    "capture": "\u8BFB\u53D6\u5B8C\u6574 Flash\uFF08\u53EA\u8BFB\uFF09",
    "save": "\u4FDD\u5B58\u79C1\u4EBA\u6062\u590D ZIP",
    "raw": "\u53EF\u9009\uFF1A\u6211\u5DF2\u6709\u5B8C\u6574 16 MiB Flash .bin",
    "reselect": "\u56DE\u9009\u5DF2\u4FDD\u5B58\u7684\u79C1\u4EBA\u6062\u590D ZIP",
    "saved": "\u4FDD\u5B58\u6587\u4EF6\u56DE\u9009\u901A\u8FC7\uFF0C\u4E0E\u5F53\u524D\u540C\u4E00\u8BBE\u5907\u5B8C\u6574 Flash \u4E00\u81F4\u3002",
    "saveWarning": "\u70B9\u51FB\u4FDD\u5B58\u4E0D\u4EE3\u8868\u6587\u4EF6\u5DF2\u843D\u76D8\uFF1B\u5FC5\u987B\u7B49\u5F85\u4E0B\u8F7D\u5B8C\u6210\uFF0C\u518D\u56DE\u9009\u8BE5 ZIP\u3002\u8BF7\u52FF\u89E3\u538B\u3001\u4FEE\u6539\u6216\u516C\u5F00\u79C1\u4EBA\u5305\u3002",
    "tool": "\u4E0B\u8F7D\u79BB\u7EBF\u6062\u590D\u5DE5\u5177\uFF08HTML\uFF09",
    "toolKeep": "\u79BB\u7EBF HTML \u5DF2\u4FDD\u5B58\u5B8C\u6210\uFF0C\u6211\u4F1A\u5C06\u5B83\u4E0E\u79C1\u4EBA ZIP \u4E00\u8D77\u4FDD\u7BA1\u3002",
    "toolHelp": "\u4FDD\u5B58\u672C\u9875\u63D0\u4F9B\u7684 Draftor_Recovery_R13_Offline.html\uFF0C\u4E0E\u5B8C\u6574 ZIP \u653E\u5728\u4E00\u8D77\u3002\u6062\u590D\u65F6\u5173\u95ED\u7F51\u9875\uFF0C\u5728 Chrome / Edge \u6309 Ctrl+O\uFF08Mac \u6309 \u2318O\uFF09\u6253\u5F00\u8BE5 HTML\u3002\u6B64\u5DE5\u5177\u540C\u65F6\u652F\u6301\u539F\u6709\u5907\u4EFD\u548C\u65B0\u751F\u6210\u7684\u672A\u77E5\u56FA\u4EF6\u5B8C\u6574\u5907\u4EFD\u3002",
    "toolReady": "\u79BB\u7EBF\u5DE5\u5177\u5185\u5BB9\u6458\u8981\u5DF2\u6838\u9A8C\uFF1B\u8BF7\u786E\u8BA4\u6D4F\u89C8\u5668\u4FDD\u5B58\u5B8C\u6210\u3002",
    "toolProblem": "\u5DE5\u5177\u672A\u4FDD\u5B58\uFF0F\u4E0B\u8F7D\u6709\u95EE\u9898",
    "backupConsent": "\u6211\u5DF2\u4FDD\u5B58\u5E76\u56DE\u9009\u6709\u6548\u5B8C\u6574\u6062\u590D\u5305\uFF0C\u4E14\u5DF2\u53E6\u884C\u4FDD\u7BA1 TF \u6587\u7A3F\u548C\u5B66\u4E60\u76EE\u5F55\u3002",
    "planConsent": "\u6211\u786E\u8BA4\u91CD\u65B0\u5B89\u88C5\u5F53\u524D\u7248\u672C\uFF0C\u5E76\u4FDD\u7559\u9875\u9762\u8BF4\u660E\u7684\u79C1\u4EBA\u6570\u636E\u533A\u57DF\u3002",
    "eraseConsent": "\u6211\u5DF2\u9605\u8BFB\u4E0A\u8FF0\u5F71\u54CD\uFF0C\u786E\u8BA4\u6E05\u7A7A\u8FD9\u5757\u8BBE\u5907\u7684\u5168\u90E8\u5185\u90E8 Flash \u5E76\u5B89\u88C5 Draftor\u3002",
    "oldApp": "\u53EF\u9009\uFF1A\u51C6\u786E\u65E7 App .bin\uFF08\u672C\u5730\u6BD4\u5BF9\uFF0C\u4E0D\u4E0A\u4F20\uFF09",
    "execute": "\u6309\u5DF2\u6838\u9A8C\u8BA1\u5212\u5B89\u88C5",
    "close": "\u5173\u95ED\u8FDE\u63A5",
    "stop": "\u505C\u6B62\u64CD\u4F5C",
    "receipt": "\u4FDD\u5B58\u8131\u654F\u56DE\u6267",
    "finished": "Flash \u5185\u5BB9\u5DF2\u6821\u9A8C\uFF0C\u4E32\u53E3\u5DF2\u91CA\u653E\u3002\u8BF7\u5F7B\u5E95\u65AD\u7535\uFF08\u542B\u7535\u6C60\uFF09\uFF0C\u63D2\u56DE\u5DF2\u51C6\u5907\u597D\u7684 TF\uFF0C\u677E\u5F00 BOOT \u666E\u901A\u542F\u52A8\u3002\u9996\u88C5\u6309\u63D0\u793A\u914D\u5BF9\u952E\u76D8\uFF1B\u91CD\u65B0\u5B89\u88C5\u7528\u539F\u901A\u9053\u56DE\u8FDE\u3002\u68C0\u67E5\u663E\u793A\u3001\u8F93\u5165\u3001\u65E7\u7A3F\u4EE5\u53CA\u65B0\u7A3F\u4FDD\u5B58\u4E0E\u91CD\u542F\u8BFB\u53D6\u3002\u5185\u5BB9\u6821\u9A8C\u4E0D\u7B49\u4E8E\u6B63\u5E38\u542F\u52A8\u5DF2\u901A\u8FC7\u3002",
    "phases": {
      "IDLE": "\u5C1A\u672A\u8FDE\u63A5",
      "CONNECTING": "\u6B63\u5728\u8FDE\u63A5 ROM\u2026",
      "CHECKING": "\u6B63\u5728\u53EA\u8BFB\u6838\u5BF9\u8EAB\u4EFD\u3001Flash\u3001\u5206\u533A\u548C OTA\u2026",
      "PLAN_READY": "\u8BA1\u5212\u5DF2\u5C31\u7EEA\u3002\u5C1A\u672A\u64E6\u9664\u6216\u5199\u5165\uFF0C\u539F\u673A Flash \u5185\u5BB9\u672A\u88AB\u672C\u6B21\u64CD\u4F5C\u4FEE\u6539\u3002",
      "BACKUP_READING": "\u6B63\u5728\u53EA\u8BFB\u6355\u83B7\u5B8C\u6574 Flash\u2026",
      "BACKUP_VERIFYING": "\u6B63\u5728\u6838\u9A8C\u5B9E\u9645\u56DE\u9009\u6587\u4EF6\u4E0E\u5F53\u524D\u8BBE\u5907\u2026",
      "RECHECKING": "\u5199\u5165\u524D\u91CD\u65B0\u6838\u5BF9\u539F\u8BBE\u5907\u4E0E\u8BA1\u5212\u2026",
      "ERASING": "\u6B63\u5728\u6E05\u9664\u5DF2\u786E\u8BA4\u7684\u5185\u90E8 Flash\u2026",
      "WRITING": "\u6B63\u5728\u5199\u5165\u8BA1\u5212\u6307\u5B9A\u533A\u57DF\u2026",
      "VERIFYING": "\u6B63\u5728\u6838\u9A8C\u5B9E\u9645 Flash \u5185\u5BB9\u548C\u4FDD\u62A4\u533A\u2026",
      "VERIFIED_BOOT_PENDING": "\u5185\u5BB9\u5DF2\u6821\u9A8C\uFF0C\u666E\u901A\u542F\u52A8\u5F85\u786E\u8BA4\u3002",
      "STOP": "\u64CD\u4F5C\u5DF2\u505C\u6B62\u3002"
    },
    "helpTitle": "\u5B89\u88C5\u5E2E\u52A9",
    "advancedTitle": "\u53D1\u884C\u4FE1\u606F",
    "scopeLink": "\u652F\u6301\u8303\u56F4",
    "recoveryLink": "\u5907\u4EFD\u4E0E\u6062\u590D",
    "sourceLink": "\u6E90\u7801\u4E0E\u8BB8\u53EF",
    "unrecognized": "\u65E7\u56FA\u4EF6\u672A\u8BC6\u522B\uFF0C\u65E0\u6CD5\u786E\u8BA4\u4FDD\u7559\u6570\u636E\u91CD\u5237\u7684\u517C\u5BB9\u6027\u3002\u8BF7\u4FDD\u5B58\u539F\u673A\u5B8C\u6574\u5907\u4EFD\u540E\u6E05\u7A7A\u5B89\u88C5\u3002\u5C1A\u672A\u64E6\u9664\u6216\u5199\u5165\uFF0C\u539F\u673A Flash \u5185\u5BB9\u672A\u88AB\u672C\u6B21\u64CD\u4F5C\u4FEE\u6539\u3002",
    "blank": "\u5DF2\u6838\u5BF9\u5B8C\u6574 Flash \u5168\u90E8\u4E3A\u7A7A\u767D\uFF0C\u53EF\u8DF3\u8FC7\u5907\u4EFD\u548C\u56DE\u9009\u3002\u4ECD\u9700\u786E\u8BA4\u6E05\u7A7A\u5B89\u88C5\u3002",
    "knownFactory": "\u5DF2\u8BC6\u522B\u5382\u5BB6\u56FA\u4EF6\u3002\u5B8C\u6574\u5907\u4EFD\u5305\u542B\u5B9E\u9645\u539F\u673A\u5185\u5BB9\uFF0C\u6E05\u7A7A\u5B89\u88C5\u540E\u53EF\u7528\u8BE5\u5907\u4EFD\u6062\u590D\u3002",
    "knownDraftor": "\u5DF2\u8BC6\u522B\u53EF\u517C\u5BB9\u91CD\u88C5\u7684 Alpha 3\u3002\u53EF\u6309\u672C\u9875\u8BA1\u5212\u4FDD\u7559\u79C1\u4EBA\u6570\u636E\u533A\u57DF\u3002",
    "notModified": "\u5C1A\u672A\u64E6\u9664\u6216\u5199\u5165\uFF0C\u539F\u673A Flash \u5185\u5BB9\u672A\u88AB\u672C\u6B21\u64CD\u4F5C\u4FEE\u6539\u3002"
  },
  "en": {
    "browserHint": "Windows 10/11 desktop Chrome / Edge beta; macOS Chrome remains supported.",
    "wrongOriginal": "The backup original state differs from the current board. Select the full ZIP saved from this device during this operation.",
    "badPackage": "Recovery ZIP contents, format or hashes failed validation. No installation permission granted. Keep the file and check the complete package created for this session.",
    "stoppedHelp": "Interrupted after erasing or writing began. Keep the device state and sanitized receipt, then close the connection. Use the saved full ZIP and offline HTML to restore the original board.",
    "previewHint": "v0.1.0-alpha.3 \xB7 Waveshare ST7305 \xB7 Windows Chrome / Edge \xB7 macOS Chrome",
    "intro": "Check the hardware and original contents. Compatible firmware can be reinstalled with data preserved. Other states can be backed up and erased for a clean installation.",
    "publicScope": "Physically confirmed Waveshare ESP32-S3-RLCD-4.2 / ST7305 with ESP32-S3 and 16 MiB Flash. Unsupported security or encryption stops the operation. Unrecognized firmware, layouts and blank Flash are eligible for clean installation.",
    "heading": "Connect & Install",
    "prepare": "Save writing and finish transfers. Power off completely, including the battery, and remove TF. Connect only this Waveshare. On Windows use USB connected \u2192 hold PWR to power off \u2192 hold BOOT \u2192 briefly press PWR to power on \u2192 release BOOT after about two seconds. Close serial monitors. Select the corresponding USB Serial Device (COM\u2026) or USB JTAG/serial debug unit.",
    "board": "I checked the physical Waveshare ESP32-S3-RLCD-4.2 / ST7305; only this board is connected.",
    "tf": "Writing is saved, transfers ended, and TF was removed after full power-off.",
    "connect": "Select port & inspect read-only",
    "unsupported": "Desktop secure Web Serial and user permission are required. See the capability message. No device connected.",
    "loading": "Verifying the four pinned release images\u2026",
    "ready": "Firmware files verified. Ready to select a port. No erase or write has started.",
    "update": "Reinstall the current version",
    "fresh": "Back up and clean-install Draftor",
    "blankFresh": "Install Draftor on a blank device",
    "blankImpact": "Erase all 16 MiB of internal Flash and install Draftor. The entire Flash is verified blank, so there are no original contents to save and no backup is required. Keep TF separate.",
    "updateImpact": "Write only the confirmed active App and its covered sectors. Preserve the other App, partitions, OTA, settings and pairing. Interruption may prevent boot. No automatic retry.",
    "freshImpact": "Erase all 16 MiB of internal Flash and install Draftor. Existing firmware, internal documents, settings and pairing will be overwritten. Save and reselect a full original ZIP and keep the matching offline tool. Preserve TF files separately.",
    "plan": "Complete plan and checked ranges",
    "backupHelp": "No old files needed: capture all 16 MiB read-only, save the private ZIP, then reselect the actual saved file. ROM slow reading takes time; keep power, USB and this page connected. Full Flash includes settings, pairing and possibly internal writing. TF documents are separate: keep them independently. Files stay local and are never uploaded.",
    "capture": "Read complete Flash (read-only)",
    "save": "Save private recovery ZIP",
    "raw": "Optional: I have a complete 16 MiB Flash .bin",
    "reselect": "Reselect saved private recovery ZIP",
    "saved": "Saved file verified against the same device\u2019s entire current Flash.",
    "saveWarning": "Clicking Save does not prove the file reached disk. Wait for download completion, then reselect that ZIP. Do not extract, modify or publish the private package.",
    "tool": "Download offline recovery tool (HTML)",
    "toolKeep": "The offline HTML has finished saving; I will keep it with my private ZIP.",
    "toolHelp": "Save Draftor_Recovery_R13_Offline.html beside your full ZIP. To restore, close the website and open that HTML using Ctrl+O in Chrome / Edge (Command+O on Mac). It reads existing packages and the new full backups of unrecognized firmware.",
    "toolReady": "Offline tool contents verified. Confirm the browser finished saving.",
    "toolProblem": "Tool was not saved / download problem",
    "backupConsent": "I saved and reselected a valid full recovery package, and separately retained TF writing and learning directories.",
    "planConsent": "I confirm this same-version reinstall and preservation of the stated private data regions.",
    "eraseConsent": "I understand this will erase all internal Flash on this device and install Draftor, and confirm this operation.",
    "oldApp": "Optional: exact old App .bin (local comparison, never uploaded)",
    "execute": "Install the verified plan",
    "close": "Close connection",
    "stop": "Stop operation",
    "receipt": "Save sanitized receipt",
    "finished": "Flash contents verified and port released. Fully power off including the battery, insert the prepared TF and boot normally with BOOT released. Pair a keyboard after first installation; use the original channel after reinstallation. Check display, input, old writing, saving a new document and reading it after restart. Content verification does not prove normal boot.",
    "phases": {
      "IDLE": "Not connected",
      "CONNECTING": "Connecting to ROM\u2026",
      "CHECKING": "Checking identity, Flash, partitions and OTA read-only\u2026",
      "PLAN_READY": "Plan ready. No erase or write has started; original Flash is unmodified.",
      "BACKUP_READING": "Capturing complete Flash read-only\u2026",
      "BACKUP_VERIFYING": "Checking the reselected file against the current device\u2026",
      "RECHECKING": "Rechecking the original device and plan\u2026",
      "ERASING": "Erasing the confirmed internal Flash\u2026",
      "WRITING": "Writing the planned ranges\u2026",
      "VERIFYING": "Verifying actual Flash and protected regions\u2026",
      "VERIFIED_BOOT_PENDING": "Contents verified; normal boot pending.",
      "STOP": "Operation stopped."
    },
    "helpTitle": "Installation help",
    "advancedTitle": "Release information",
    "scopeLink": "Supported scope",
    "recoveryLink": "Backup & recovery",
    "sourceLink": "Source & licenses",
    "unrecognized": "Original firmware is unrecognized. Data-preserving reinstall compatibility could not be established. Save the complete original backup and proceed with a clean installation. No erase or write has started.",
    "blank": "The complete Flash is verified blank. Backup and reimport can be skipped. Confirm clean installation to proceed.",
    "knownFactory": "Factory firmware recognized. The full backup holds the actual original contents for restoring this board.",
    "knownDraftor": "Compatible Alpha 3 recognized. The stated private regions will be preserved.",
    "notModified": "No erase or write has started. This operation has not modified the original Flash."
  }
};

// src/pins.js
var PINS = {
  "version": "v0.1.0-alpha.3",
  "commit": "f18939f8880e2278c231739d553fbf983c19e0f6",
  "tagObject": "36366a585dc8151b28463d0d78d0d11ce5ccbacf",
  "board": "WAVESHARE_RLCD42",
  "controller": "ST7305",
  "chip": "esp32s3",
  "flashBytes": 16777216,
  "slotCapacity": 4194304,
  "label": "WAVESHARE_A3_BLE_REWRITE_R3_ROT180_TEST_ONLY",
  "files": {
    "app.bin": {
      "offset": 65536,
      "bytes": 4021152,
      "sha256": "165782a52c59f677579f92a0b35e950eed5f041df349813c37bff1e9ad5680bd",
      "md5": "21695d723c88f0d43e84ae33f8eb848b"
    },
    "bootloader.bin": {
      "offset": 0,
      "bytes": 19968,
      "sha256": "31b3c1be45dc5a76aa85c82540d6787b675e711eaf11021a5eea7e36f469c6de",
      "md5": "51ea0a64aa9692776750b237b2a34821"
    },
    "partitions.bin": {
      "offset": 32768,
      "bytes": 3072,
      "sha256": "a172a8ae4cd0db329d6de2635991e3f6e3feb374e0ee9dcab78d9d7b360df6c9",
      "md5": "c0dc7c8326c9350832563a1aeb6abc13"
    },
    "boot_app0.bin": {
      "offset": 57344,
      "bytes": 8192,
      "sha256": "f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0",
      "md5": "e6327541e2dc394ca2c3b3280ac0f39f"
    }
  },
  "documents": {
    "FIRMWARE_MANIFEST.json": {
      "bytes": 1360,
      "sha256": "9a12078c1f47c92da7f46dfd4d896a7bfa4e36bec1f0824b400e8ed60ef3b7e0"
    },
    "WAVESHARE_RLCD42.json": {
      "bytes": 1360,
      "sha256": "9a12078c1f47c92da7f46dfd4d896a7bfa4e36bec1f0824b400e8ed60ef3b7e0"
    },
    "RELEASE_PAIR.json": {
      "bytes": 4207,
      "sha256": "1aae2305882ff3a231f2d94bb7a660aeb3d6e9e0bfa502536d974b56025f9400"
    },
    "TF_RESOURCES.json": {
      "bytes": 5400,
      "sha256": "215f0b2fb905b0a7f24d7c2d085a6480711f2e7fe27ffb30ab6c1c9d840fe844"
    }
  },
  "archives": {
    "Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Firmware.zip": {
      "bytes": 2913395,
      "sha256": "626fb11222281dc4351d2de71ff9a36422f4aeca487e3475484b8bd55315699a"
    },
    "Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip": {
      "bytes": 1831828,
      "sha256": "05da4128d132c0673ae28bf9a1f35d3997a017cd1995d51d38051dc15f55c3eb"
    },
    "Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_MANIFEST.json": {
      "bytes": 2383,
      "sha256": "50dc7c11e4097bb601cc658cbccc986f4fbca4cfef3633840e46696e59f5b548"
    },
    "SHA256SUMS": {
      "bytes": 480,
      "sha256": "42bbf75474f1590d6d514cbf6e6ad73b694e4b87218c6625ee046a273f1ff27b"
    },
    "Draftor_Web_Installer_v0_1_repacked.zip": {
      "bytes": 8240,
      "sha256": "702d71dbe462d7944a0370fcff81326fe522a4fd23e9eb176b5f891aec622a28"
    },
    "BRIEF.md": {
      "bytes": 9206,
      "sha256": "0ccf2accb5d8fa85a069d7271e556180d684e198e37fe75819212a5e0f05ccfc"
    },
    "CONTENTS.json": {
      "bytes": 6972,
      "sha256": "4b9347529a4c2561f72ccdcbb62a50a2a9a707deca00c9b66a9e68a5df07cb90"
    }
  },
  "blankFlashMd5": "d5929b7a5ad99f511fc83d7d0b48b85f"
};

// src/safety.js
var import_spark_md5 = __toESM(require_spark_md5(), 1);
var Stop = class extends Error {
  constructor(code) {
    super(code);
    this.name = "Stop";
    this.code = code;
  }
};
function requireSafe(ok, code) {
  if (!ok) throw new Stop(code);
}
var hex = (n) => `0x${n.toString(16)}`;
var bytesEqual = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
var u32 = (a, offset = 0) => new DataView(a.buffer, a.byteOffset, a.byteLength).getUint32(offset, true);
var le32 = (...values) => {
  const out = new Uint8Array(values.length * 4), view = new DataView(out.buffer);
  values.forEach((value, i) => view.setUint32(i * 4, value, true));
  return out;
};
function concat(...arrays) {
  const out = new Uint8Array(arrays.reduce((n, a) => n + a.length, 0));
  let at = 0;
  for (const a of arrays) {
    out.set(a, at);
    at += a.length;
  }
  return out;
}
async function sha256(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest), (n) => n.toString(16).padStart(2, "0")).join("");
}
var md5 = (bytes) => import_spark_md5.default.ArrayBuffer.hash(bytes.slice().buffer);
function deepFreeze(object) {
  for (const value of Object.values(object)) if (value && typeof value === "object") deepFreeze(value);
  return Object.freeze(object);
}
function normalizeMac(value) {
  const out = String(value ?? "").toLowerCase().replace(/[:-]/g, "");
  requireSafe(/^[0-9a-f]{12}$/.test(out) && out !== "000000000000" && out !== "ffffffffffff", "IDENTITY");
  return out;
}
function otaCrc(data) {
  let crc = 0;
  for (const value of data) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit++) crc = crc >>> 1 ^ (crc & 1 ? 3988292384 : 0);
  }
  return (crc ^ 4294967295) >>> 0;
}
var PARTITIONS = deepFreeze([
  ["nvs", 1, 2, 36864, 20480],
  ["otadata", 1, 0, 57344, 8192],
  ["app0", 0, 16, 65536, 4194304],
  ["app1", 0, 17, 4259840, 4194304],
  ["ime_ro", 64, 0, 8454144, 2097152],
  ["ffat", 1, 129, 10551296, 6160384],
  ["coredump", 1, 3, 16711680, 65536]
]);
function checkPartitions(data) {
  requireSafe([3072, 4096].includes(data.length), "PARTITIONS");
  const rows = [];
  for (let at = 0; at < 3072; at += 32) {
    const magic = data[at] | data[at + 1] << 8;
    if (magic === 65535 || magic === 60395) break;
    requireSafe(magic === 20650 && u32(data, at + 28) === 0, "PARTITIONS");
    const name = new TextDecoder("ascii").decode(data.slice(at + 12, at + 28)).split("\0")[0];
    rows.push([name, data[at + 2], data[at + 3], u32(data, at + 4), u32(data, at + 8)]);
  }
  requireSafe(JSON.stringify(rows) === JSON.stringify(PARTITIONS), "PARTITIONS");
  return rows;
}
function activeSlot(data) {
  requireSafe(data.length === 8192, "OTA");
  const entries = [];
  for (const at of [0, 4096]) {
    const entry = data.slice(at, at + 32);
    if (entry.every((x) => x === 255) || entry.slice(0, 4).every((x) => x === 0) && entry.slice(4).every((x) => x === 255)) continue;
    const seq = u32(entry), state = u32(entry, 24);
    requireSafe(seq > 0 && seq < 4294967295 && [2, 4294967295].includes(state) && u32(entry, 28) === otaCrc(entry.slice(0, 4)), "OTA");
    entries.push(seq);
  }
  requireSafe(entries.length === 1, "OTA");
  return (entries[0] - 1) % 2;
}

// src/firmware.js
deepFreeze(PINS);
var verified = /* @__PURE__ */ new WeakMap();
var releaseIsVerified = (release) => verified.has(release);
function releaseFile(release, name) {
  requireSafe(verified.has(release) && PINS.files[name], "ASSET");
  return verified.get(release)[name].slice();
}
async function verifyRelease(inputs) {
  const files = {};
  for (const [name, pin] of Object.entries({ ...PINS.documents, ...PINS.files })) {
    const bytes = inputs[name];
    requireSafe(bytes instanceof Uint8Array && bytes.length === pin.bytes, "ASSET");
    const copy2 = bytes.slice();
    requireSafe(await sha256(copy2) === pin.sha256, "ASSET");
    if (PINS.files[name]) {
      requireSafe(md5(copy2) === pin.md5, "ASSET");
      files[name] = copy2;
    }
  }
  const manifest = JSON.parse(new TextDecoder().decode(inputs["FIRMWARE_MANIFEST.json"]));
  const pair = JSON.parse(new TextDecoder().decode(inputs["RELEASE_PAIR.json"]));
  requireSafe(manifest.version === PINS.version && manifest.board === PINS.board && manifest.chip === PINS.chip && manifest.flash_bytes === PINS.flashBytes, "ASSET");
  requireSafe(pair.app.sha256 === PINS.files["app.bin"].sha256, "ASSET");
  for (const name of Object.keys(PINS.files)) {
    const pin = PINS.files[name], item = manifest.files[name];
    requireSafe(item.bytes === pin.bytes && item.offset === pin.offset && item.sha256 === pin.sha256, "ASSET");
  }
  for (const name of ["app.bin", "bootloader.bin"]) {
    const b = files[name];
    requireSafe(b.length >= 24 && b[0] === 233 && (b[12] | b[13] << 8) === 9, "IMAGE");
  }
  requireSafe(files["app.bin"].length > 0 && files["app.bin"].length <= PINS.slotCapacity && files["bootloader.bin"].length < 32768, "CAPACITY");
  requireSafe(new TextDecoder().decode(files["app.bin"]).includes(PINS.label), "IMAGE");
  checkPartitions(files["partitions.bin"]);
  requireSafe(activeSlot(files["boot_app0.bin"]) === 0, "OTA");
  const release = Object.freeze({ version: PINS.version, board: PINS.board, appSha256: PINS.files["app.bin"].sha256 });
  verified.set(release, files);
  return release;
}
var DOWNLOAD_IDLE_MS = 6e4;
var DownloadFailure = class extends Stop {
  constructor(category, detail = {}) {
    super(["LENGTH", "SHA256", "MD5", "RELEASE"].includes(category) ? "ASSET" : category === "CANCELLED" ? "CANCELLED" : "DOWNLOAD");
    this.name = "DownloadFailure";
    this.detail = Object.freeze({ category, ...detail });
  }
};
function abortable(work, signal) {
  if (signal.aborted) return Promise.reject(new Error("download aborted"));
  return new Promise((resolve, reject) => {
    const abort = () => {
      signal.removeEventListener("abort", abort);
      reject(new Error("download aborted"));
    };
    signal.addEventListener("abort", abort, { once: true });
    Promise.resolve(work).then((value) => {
      signal.removeEventListener("abort", abort);
      resolve(value);
    }, (error) => {
      signal.removeEventListener("abort", abort);
      reject(error);
    });
  });
}
var ReleaseDownloader = class {
  #base;
  #fetch;
  #idle;
  #notify;
  #entries;
  #cache = /* @__PURE__ */ new Map();
  #active;
  #attempt = 0;
  #history = [];
  #current = { stage: "IDLE" };
  #disposed = false;
  constructor({ base = new URL("../firmware/", import.meta.url), fetcher = fetch, idleTimeoutMs = DOWNLOAD_IDLE_MS, onProgress = () => {
  }, extras = [] } = {}) {
    requireSafe(Number.isFinite(idleTimeoutMs) && idleTimeoutMs > 0, "DOWNLOAD_CONFIG");
    this.#base = new URL(base);
    this.#fetch = fetcher;
    this.#idle = idleTimeoutMs;
    this.#notify = onProgress;
    this.#entries = Object.entries({ ...PINS.documents, ...PINS.files }).map(([name, pin]) => ({ name, pin, url: new URL(name, this.#base) }));
    for (const extra of extras) {
      requireSafe(/^[A-Za-z0-9_.-]+$/.test(extra.name) && !this.#entries.some((e) => e.name === extra.name), "DOWNLOAD_CONFIG");
      requireSafe(Number.isSafeInteger(extra.pin.bytes) && extra.pin.bytes > 0 && extra.pin.bytes <= PINS.flashBytes && /^[a-f0-9]{64}$/.test(extra.pin.sha256), "DOWNLOAD_CONFIG");
      this.#entries.push({ name: extra.name, pin: Object.freeze({ ...extra.pin }), url: new URL(extra.url, this.#base) });
    }
    for (const entry of this.#entries) requireSafe(entry.url.origin === this.#base.origin, "ASSET");
  }
  snapshot() {
    return structuredClone({
      schema: "draftor-download/1",
      attempt: this.#attempt,
      idleTimeoutMs: this.#idle,
      totalDeadlineMs: null,
      ...this.#current,
      verifiedFiles: [...this.#cache.keys()],
      history: this.#history
    });
  }
  #emit(value) {
    this.#current = { ...value, attempt: this.#attempt, verifiedCount: this.#cache.size, fileCount: this.#entries.length };
    this.#notify(this.snapshot());
  }
  #remember(value) {
    this.#history.push(value);
    if (this.#history.length > 64) this.#history.shift();
  }
  cancel() {
    this.#active?.abort("CANCELLED");
  }
  dispose() {
    this.#disposed = true;
    this.cancel();
    for (const b of this.#cache.values()) b.fill(0);
    this.#cache.clear();
  }
  extraFile(name) {
    requireSafe(this.#cache.has(name) && !PINS.files[name] && !PINS.documents[name], "ASSET");
    return this.#cache.get(name).slice();
  }
  async #read(entry, controller, index) {
    const { name, pin, url } = entry, signal = controller.signal;
    let reader, response, buffer, received = 0, httpStatus = null, stage = "CONNECTING", timer, complete = false;
    const started = performance.now();
    const detail = () => ({ file: name, stage, httpStatus, receivedBytes: received, expectedBytes: pin.bytes });
    const failure = (category) => new DownloadFailure(category, detail());
    const emit = () => this.#emit({ ...detail(), fileIndex: index + 1 });
    const arm = () => {
      clearTimeout(timer);
      timer = setTimeout(() => controller.abort("IDLE_TIMEOUT"), this.#idle);
    };
    const alive = () => {
      if (signal.aborted) throw failure(signal.reason === "IDLE_TIMEOUT" ? "IDLE_TIMEOUT" : "CANCELLED");
    };
    emit();
    arm();
    try {
      alive();
      response = await abortable(this.#fetch.call(globalThis, url, { redirect: "error", credentials: "omit", cache: "no-store", signal }), signal);
      alive();
      httpStatus = response.status;
      if (!response.ok || response.redirected) throw failure("HTTP_STATUS");
      if (!response.body) throw failure("INCOMPLETE");
      const encoding = response.headers.get("content-encoding"), length = response.headers.get("content-length");
      if ((!encoding || encoding === "identity") && length !== null && Number(length) !== pin.bytes) throw failure("LENGTH");
      reader = response.body.getReader();
      buffer = new Uint8Array(pin.bytes);
      stage = "RECEIVING";
      emit();
      while (true) {
        alive();
        const { value, done } = await abortable(reader.read(), signal);
        alive();
        if (done) break;
        if (!(value instanceof Uint8Array)) throw failure("INCOMPLETE");
        if (!value.byteLength) continue;
        if (received + value.byteLength > pin.bytes) throw failure("LENGTH");
        buffer.set(value, received);
        received += value.byteLength;
        arm();
        emit();
      }
      clearTimeout(timer);
      if (received !== pin.bytes) throw failure("INCOMPLETE");
      stage = "VERIFYING";
      emit();
      if (await sha256(buffer) !== pin.sha256) throw failure("SHA256");
      alive();
      if (pin.md5 && md5(buffer) !== pin.md5) throw failure("MD5");
      alive();
      this.#cache.set(name, buffer);
      complete = true;
      stage = "VERIFIED";
      emit();
      this.#remember({ attempt: this.#attempt, ...detail(), elapsedMs: Math.round(performance.now() - started), result: "VERIFIED" });
    } catch (error) {
      const e = signal.aborted ? failure(signal.reason === "IDLE_TIMEOUT" ? "IDLE_TIMEOUT" : "CANCELLED") : error instanceof DownloadFailure ? error : failure("NETWORK");
      this.#remember({ attempt: this.#attempt, ...e.detail, elapsedMs: Math.round(performance.now() - started), result: "FAILED" });
      throw e;
    } finally {
      clearTimeout(timer);
      if (!complete) {
        buffer?.fill(0);
        controller.abort(signal.reason || "FAILED");
        try {
          Promise.resolve(reader ? reader.cancel() : response?.body?.cancel()).catch(() => {
          });
        } catch {
        }
      }
      try {
        reader?.releaseLock();
      } catch {
      }
    }
  }
  async load({ signal } = {}) {
    requireSafe(!this.#active && !this.#disposed, "DOWNLOAD_STATE");
    const controller = new AbortController();
    this.#active = controller;
    this.#attempt++;
    const abort = () => controller.abort("CANCELLED");
    signal?.addEventListener("abort", abort, { once: true });
    if (signal?.aborted) abort();
    try {
      for (let index = 0; index < this.#entries.length; index++) {
        if (controller.signal.aborted) throw new DownloadFailure("CANCELLED", { file: this.#entries[index].name, stage: "CONNECTING", httpStatus: null });
        const entry = this.#entries[index];
        if (this.#cache.has(entry.name)) {
          this.#emit({ file: entry.name, stage: "REUSED", fileIndex: index + 1, receivedBytes: entry.pin.bytes, expectedBytes: entry.pin.bytes });
          this.#remember({ attempt: this.#attempt, file: entry.name, stage: "REUSED", result: "VERIFIED_CACHE" });
        } else await this.#read(entry, controller, index);
      }
      this.#emit({ stage: "VERIFYING_RELEASE" });
      let release;
      try {
        release = await verifyRelease(Object.fromEntries(this.#cache));
      } catch {
        if (controller.signal.aborted) throw new DownloadFailure("CANCELLED", { file: null, stage: "VERIFYING_RELEASE", httpStatus: null });
        for (const b of this.#cache.values()) b.fill(0);
        this.#cache.clear();
        throw new DownloadFailure("RELEASE", { file: null, stage: "VERIFYING_RELEASE", httpStatus: null });
      }
      if (controller.signal.aborted) throw new DownloadFailure("CANCELLED", { file: null, stage: "VERIFYING_RELEASE", httpStatus: null });
      this.#emit({ stage: "READY" });
      return release;
    } catch (error) {
      const e = error instanceof DownloadFailure ? error : new DownloadFailure("NETWORK", { file: null, stage: this.#current.stage, httpStatus: null });
      this.#emit({ stage: e.detail.category === "CANCELLED" ? "CANCELLED" : "FAILED", error: e.detail });
      throw e;
    } finally {
      signal?.removeEventListener("abort", abort);
      this.#active = void 0;
    }
  }
};
function tableMatchesRelease(release, actual) {
  checkPartitions(actual);
  requireSafe(bytesEqual(actual.slice(0, 3072), releaseFile(release, "partitions.bin")), "PARTITIONS");
}

// src/download-ui.js
var copy = {
  zh: { preparing: "\u6B63\u5728\u51C6\u5907\u5B89\u88C5\u6587\u4EF6", checking: "\u6B63\u5728\u68C0\u67E5\u5B89\u88C5\u6587\u4EF6", failed: "\u4E0B\u8F7D\u5931\u8D25\uFF0C\u8BF7\u68C0\u67E5\u7F51\u7EDC\u540E\u91CD\u8BD5", retry: "\u91CD\u65B0\u4E0B\u8F7D", details: "\u4E0B\u8F7D\u8BE6\u60C5\uFF08\u4E0D\u542B\u4E2A\u4EBA\u6570\u636E\uFF09", receiving: "\u6B63\u5728\u4E0B\u8F7D", file: "\u6587\u4EF6", idle: "\u8FDE\u63A5\u957F\u65F6\u95F4\u6CA1\u6709\u6536\u5230\u65B0\u6570\u636E\u3002", network: "\u7F51\u7EDC\u8FDE\u63A5\u5931\u8D25\u6216\u4E0B\u8F7D\u4E2D\u65AD\u3002", http: "\u670D\u52A1\u5668\u6682\u65F6\u65E0\u6CD5\u63D0\u4F9B\u6587\u4EF6\u3002", incomplete: "\u6587\u4EF6\u672A\u4E0B\u8F7D\u5B8C\u6574\u3002", invalid: "\u6587\u4EF6\u6821\u9A8C\u672A\u901A\u8FC7\uFF0C\u4E0D\u80FD\u7528\u4E8E\u5B89\u88C5\u3002", safe: "\u5C1A\u672A\u8FDE\u63A5\u8BBE\u5907\uFF0C\u672A\u64E6\u9664\u6216\u5199\u5165\u3002", cancelled: "\u5DF2\u53D6\u6D88\u4E0B\u8F7D\uFF0C\u672A\u64E6\u9664\u6216\u5199\u5165\u3002" },
  en: { preparing: "Preparing installation files", checking: "Checking installation files", failed: "Download failed. Check your network and retry.", retry: "Retry download", details: "Download details (no personal data)", receiving: "Downloading", file: "File", idle: "No new data arrived for too long.", network: "The network connection failed or the download was interrupted.", http: "The server could not provide the file.", incomplete: "The file was not downloaded completely.", invalid: "File verification failed. It cannot be used for installation.", safe: "No device connection, erase or write has started.", cancelled: "Download cancelled. No erase or write has started." }
};
var amount = (n) => (n / 1e6).toFixed(2) + " MB";
function downloadPanel({ anchor, onChange, onRetry, options = {} }) {
  const section = document.createElement("section");
  section.id = "download-preparation";
  section.hidden = true;
  section.innerHTML = '<p id="download-progress-text" aria-live="polite"></p><progress id="download-progress" max="1" value="0"></progress><p id="download-problem" role="alert"></p><button id="retry-download" type="button"></button><details id="download-details"><summary></summary><pre></pre></details>';
  anchor.after(section);
  const $ = (id) => section.querySelector("#" + id);
  let loader, state = { stage: "IDLE" }, lang = "zh";
  $("retry-download").addEventListener("click", () => {
    if (state.stage === "FAILED") onRetry();
  });
  function status(language) {
    const c = copy[language];
    return state.stage === "FAILED" ? c.failed : state.stage === "CANCELLED" ? c.cancelled : ["VERIFYING", "VERIFYING_RELEASE"].includes(state.stage) ? c.checking : c.preparing;
  }
  function render(language, visible) {
    lang = language;
    const c = copy[lang];
    section.hidden = !visible;
    const failed = state.stage === "FAILED", active = ["CONNECTING", "RECEIVING"].includes(state.stage);
    const detail = state.error, category = detail?.category;
    $("download-progress-text").textContent = active ? `${c.file} ${state.fileIndex} / ${state.fileCount} \xB7 ${c.receiving}\uFF1A${amount(state.receivedBytes || 0)}${Number.isFinite(state.expectedBytes) ? " / " + amount(state.expectedBytes) : ""}` : "";
    $("download-progress").hidden = !active;
    if (Number.isFinite(state.expectedBytes)) {
      $("download-progress").max = state.expectedBytes;
      $("download-progress").value = state.receivedBytes || 0;
    } else $("download-progress").removeAttribute("value");
    $("download-problem").hidden = !failed;
    $("download-problem").textContent = failed ? (category === "IDLE_TIMEOUT" ? c.idle : category === "HTTP_STATUS" ? c.http : category === "INCOMPLETE" ? c.incomplete : ["LENGTH", "SHA256", "MD5", "RELEASE"].includes(category) ? c.invalid : c.network) + " " + c.safe : "";
    $("retry-download").hidden = !failed;
    $("retry-download").disabled = !failed;
    $("retry-download").textContent = c.retry;
    $("download-details").querySelector("summary").textContent = c.details;
    $("download-details").querySelector("pre").textContent = JSON.stringify(state, null, 2);
  }
  return {
    reset() {
      loader?.dispose();
      state = { stage: "IDLE" };
      loader = new ReleaseDownloader({ ...options, onProgress: (value) => {
        state = value;
        onChange();
      } });
    },
    load() {
      return loader.load();
    },
    extraFile(name) {
      return loader.extraFile(name);
    },
    cancel() {
      loader?.cancel();
    },
    dispose() {
      loader?.dispose();
    },
    snapshot() {
      return structuredClone(state);
    },
    status,
    render
  };
}

// src/recovery-zip.js
var PACKAGE_NAMES = Object.freeze(["manifest.json", "flash.bin", "README.zh-CN.txt", "README.en.txt"]);
var MAX_PACKAGE_BYTES = 16777216 + 32768;
var te = new TextEncoder();
var td = new TextDecoder("utf-8", { fatal: true });
var crcTable = Uint32Array.from({ length: 256 }, (_, n) => {
  for (let i = 0; i < 8; i++) n = n >>> 1 ^ (n & 1 ? 3988292384 : 0);
  return n >>> 0;
});
function zipCrc(bytes) {
  let n = 4294967295;
  for (const b of bytes) n = n >>> 8 ^ crcTable[(n ^ b) & 255];
  return (n ^ 4294967295) >>> 0;
}
var limit = (name, n) => requireSafe(name === "flash.bin" ? n === 16777216 : n > 0 && n <= (name === "manifest.json" ? 12288 : 4096), "PACKAGE_SIZE");
function storePackage(entries) {
  const local = [], central = [];
  let at = 0;
  for (const name of PACKAGE_NAMES) {
    const data = entries[name];
    requireSafe(data instanceof Uint8Array, "PACKAGE_FORMAT");
    limit(name, data.length);
    const n = te.encode(name), crc = zipCrc(data), h = new Uint8Array(30 + n.length), v2 = new DataView(h.buffer);
    v2.setUint32(0, 67324752, true);
    v2.setUint16(4, 20, true);
    v2.setUint16(12, 33, true);
    v2.setUint32(14, crc, true);
    v2.setUint32(18, data.length, true);
    v2.setUint32(22, data.length, true);
    v2.setUint16(26, n.length, true);
    h.set(n, 30);
    const c = new Uint8Array(46 + n.length), w = new DataView(c.buffer);
    w.setUint32(0, 33639248, true);
    w.setUint16(4, 20, true);
    w.setUint16(6, 20, true);
    w.setUint16(14, 33, true);
    w.setUint32(16, crc, true);
    w.setUint32(20, data.length, true);
    w.setUint32(24, data.length, true);
    w.setUint16(28, n.length, true);
    w.setUint32(42, at, true);
    c.set(n, 46);
    local.push(h, data);
    central.push(c);
    at += h.length + data.length;
  }
  const dir = concat(...central), end = new Uint8Array(22), v = new DataView(end.buffer);
  v.setUint32(0, 101010256, true);
  v.setUint16(8, 4, true);
  v.setUint16(10, 4, true);
  v.setUint32(12, dir.length, true);
  v.setUint32(16, at, true);
  return concat(...local, dir, end);
}
function readPackageArchive(input) {
  requireSafe(input instanceof Uint8Array && input.length > 16777216 && input.length <= MAX_PACKAGE_BYTES, "PACKAGE_SIZE");
  const b = input, v = new DataView(b.buffer, b.byteOffset, b.byteLength), end = b.length - 22;
  const u16 = (p) => v.getUint16(p, true), u322 = (p) => v.getUint32(p, true);
  requireSafe(u322(end) === 101010256 && u16(end + 4) === 0 && u16(end + 6) === 0 && u16(end + 8) === 4 && u16(end + 10) === 4 && u16(end + 20) === 0, "PACKAGE_FORMAT");
  const dir = u322(end + 16), dirSize = u322(end + 12);
  requireSafe(dir > 0 && dir + dirSize === end && dirSize < 1024, "PACKAGE_FORMAT");
  let c = dir, at = 0;
  const entries = {};
  for (const expected of PACKAGE_NAMES) {
    requireSafe(c + 46 <= end && u322(c) === 33639248, "PACKAGE_FORMAT");
    requireSafe(u16(c + 4) === 20 && u16(c + 6) === 20 && u16(c + 8) === 0 && u16(c + 10) === 0 && u16(c + 12) === 0 && u16(c + 14) === 33 && u16(c + 30) === 0 && u16(c + 32) === 0 && u16(c + 34) === 0 && u16(c + 36) === 0 && u322(c + 38) === 0, "PACKAGE_FORMAT");
    const size = u322(c + 24), crc = u322(c + 16), n = u16(c + 28), off = u322(c + 42);
    requireSafe(n === expected.length && c + 46 + n <= end && td.decode(b.subarray(c + 46, c + 46 + n)) === expected && off === at, "PACKAGE_PATH");
    limit(expected, size);
    requireSafe(u322(c + 20) === size && at + 30 + n + size <= dir, "PACKAGE_SIZE");
    requireSafe(u322(at) === 67324752 && u16(at + 4) === 20 && u16(at + 6) === 0 && u16(at + 8) === 0 && u16(at + 10) === 0 && u16(at + 12) === 33 && u322(at + 14) === crc && u322(at + 18) === size && u322(at + 22) === size && u16(at + 26) === n && u16(at + 28) === 0 && bytesEqual(b.subarray(at + 30, at + 30 + n), te.encode(expected)), "PACKAGE_FORMAT");
    const data = b.slice(at + 30 + n, at + 30 + n + size);
    requireSafe(zipCrc(data) === crc, "PACKAGE_CRC");
    entries[expected] = data;
    at += 30 + n + size;
    c += 46 + n;
  }
  requireSafe(at === dir && c === end, "PACKAGE_FORMAT");
  return entries;
}

// src/release-platform.js
function desktopSerialStatus(environment) {
  const { userAgent = "", platform = "", mobile = false, maxTouchPoints = 0, secureContext = false, serialAvailable = false, serialPolicyAllowed = true } = environment;
  if (mobile || /Android|iPhone|iPad|iPod|Mobile/i.test(userAgent) || /Mac/i.test(platform) && maxTouchPoints > 1) return { qualified: false, reason: "MOBILE" };
  if (!secureContext) return { qualified: false, reason: "INSECURE_CONTEXT" };
  if (!serialPolicyAllowed) return { qualified: false, reason: "SERIAL_POLICY" };
  if (!serialAvailable) return { qualified: false, reason: "SERIAL_UNAVAILABLE" };
  return { qualified: true, reason: "CAPABLE_DESKTOP" };
}
function browserEnvironment() {
  return { userAgent: navigator.userAgent, platform: navigator.userAgentData?.platform || navigator.platform, mobile: navigator.userAgentData?.mobile === true, maxTouchPoints: navigator.maxTouchPoints || 0, secureContext: isSecureContext, serialAvailable: typeof navigator.serial?.requestPort === "function", serialPolicyAllowed: (document.permissionsPolicy || document.featurePolicy)?.allowsFeature?.("serial") !== false };
}
var browserIsQualified = () => desktopSerialStatus(browserEnvironment()).qualified;
function platformMessage(language, environment = browserEnvironment()) {
  const reason = desktopSerialStatus(environment).reason;
  const copy2 = { zh: {
    MOBILE: "\u624B\u673A\u548C\u5E73\u677F\u4E0D\u5F00\u653E USB \u64CD\u4F5C\u3002\u8BF7\u4F7F\u7528 Windows 10/11 \u684C\u9762 Chrome \u6216 Edge\uFF1B\u6B64\u5904\u4ECD\u53EF\u6D4F\u89C8\u548C\u4E0B\u8F7D\u3002",
    INSECURE_CONTEXT: "\u5F53\u524D\u9875\u9762\u4E0D\u662F\u5B89\u5168\u4E0A\u4E0B\u6587\uFF0C\u672A\u8FDE\u63A5\u8BBE\u5907\u3002\u8BF7\u6253\u5F00\u6B63\u5F0F HTTPS \u7F51\u7AD9\uFF1B\u79BB\u7EBF\u6062\u590D\u8BF7\u7528\u684C\u9762 Chrome/Edge \u76F4\u63A5\u6253\u5F00\u5DF2\u4FDD\u5B58\u7684 HTML\u3002\u4E0D\u8981\u5173\u95ED\u6D4F\u89C8\u5668\u5B89\u5168\u8BBE\u7F6E\u3002",
    SERIAL_POLICY: "\u6B64\u9875\u9762\u7684\u4E32\u53E3\u6743\u9650\u88AB\u7B56\u7565\u9650\u5236\uFF0C\u672A\u8FDE\u63A5\u8BBE\u5907\u3002\u53EF\u5C1D\u8BD5\u684C\u9762 Chrome\uFF1B\u53D7\u7BA1\u7406\u7684\u7535\u8111\u8BF7\u8054\u7CFB\u7BA1\u7406\u5458\u5141\u8BB8 Serial API\uFF0C\u4E0D\u8981\u7ED5\u8FC7\u7EC4\u7EC7\u7B56\u7565\u3002",
    SERIAL_UNAVAILABLE: "\u6D4F\u89C8\u5668\u6CA1\u6709\u63D0\u4F9B Web Serial\uFF0C\u672A\u8FDE\u63A5\u8BBE\u5907\u3002Windows \u8BF7\u7528\u6700\u65B0\u7248\u684C\u9762 Chrome \u6216 Edge\uFF1BEdge \u53D7\u7EC4\u7EC7\u7B56\u7565\u9650\u5236\u65F6\u53EF\u5C1D\u8BD5 Chrome\uFF0C\u6216\u8054\u7CFB\u7BA1\u7406\u5458\u3002\u79BB\u7EBF HTML \u5FC5\u987B\u76F4\u63A5\u5728\u8BE5\u6D4F\u89C8\u5668\u6253\u5F00\u3002",
    CAPABLE_DESKTOP: "\u5F53\u524D\u684C\u9762\u73AF\u5883\u53EF\u4EE5\u8FDE\u63A5\u4E32\u53E3\u3002Windows Chrome\uFF0FEdge \u53EF\u4F7F\u7528\uFF1B\u5B8C\u6574\u5B89\u88C5\u4E0E\u6062\u590D\u4ECD\u5F85 Windows \u5B9E\u673A\u53CD\u9988\u3002\u8BF7\u6309\u672C\u9875\u63D0\u793A\u4FDD\u5B58\u6587\u4EF6\u3002"
  }, en: {
    MOBILE: "USB operations are unavailable on phones/tablets. Use Windows 10/11 desktop Chrome or Edge. Reading and downloads remain available.",
    INSECURE_CONTEXT: "This page is not a secure context; no device connected. Use the official HTTPS site, or open the saved offline HTML directly in desktop Chrome/Edge. Do not disable browser security.",
    SERIAL_POLICY: "Serial access is restricted by policy; no device connected. Try desktop Chrome or ask your administrator to allow the Serial API. Do not bypass organizational policy.",
    SERIAL_UNAVAILABLE: "Web Serial is unavailable; no device connected. On Windows use current desktop Chrome or Edge. If Edge is managed, try Chrome or ask your administrator. Open the offline HTML directly in that browser.",
    CAPABLE_DESKTOP: "Web Serial is available. Windows Chrome/Edge can be used; complete installation and recovery still await Windows device feedback. Save files as directed on this page."
  } };
  return copy2[language === "en" ? "en" : "zh"][reason];
}
function publicErrorCode(error) {
  if (typeof error?.code === "string" && /^[A-Z_0-9]{1,48}$/.test(error.code)) return error.code;
  if (error?.name === "NotFoundError") return "PORT_SELECTION_CANCELLED";
  if (["SecurityError", "NotAllowedError"].includes(error?.name)) return "SERIAL_PERMISSION_DENIED";
  return "IO";
}
function serialErrorHelp(code, language) {
  const en = language === "en";
  if (code === "SERIAL_PERMISSION_DENIED") return en ? "Serial permission was denied or blocked by browser/organization policy. No automatic retry. Use site permissions, try desktop Chrome, or ask your administrator." : "\u4E32\u53E3\u6388\u6743\u88AB\u62D2\u7EDD\u6216\u88AB\u6D4F\u89C8\u5668\uFF0F\u7EC4\u7EC7\u7B56\u7565\u9650\u5236\uFF0C\u4E0D\u4F1A\u81EA\u52A8\u91CD\u8BD5\u3002\u8BF7\u68C0\u67E5\u7F51\u7AD9\u6743\u9650\uFF0C\u5C1D\u8BD5\u684C\u9762 Chrome\uFF0C\u6216\u8054\u7CFB\u7BA1\u7406\u5458\u3002";
  if (code === "PORT_SELECTION_CANCELLED") return en ? "Port selection cancelled; no device connected. Close this session before starting again." : "\u5DF2\u53D6\u6D88\u4E32\u53E3\u9009\u62E9\uFF0C\u5C1A\u672A\u8FDE\u63A5\u8BBE\u5907\u3002\u5173\u95ED\u672C\u6B21\u4F1A\u8BDD\u540E\u53EF\u4E3B\u52A8\u91CD\u65B0\u5F00\u59CB\u3002";
  if (code === "CONNECTION") return en ? "The port could not be opened. Close serial monitors/other installer tabs; check the data cable, BOOT download mode and Device Manager. Do not guess a COM port or keep retrying a communication fault." : "\u4E32\u53E3\u672A\u80FD\u6253\u5F00\u3002\u8BF7\u5173\u95ED\u4E32\u53E3\u76D1\u89C6\u5668\u6216\u5176\u4ED6\u5B89\u88C5\u9875\uFF0C\u68C0\u67E5\u6570\u636E\u7EBF\u3001BOOT \u4E0B\u8F7D\u6A21\u5F0F\u53CA\u8BBE\u5907\u7BA1\u7406\u5668\u3002\u4E0D\u8981\u731C\u6D4B COM \u7AEF\u53E3\uFF0C\u4E5F\u4E0D\u8981\u5BF9\u901A\u4FE1\u5F02\u5E38\u8FDE\u7EED\u91CD\u8BD5\u3002";
  return "";
}

// src/factory-pins.js
var FACTORY = deepFreeze({
  "commit": "d0644cf2c7a89459538935404ee04b76648e7dec",
  "bytes": 4548144,
  "sha256": "d0591315a722d33f4a08931a0341ab840a6c15c56b289d621e8fc18bec8d55a8",
  "md5": "4c88298de096e1cdf01a1433dd6896cd",
  "git_blob_sha1": "de836216f297a8f3c25b82a4e944682bbf90cca5",
  "offset": 0,
  "partitions": [
    {
      "type": 1,
      "subtype": 2,
      "offset": "0x9000",
      "size": "0x6000",
      "label": "nvs",
      "flags": 0
    },
    {
      "type": 1,
      "subtype": 1,
      "offset": "0xf000",
      "size": "0x1000",
      "label": "phy_init",
      "flags": 0
    },
    {
      "type": 0,
      "subtype": 0,
      "offset": "0x10000",
      "size": "0x800000",
      "label": "factory",
      "flags": 0
    }
  ],
  "header": "e903024f4c893c40ee000000090000000063000000000001",
  "repository": "https://github.com/waveshareteam/ESP32-S3-RLCD-4.2",
  "file": "firmware/ESP32-S3-RLCD-4.2_FactoryOnly_20260120.bin",
  "tableSha256": "597f3a1bd0e54c833cea76f49728719f67a09d69995046f077727904d0267ef4",
  "fingerprint": [
    {
      "offset": 0,
      "bytes": 36864,
      "md5": "e901c055ab7598f98b053ba3d50dbd00"
    },
    {
      "offset": 65536,
      "bytes": 4482608,
      "md5": "43632d06e0a705969cbcd7bb3eb1da0b"
    },
    {
      "offset": 4548144,
      "bytes": 12229072,
      "md5": "db8a00c2dc71ca9943c0a9b074238206"
    }
  ],
  "mutableRanges": [
    {
      "offset": 36864,
      "bytes": 28672,
      "reason": "factory NVS and phy_init; may change on ordinary factory boot, erased only after separate approval"
    }
  ],
  "draftorInitializedFullMd5": "4e805a469b67740ccaad7e3807ba0509"
});

// src/first-install.js
var FIRST_FILES = Object.freeze(["bootloader.bin", "partitions.bin", "boot_app0.bin", "app.bin"]);
var CLEAN_INITIAL_STATES = Object.freeze(["blank", "waveshare-factory-20260120", "unrecognized"]);
function cleanStateAllowed(state) {
  return CLEAN_INITIAL_STATES.includes(state);
}
async function inspectOriginalFlash(driver, table, ota) {
  if (await driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5)
    return { firmware: "blank", activeApp: "none" };
  if (await isFactoryTable(table)) {
    let matches = true;
    for (const row of FACTORY.fingerprint) {
      if (await driver.flashMd5(row.offset, row.bytes) !== row.md5) matches = false;
    }
    if (matches) return { firmware: "waveshare-factory-20260120", activeApp: "factory" };
  }
  if (await sha256(table.subarray(0, 3072)) === PINS.files["partitions.bin"].sha256) {
    let slot;
    try {
      slot = activeSlot(ota);
    } catch (error) {
      if (!(error instanceof Stop) || error.code !== "OTA") throw error;
    }
    if (slot === 0) {
      let matches = true;
      for (const name of ["bootloader.bin", "app.bin"]) {
        const pin = PINS.files[name];
        if (await driver.flashMd5(pin.offset, pin.bytes) !== pin.md5) matches = false;
      }
      if (matches) return { firmware: "draftor-alpha3-app0", activeApp: "app0" };
    }
  }
  return { firmware: "unrecognized", activeApp: "unknown" };
}
async function isFactoryTable(table) {
  return table.length === 4096 && await sha256(table) === FACTORY.tableSha256;
}
async function verifyInitialFlash(driver, source) {
  requireSafe(["blank", "waveshare-factory-20260120"].includes(source), "UNKNOWN");
  if (source === "blank") {
    requireSafe(await driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "UNKNOWN");
  } else {
    for (const row of FACTORY.fingerprint)
      requireSafe(await driver.flashMd5(row.offset, row.bytes) === row.md5, "FACTORY_FINGERPRINT");
  }
}

// src/recovery-package.js
var RECOVERY_SCHEMA = "draftor-private-recovery/1";
var RAW_RECOVERY_SCHEMA = "draftor-private-recovery/2";
var te2 = new TextEncoder();
var td2 = new TextDecoder("utf-8", { fatal: true });
var validated = /* @__PURE__ */ new WeakMap();
var json = (v) => JSON.stringify(v, null, 2) + "\n";
var keys = (v, expected) => requireSafe(v && typeof v === "object" && !Array.isArray(v) && Object.keys(v).sort().join(",") === expected.split(",").sort().join(","), "PACKAGE_SCHEMA");
var text = (v, n = 128) => requireSafe(typeof v === "string" && v.length > 0 && v.length <= n && !/[\u0000-\u001f]/.test(v), "PACKAGE_SCHEMA");
function checkedIdentity(info) {
  requireSafe(info?.chip === "ESP32-S3" && info.flashBytes === PINS.flashBytes, "CHIP");
  requireSafe(info.securityKnown === true && info.secureBoot === false && info.flashEncryption === false && info.securityRule === "ESP32S3_ROM20_ESPt531_V1" && info.romChipId === 9, "SECURITY");
  const flags = info.securityFlags, count = info.cryptCount;
  requireSafe(Number.isInteger(flags) && flags >= 0 && (flags & ~2047) === 0 && (flags & 5) === 0 && Number.isInteger(count) && count >= 0 && count <= 7 && count.toString(2).split("1").length % 2 === 1, "SECURITY");
  return { chip: info.chip, flashBytes: info.flashBytes, mac: normalizeMac(info.mac), security: { rule: info.securityRule, flags, cryptCount: count, romChipId: 9, secureBoot: false, flashEncryption: false } };
}
function sameOriginalIdentity(info, original) {
  const actual = checkedIdentity(info);
  requireSafe(actual.mac === original.mac, "IDENTITY");
  requireSafe(JSON.stringify(actual) === JSON.stringify(original), "SECURITY_CHANGED");
  return actual;
}
async function classifyRecoveryImage(bytes) {
  requireSafe(bytes instanceof Uint8Array && bytes.length === PINS.flashBytes, "PACKAGE_SIZE");
  const table = bytes.subarray(32768, 36864), ota = bytes.subarray(57344, 65536);
  const tableSha = await sha256(table);
  const original = await inspectOriginalFlash({
    flashMd5: async (offset, size) => md5(bytes.subarray(offset, offset + size))
  }, table, ota);
  if (original.firmware === "waveshare-factory-20260120") {
    return { firmware: original.firmware, rule: "FACTORY_FIXED_THREE_RANGES_V1", partition: { offset: 32768, bytes: 4096, sha256: tableSha, md5: md5(table), rows: FACTORY.partitions }, ota: { applicable: false }, activeApp: "factory" };
  }
  if (original.firmware === "draftor-alpha3-app0") {
    const rows = checkPartitions(table);
    for (const name of ["bootloader.bin", "app.bin"]) {
      const pin = PINS.files[name];
      requireSafe(await sha256(bytes.subarray(pin.offset, pin.offset + pin.bytes)) === pin.sha256, "PACKAGE_HASH");
    }
    return {
      firmware: original.firmware,
      rule: "ALPHA3_BOOT_APP0_B_TABLE_V1",
      partition: { offset: 32768, bytes: 4096, sha256: tableSha, md5: md5(table), rows },
      ota: { applicable: true, offset: 57344, bytes: 8192, sha256: await sha256(ota), md5: md5(ota), entries: [0, 4096].map((at) => ({ sequence: u32(ota, at), state: u32(ota, at + 24), crc: u32(ota, at + 28) })) },
      activeApp: "app0"
    };
  }
  return {
    firmware: original.firmware,
    rule: "RAW_FULL_FLASH_V1",
    partition: { offset: 32768, bytes: 4096, sha256: tableSha, md5: md5(table), rows: null },
    ota: { applicable: true, offset: 57344, bytes: 8192, sha256: await sha256(ota), md5: md5(ota), entries: null },
    activeApp: original.activeApp
  };
}
var readmes = {
  "README.zh-CN.txt": "\u79C1\u4EBA\u5B8C\u6574 Flash \u6062\u590D\u5305\u3002\u4E0D\u8981\u4E0A\u4F20\u3001\u516C\u5F00\u6216\u53D1\u9001\u7ED9\u4ED6\u4EBA\u3002\u4EC5\u9002\u7528\u4E8E\u521B\u5EFA\u65F6\u7684\u539F\u5FAE\u96EA ESP32-S3-RLCD-4.2 / ST7305\u3002\u6062\u590D\u8986\u76D6\u5168\u90E8 16 MiB \u5185\u90E8 Flash\uFF0C\u8BBE\u7F6E\u3001\u914D\u5BF9\u53CA\u5185\u90E8\u6587\u7A3F\u56DE\u5230\u5907\u4EFD\u65F6\uFF1BTF \u6587\u4EF6\u4E0D\u5728\u672C\u5305\u4E2D\uFF0C\u5FC5\u987B\u72EC\u7ACB\u4FDD\u7559\u3002\u6062\u590D\u5382\u5BB6\u5907\u4EFD\u4F1A\u56DE\u5230\u5382\u5BB6\u6F14\u793A\uFF0C\u4E0D\u4F1A\u5B89\u88C5 Draftor\u3002\u4F7F\u7528\u968F\u9644\u79BB\u7EBF\u5DE5\u5177\uFF0C\u9009\u62E9\u672C ZIP \u5E76\u8FDE\u63A5\u539F\u8BBE\u5907\uFF1B\u6821\u9A8C\u901A\u8FC7\u540E\u624D\u786E\u8BA4\u6062\u590D\u3002\u5B8C\u6210\u540E\u5FC5\u987B\u65AD\u7535\u3001\u63D2\u56DE TF\u3001\u666E\u901A\u542F\u52A8\u9A8C\u6536\u3002\u6458\u8981\u548C MAC \u53EA\u9632\u8BEF\u9009\u53CA\u610F\u5916\u635F\u574F\uFF0C\u4E0D\u662F\u6570\u5B57\u7B7E\u540D\u6216\u8EAB\u4EFD\u8BA4\u8BC1\u3002\n",
  "README.en.txt": "PRIVATE full Flash recovery package. Do not upload, publish or share. Only for the original Waveshare ESP32-S3-RLCD-4.2 / ST7305. Recovery overwrites all 16 MiB of internal Flash and rolls settings, pairing and internal documents back to backup time. TF files are NOT included; preserve TF separately. Restoring a factory backup returns the factory demo, not Draftor. Open the offline recovery tool, select this ZIP, connect the original board and confirm only after checks pass. After verification, power off, insert TF and validate normal boot. Hashes and MAC are accidental-corruption / wrong-device safeguards, not signatures or authentication.\n"
};
var proofMethods = ["ROM_READ_SLOW_MD5_V1", "ROM_MD5_MATCH_EXISTING_V1", "ROM_ORIGINAL_RECONSTRUCTED_V1"];
function checkReconstruction(p) {
  keys(p, "pointSha256,originalCheckedAt,mode,currentPrivateRangesUnchanged,currentFlashIsOriginal");
  requireSafe(/^[0-9a-f]{64}$/.test(p.pointSha256) && /^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(p.originalCheckedAt) && Number.isFinite(Date.parse(p.originalCheckedAt)), "PACKAGE_PROOF");
  requireSafe(["fresh", "update"].includes(p.mode) && p.currentPrivateRangesUnchanged === (p.mode === "update") && p.currentFlashIsOriginal === false, "PACKAGE_PROOF");
}
async function createRecoveryPackage(bytes, info, { boardConfirmed, romMd5Before, romMd5After, method, build, sourceCommit, reconstruction, createdAt = (/* @__PURE__ */ new Date()).toISOString(), id = crypto.randomUUID() } = {}) {
  requireSafe(boardConfirmed === true, "BOARD_CONFIRM");
  const image = bytes.slice(), device = checkedIdentity(info), digest = md5(image);
  requireSafe(romMd5Before === digest && romMd5After === digest, "BACKUP_STALE");
  requireSafe(proofMethods.includes(method), "PACKAGE_PROOF");
  if (method === "ROM_ORIGINAL_RECONSTRUCTED_V1") checkReconstruction(reconstruction);
  else requireSafe(reconstruction === void 0, "PACKAGE_PROOF");
  const original = await classifyRecoveryImage(image);
  const manifest = {
    schema: original.rule === "RAW_FULL_FLASH_V1" ? RAW_RECOVERY_SCHEMA : RECOVERY_SCHEMA,
    id,
    createdAt,
    tool: { name: "Draftor Recovery", build, sourceCommit },
    board: { model: "ESP32-S3-RLCD-4.2", controller: "ST7305", confirmation: "USER_PHYSICAL_CONFIRMATION" },
    device,
    image: { file: "flash.bin", offset: 0, bytes: PINS.flashBytes, sha256: await sha256(image), md5: digest },
    creationProof: { method, romMd5Before, romMd5After, romMd5Match: true, identitySource: "ROM_READ_REG_EFUSE", securitySource: "ROM_GET_SECURITY_INFO", capacitySource: "JEDEC_RDID", ...reconstruction ? { reconstruction } : {} },
    original
  };
  const entries = { "manifest.json": te2.encode(json(manifest)), "flash.bin": image, ...Object.fromEntries(Object.entries(readmes).map(([k, v]) => [k, te2.encode(v)])) };
  const archive = storePackage(entries);
  const checked = await validateRecoveryPackage(archive);
  destroyRecoveryPackage(checked);
  image.fill(0);
  return archive;
}
async function validateRecoveryPackage(input) {
  const entries = readPackageArchive(input), raw = td2.decode(entries["manifest.json"]);
  let m;
  try {
    m = JSON.parse(raw);
  } catch {
    requireSafe(false, "PACKAGE_JSON");
  }
  requireSafe(json(m) === raw, "PACKAGE_JSON");
  keys(m, "schema,id,createdAt,tool,board,device,image,creationProof,original");
  requireSafe([RECOVERY_SCHEMA, RAW_RECOVERY_SCHEMA].includes(m.schema), "PACKAGE_SCHEMA");
  requireSafe(/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(m.id), "PACKAGE_SCHEMA");
  requireSafe(typeof m.createdAt === "string" && /^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(m.createdAt) && Number.isFinite(Date.parse(m.createdAt)), "PACKAGE_SCHEMA");
  keys(m.tool, "name,build,sourceCommit");
  requireSafe(m.tool.name === "Draftor Recovery", "PACKAGE_SCHEMA");
  text(m.tool.build);
  requireSafe(/^[0-9a-f]{40}$/.test(m.tool.sourceCommit), "PACKAGE_SCHEMA");
  requireSafe(JSON.stringify(m.board) === JSON.stringify({ model: "ESP32-S3-RLCD-4.2", controller: "ST7305", confirmation: "USER_PHYSICAL_CONFIRMATION" }), "BOARD_CONFIRM");
  keys(m.device, "chip,flashBytes,mac,security");
  keys(m.device.security, "rule,flags,cryptCount,romChipId,secureBoot,flashEncryption");
  const s = m.device.security, normalized = checkedIdentity({ ...m.device, securityKnown: true, secureBoot: s.secureBoot, flashEncryption: s.flashEncryption, securityRule: s.rule, securityFlags: s.flags, cryptCount: s.cryptCount, romChipId: s.romChipId });
  requireSafe(JSON.stringify(normalized) === JSON.stringify(m.device), "PACKAGE_SCHEMA");
  keys(m.image, "file,offset,bytes,sha256,md5");
  const image = entries["flash.bin"];
  requireSafe(m.image.file === "flash.bin" && m.image.offset === 0 && m.image.bytes === PINS.flashBytes && m.image.sha256 === await sha256(image) && m.image.md5 === md5(image), "PACKAGE_HASH");
  const rebuilt = m.creationProof?.method === "ROM_ORIGINAL_RECONSTRUCTED_V1";
  keys(m.creationProof, "method,romMd5Before,romMd5After,romMd5Match,identitySource,securitySource,capacitySource" + (rebuilt ? ",reconstruction" : ""));
  if (rebuilt) checkReconstruction(m.creationProof.reconstruction);
  requireSafe(m.creationProof.identitySource === "ROM_READ_REG_EFUSE" && m.creationProof.securitySource === "ROM_GET_SECURITY_INFO" && m.creationProof.capacitySource === "JEDEC_RDID", "PACKAGE_PROOF");
  requireSafe(proofMethods.includes(m.creationProof.method) && m.creationProof.romMd5Match === true && m.creationProof.romMd5Before === m.image.md5 && m.creationProof.romMd5After === m.image.md5, "PACKAGE_PROOF");
  if (rebuilt) requireSafe(m.original.firmware === (m.creationProof.reconstruction.mode === "fresh" ? "waveshare-factory-20260120" : "draftor-alpha3-app0"), "PACKAGE_PROOF");
  requireSafe(JSON.stringify(await classifyRecoveryImage(image)) === JSON.stringify(m.original), "PACKAGE_METADATA");
  requireSafe(m.schema === (m.original.rule === "RAW_FULL_FLASH_V1" ? RAW_RECOVERY_SCHEMA : RECOVERY_SCHEMA), "PACKAGE_SCHEMA");
  for (const [name, content] of Object.entries(readmes)) requireSafe(td2.decode(entries[name]) === content, "PACKAGE_FORMAT");
  const handle = Object.freeze({});
  validated.set(handle, { manifest: deepFreeze(m), image });
  return handle;
}
function recoverySnapshot(handle) {
  const p = validated.get(handle);
  requireSafe(p, "PACKAGE_REQUIRED");
  return { manifest: structuredClone(p.manifest), image: p.image.slice() };
}
function recoverySummary(handle) {
  const p = validated.get(handle);
  requireSafe(p, "PACKAGE_REQUIRED");
  return { createdAt: p.manifest.createdAt, firmware: p.manifest.original.firmware, bytes: PINS.flashBytes, activeApp: p.manifest.original.activeApp };
}
function destroyRecoveryPackage(handle) {
  const p = validated.get(handle);
  if (p) {
    p.image.fill(0);
    validated.delete(handle);
  }
}

// src/recovery-session.js
async function inspectSupportedOriginal(driver, diagnostics) {
  const table = await driver.readFlash(32768, 4096);
  const ota = await driver.readFlash(57344, 8192);
  requireSafe(table.length === 4096 && ota.length === 8192, "READBACK");
  for (const [label, offset, bytes] of [["partition", 32768, table], ["ota", 57344, ota]]) {
    diagnostics?.check(label + "Read", true);
    const match = md5(bytes) === await driver.flashMd5(offset, bytes.length);
    diagnostics?.check(label + "Md5Match", match);
    requireSafe(match, "READBACK");
  }
  try {
    return await inspectOriginalFlash(driver, table, ota);
  } finally {
    table.fill(0);
    ota.fill(0);
  }
}
var RecoverySession = class {
  #driver;
  #diagnostics;
  #build;
  #source;
  #handle;
  #identity;
  #romInfo;
  #beforeMd5;
  #capture;
  #busy = false;
  #closed = false;
  #consent;
  #mode;
  state = "IDLE";
  constructor(driver, { diagnostics, build, sourceCommit, mode = "backup" } = {}) {
    requireSafe(["backup", "restore", "verify"].includes(mode), "STATE");
    this.#driver = driver;
    this.#diagnostics = diagnostics;
    this.#build = build;
    this.#source = sourceCommit;
    this.#mode = mode;
  }
  #stage(s) {
    this.state = s;
    this.#diagnostics?.stage(s);
  }
  #available() {
    requireSafe(!this.#closed && !this.#busy && this.state !== "STOP", "STATE");
  }
  async selectPackage(input) {
    this.#available();
    if (!["IDLE", "PACKAGE_VALIDATED"].includes(this.state)) {
      this.cancel("PACKAGE_CHANGED");
      destroyRecoveryPackage(this.#handle);
      this.#handle = void 0;
      throw new Stop("STATE");
    }
    this.#busy = true;
    destroyRecoveryPackage(this.#handle);
    this.#handle = void 0;
    this.#stage("IDLE");
    try {
      this.#handle = await validateRecoveryPackage(input);
      requireSafe(!this.#closed, "CANCELLED");
      this.#diagnostics?.check("packageValidated", true);
      this.#stage("PACKAGE_VALIDATED");
      return recoverySummary(this.#handle);
    } finally {
      this.#busy = false;
      if (this.#closed) {
        destroyRecoveryPackage(this.#handle);
        this.#handle = void 0;
      }
    }
  }
  async prepare({ boardConfirmed, tfRemoved } = {}) {
    this.#available();
    requireSafe(boardConfirmed === true && tfRemoved === true, "BOARD_CONFIRM");
    requireSafe(this.#mode === "backup" ? this.state === "IDLE" : this.state === "PACKAGE_VALIDATED", "STATE");
    this.#busy = true;
    this.#consent = { boardConfirmed, tfRemoved };
    try {
      this.#stage("CONNECTING");
      await this.#driver.connect();
      this.#romInfo = await this.#driver.identity();
      this.#identity = checkedIdentity(this.#romInfo);
      if (this.#mode === "backup") {
        this.#beforeMd5 = await this.#driver.flashMd5(0, PINS.flashBytes);
        const original = await inspectSupportedOriginal(this.#driver, this.#diagnostics);
        this.#diagnostics?.check("originalState", original);
        this.#stage("DEVICE_VERIFIED");
        return original;
      }
      const { manifest, image } = recoverySnapshot(this.#handle);
      image.fill(0);
      sameOriginalIdentity(this.#romInfo, manifest.device);
      this.#diagnostics?.check("originalIdentityMatch", true);
      const plan = { mode: this.#mode, offset: 0, bytes: PINS.flashBytes, original: manifest.original.firmware, currentImageMayDiffer: true, noReset: true, noStub: true };
      this.#diagnostics?.check("plan", plan);
      this.#diagnostics?.check("originalState", { firmware: manifest.original.firmware, activeApp: manifest.original.activeApp });
      this.#stage(this.#mode === "restore" ? "READY_FOR_CONFIRMATION" : "DEVICE_VERIFIED");
      return plan;
    } catch (error) {
      this.#stop(error);
      throw error;
    } finally {
      this.#busy = false;
    }
  }
  async #package(bytes, method) {
    sameOriginalIdentity(await this.#driver.identity(), this.#identity);
    const after = await this.#driver.flashMd5(0, PINS.flashBytes);
    const equal = after === this.#beforeMd5 && md5(bytes) === after;
    this.#diagnostics?.check("fullFlashMd5Match", equal);
    requireSafe(equal, "BACKUP_STALE");
    const result = await createRecoveryPackage(bytes, this.#romInfo, { ...this.#consent, romMd5Before: this.#beforeMd5, romMd5After: after, method, build: this.#build, sourceCommit: this.#source });
    requireSafe(!this.#closed && this.state !== "STOP", "CANCELLED");
    this.#stage("SAVED_FILE_REIMPORT_REQUIRED");
    return result;
  }
  async useExistingRaw(input) {
    this.#available();
    requireSafe(this.#mode === "backup" && this.state === "DEVICE_VERIFIED", "STATE");
    requireSafe(input instanceof Uint8Array && input.length === PINS.flashBytes && md5(input) === this.#beforeMd5, "BACKUP_STALE");
    this.#busy = true;
    this.#capture = input.slice();
    try {
      return await this.#package(this.#capture, "ROM_MD5_MATCH_EXISTING_V1");
    } catch (error) {
      this.#stop(error);
      throw error;
    } finally {
      this.#capture?.fill(0);
      this.#capture = void 0;
      this.#busy = false;
    }
  }
  async capture({ progress = () => {
  } } = {}) {
    this.#available();
    requireSafe(this.#mode === "backup" && this.state === "DEVICE_VERIFIED", "STATE");
    this.#busy = true;
    this.#stage("READING_BACKUP");
    this.#capture = new Uint8Array(PINS.flashBytes);
    try {
      requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === this.#beforeMd5, "BACKUP_STALE");
      for (let at = 0; at < PINS.flashBytes; at += 65536) {
        const part = await this.#driver.readFlash(at, 65536);
        requireSafe(part.length === 65536, "READBACK");
        requireSafe(!this.#closed && this.state !== "STOP", "CANCELLED");
        this.#capture.set(part, at);
        part.fill(0);
        progress((at + 65536) / PINS.flashBytes);
      }
      return await this.#package(this.#capture, "ROM_READ_SLOW_MD5_V1");
    } catch (error) {
      this.#stop(error);
      throw error;
    } finally {
      this.#capture?.fill(0);
      this.#capture = void 0;
      this.#busy = false;
    }
  }
  async verifySavedPackage(input) {
    this.#available();
    requireSafe(this.#mode === "backup" && ["SAVED_FILE_REIMPORT_REQUIRED", "DEVICE_VERIFIED"].includes(this.state), "STATE");
    this.#busy = true;
    let handle, communicating = false;
    try {
      handle = await validateRecoveryPackage(input);
      const { manifest, image } = recoverySnapshot(handle);
      image.fill(0);
      communicating = true;
      sameOriginalIdentity(await this.#driver.identity(), manifest.device);
      sameOriginalIdentity(this.#romInfo, manifest.device);
      requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === manifest.image.md5 && manifest.image.md5 === this.#beforeMd5, "BACKUP_STALE");
      this.#diagnostics?.check("packageValidated", true);
      this.#diagnostics?.check("savedPackageReimported", true);
      this.#diagnostics?.check("fullFlashMd5Match", true);
      this.#stage("BACKUP_SAVED_VERIFIED");
      return recoverySummary(handle);
    } catch (error) {
      if (communicating) this.#stop(error);
      throw error;
    } finally {
      destroyRecoveryPackage(handle);
      this.#busy = false;
    }
  }
  async restore({ restoreConfirmed, progress } = {}) {
    this.#available();
    requireSafe(this.#mode === "restore" && this.state === "READY_FOR_CONFIRMATION", "STATE");
    requireSafe(restoreConfirmed === true, "RESTORE_CONFIRM");
    this.#busy = true;
    try {
      this.#stage("RECHECKING");
      await this.#driver.grantRecovery(this.#handle, { ...this.#consent, restoreConfirmed });
      const result = await this.#driver.restoreRecovery({ progress });
      this.#stage("VERIFIED_BOOT_PENDING");
      return result;
    } catch (error) {
      this.#stop(error);
      throw error;
    } finally {
      this.#busy = false;
    }
  }
  async verifyAgain() {
    this.#available();
    requireSafe(this.#mode === "verify" && this.state === "DEVICE_VERIFIED", "STATE");
    this.#busy = true;
    try {
      this.#stage("VERIFYING");
      const result = await this.#driver.verifyRecoveryImage(this.#handle);
      this.#stage("VERIFIED_BOOT_PENDING");
      return result;
    } catch (error) {
      this.#stop(error);
      throw error;
    } finally {
      this.#busy = false;
    }
  }
  #stop(error) {
    this.#diagnostics?.error(error);
    this.#stage("STOP");
    this.#driver.cancel();
  }
  cancel(code = "CANCELLED") {
    if (!this.#closed && this.state !== "STOP") this.#stop(new Stop(code));
  }
  async close() {
    if (this.#busy) this.cancel();
    this.#closed = true;
    try {
      await this.#driver.disconnect();
    } finally {
      destroyRecoveryPackage(this.#handle);
      this.#handle = void 0;
      this.#capture?.fill(0);
      this.#capture = void 0;
      this.#identity = void 0;
      this.#romInfo = void 0;
      this.#beforeMd5 = void 0;
      this.#consent = void 0;
      this.#diagnostics?.check("cleanup", { sessionPrivateReferencesCleared: true, physicalMemoryErasureGuaranteed: false });
    }
  }
};

// src/install-protection.js
var APP_END = 65536 + Math.ceil(PINS.files["app.bin"].bytes / 4096) * 4096;
var PRIVATE_RANGES = [[0, 65536], [APP_END, PINS.flashBytes - APP_END]];
var te3 = new TextEncoder();
var td3 = new TextDecoder("utf-8", { fatal: true });
var live = /* @__PURE__ */ new WeakMap();
function savedProtection(handle, { mode, mac } = {}) {
  const x = live.get(handle);
  requireSafe(x?.saved, "PROTECTION_SAVE");
  requireSafe(x.point.mode === mode && x.point.device.mac === mac, "PROTECTION_SCOPE");
  return structuredClone(x.point);
}

// src/session.js
var UPDATE_NEVER = ["bootloader", "partition table", "otadata", "NVS / pairing", "inactive App", "ime_ro", "ffat", "coredump", "TF card"];
var InstallSession = class {
  #driver;
  #release;
  #plan;
  #fingerprint;
  #mac;
  #busy = false;
  #writesEnabled;
  #state = "IDLE";
  #receipt;
  #emit;
  #cancelled = false;
  #freshEnabled;
  #bindSelectedIdentity;
  #candidate;
  #backupMd5;
  #capturedSha256;
  #diagnostics;
  #productProtection;
  #allowCleanInstall;
  #initialMd5;
  constructor(driver, release, { hardwareWritesEnabled = false, hardwareFreshWritesEnabled = false, bindSelectedIdentity = false, candidate = false, productProtection = false, allowCleanInstall = false, diagnostics, onState = () => {
  } } = {}) {
    this.#diagnostics = diagnostics;
    requireSafe(!(hardwareWritesEnabled && hardwareFreshWritesEnabled), "BUILD_SCOPE");
    requireSafe(!(candidate && (hardwareWritesEnabled || hardwareFreshWritesEnabled)), "BUILD_SCOPE");
    requireSafe(!productProtection || candidate && bindSelectedIdentity, "BUILD_SCOPE");
    this.#productProtection = productProtection;
    requireSafe(!allowCleanInstall || candidate && !productProtection, "BUILD_SCOPE");
    this.#allowCleanInstall = allowCleanInstall;
    this.#candidate = candidate;
    this.#freshEnabled = hardwareFreshWritesEnabled;
    this.#bindSelectedIdentity = bindSelectedIdentity;
    this.#driver = driver;
    this.#release = release;
    this.#writesEnabled = hardwareWritesEnabled;
    this.#emit = onState;
    this.#receipt = {
      status: "IDLE",
      simulation: driver.kind === "simulation",
      release: PINS.version,
      appSha256: PINS.files["app.bin"].sha256,
      eraseStarted: false,
      writeStarted: false,
      independentVerification: false,
      resetIssued: false,
      deviceQualification: "DEVICE_PENDING",
      tfResources: "NOT_CHECKED"
    };
  }
  get state() {
    return this.#state;
  }
  get plan() {
    return this.#plan;
  }
  get selectedIdentity() {
    return this.#mac;
  }
  // Display only, never included in receipts.
  get backupVerified() {
    return this.#backupMd5 !== void 0;
  }
  receipt() {
    return structuredClone({ ...this.#receipt, mutationStarted: this.#driver.flashMutationStarted ?? (this.#receipt.eraseStarted || this.#receipt.writeStarted), ...this.#diagnostics ? { diagnostics: this.#diagnostics.snapshot() } : {} });
  }
  #set(status) {
    this.#state = status;
    this.#receipt.status = status;
    this.#diagnostics?.stage(status);
    this.#emit(status, this.receipt());
  }
  #alive() {
    requireSafe(!this.#cancelled, "CANCELLED");
  }
  async #run(fn) {
    requireSafe(!this.#busy, "BUSY");
    this.#busy = true;
    try {
      this.#alive();
      return await fn();
    } catch (error) {
      const code = this.#cancelled ? this.#receipt.error || "CANCELLED" : error instanceof Stop ? error.code : "IO";
      this.#receipt.error = code;
      this.#diagnostics?.error(new Stop(code));
      if (this.receipt().mutationStarted) this.#receipt.flashOutcome = "POSSIBLY_PARTIAL";
      this.#set("STOP");
      throw new Stop(code);
    } finally {
      this.#busy = false;
    }
  }
  async #identity() {
    this.#alive();
    const info = await this.#driver.identity();
    requireSafe(info.chip === "ESP32-S3", "CHIP");
    requireSafe(info.flashBytes === PINS.flashBytes, "FLASH");
    requireSafe(info.securityKnown === true && info.secureBoot === false && info.flashEncryption === false, "SECURITY");
    const actual = normalizeMac(info.mac);
    if (this.#mac === void 0 && this.#bindSelectedIdentity) this.#mac = actual;
    requireSafe(actual === this.#mac, "IDENTITY");
    this.#diagnostics?.check("originalIdentityMatch", true);
    return info;
  }
  async #metadata() {
    this.#alive();
    const table = await this.#driver.readFlash(32768, 4096);
    this.#diagnostics?.check("partitionRead", true);
    this.#alive();
    const ota = await this.#driver.readFlash(57344, 8192);
    this.#diagnostics?.check("otaRead", true);
    requireSafe(table.length === 4096 && ota.length === 8192, "READBACK");
    const tableMatch = await this.#driver.flashMd5(32768, 4096) === md5(table);
    this.#diagnostics?.check("partitionMd5Match", tableMatch);
    requireSafe(tableMatch, "READBACK");
    const otaMatch = await this.#driver.flashMd5(57344, 8192) === md5(ota);
    this.#diagnostics?.check("otaMd5Match", otaMatch);
    requireSafe(otaMatch, "READBACK");
    this.#alive();
    return { table, ota, fingerprint: `${await sha256(table)}:${await sha256(ota)}` };
  }
  async prepare({ boardConfirmed, expectedMac }) {
    return this.#run(async () => {
      requireSafe(this.#state === "IDLE", "STATE");
      requireSafe(releaseIsVerified(this.#release), "ASSET");
      requireSafe(boardConfirmed === true, "BOARD");
      this.#mac = this.#bindSelectedIdentity ? void 0 : normalizeMac(expectedMac);
      this.#set("CONNECTING");
      await this.#driver.connect();
      this.#set("CHECKING");
      await this.#identity();
      const { table, ota, fingerprint } = await this.#metadata();
      let mode = "update", slot, initialState = "compatible-draftor";
      if (this.#allowCleanInstall) {
        const original = await inspectOriginalFlash(this.#driver, table, ota);
        this.#initialMd5 = await this.#driver.flashMd5(0, PINS.flashBytes);
        if (original.firmware === "draftor-alpha3-app0") slot = 0;
        else {
          mode = "fresh";
          slot = 0;
          initialState = original.firmware;
          if (initialState === "blank") this.#backupMd5 = PINS.blankFlashMd5;
        }
      } else if (table.every((v) => v === 255) && ota.every((v) => v === 255)) {
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "UNKNOWN");
        mode = "fresh";
        slot = 0;
        initialState = "blank";
      } else if (await isFactoryTable(table)) {
        initialState = "waveshare-factory-20260120";
        await verifyInitialFlash(this.#driver, initialState);
        mode = "fresh";
        slot = 0;
      } else {
        tableMatchesRelease(this.#release, table);
        slot = activeSlot(ota);
      }
      if (this.#candidate && mode === "update") {
        requireSafe(slot === 0, "UNQUALIFIED_SLOT");
        requireSafe(await this.#driver.flashMd5(65536, PINS.files["app.bin"].bytes) === PINS.files["app.bin"].md5, "UNQUALIFIED_FIRMWARE");
      }
      const offset = slot === 0 ? 65536 : 4259840;
      let targetMd5;
      if (mode === "fresh") {
        const initialized = new Uint8Array(PINS.flashBytes).fill(255);
        for (const name of FIRST_FILES) initialized.set(releaseFile(this.#release, name), PINS.files[name].offset);
        targetMd5 = md5(initialized);
        initialized.fill(0);
      }
      const writes = mode === "update" ? [{ name: "app.bin", offset, bytes: PINS.files["app.bin"].bytes }] : FIRST_FILES.map((name) => ({ name, offset: PINS.files[name].offset, bytes: PINS.files[name].bytes }));
      this.#fingerprint = fingerprint;
      this.#plan = deepFreeze({
        mode,
        initialState,
        appSlot: `app${slot}`,
        appOffset: hex(offset),
        appBytes: PINS.files["app.bin"].bytes,
        appSha256: PINS.files["app.bin"].sha256,
        writes: writes.map((w) => ({ ...w, offsetHex: hex(w.offset), endExclusive: hex(w.offset + w.bytes), sectorEndExclusive: hex(w.offset + Math.ceil(w.bytes / 4096) * 4096) })),
        fullErase: mode === "fresh",
        atomicUpdate: false,
        interruption: this.#productProtection ? "Flash may be partial; stop without retry, alternate slot or reboot. Use saved installation protection to prepare exact original recovery, or an independent full backup." : "Active App may be partial; no automatic retry, alternate slot or reboot. Recover from verified private backups.",
        neverWrite: mode === "update" ? UPDATE_NEVER : ["TF card"],
        backupPolicy: this.#allowCleanInstall && initialState === "blank" ? "PROVEN_BLANK_NO_BACKUP_REQUIRED" : "SAVED_ORIGINAL_REQUIRED",
        requires: this.#productProtection ? ["physical Waveshare ST7305", "exact supported original state and same ROM identity", "installation protection file saved and actual bytes read back", "TF removed", mode === "fresh" ? "explicit full internal Flash erase approval" : "explicit App-only reinstall approval"] : mode === "update" ? ["physical Waveshare board", this.#bindSelectedIdentity ? "same selected USB identity bound to this session" : "matching private MAC", "verified recoverable Flash and TF backups", "TF removed", "explicit plan approval"] : [this.#allowCleanInstall ? "supported hardware and current original full image" : "proven blank or exact pinned factory fingerprint", this.#allowCleanInstall && initialState === "blank" ? "complete ROM blank proof; no original contents to back up" : "verified recoverable full Flash backup", "explicit full internal Flash erase approval", "TF removed", "ordinary boot qualification pending"],
        ...mode === "fresh" ? {
          eraseRange: { offset: 0, bytes: PINS.flashBytes },
          eraseMethod: "ROM FLASH_BEGIN then complete all-FF MD5",
          factoryCommit: initialState === "waveshare-factory-20260120" ? FACTORY.commit : null,
          clears: ["entire internal Flash", "old firmware", "settings", "NVS and pairing", "internal filesystem"],
          preservedOutsideFlash: ["TF card", "eFuses"],
          expectedFullMd5: targetMd5
        } : {},
        hardwareWriteAllowed: this.#candidate ? this.#allowCleanInstall || initialState !== "blank" : mode === "update" ? this.#writesEnabled : this.#freshEnabled,
        ...this.#candidate ? { candidateOnly: true, publicApproval: "PENDING", backupSavedFileReimportRequired: !this.#productProtection && !(this.#allowCleanInstall && initialState === "blank"), supportedUpdate: "exact alpha.3 app0 same-version only", blankHardwareQualification: this.#allowCleanInstall ? "HARDWARE_CHECKED_DEVICE_TEST_PENDING" : "UNVERIFIED_LOCKED" } : {},
        ...this.#productProtection ? { protectionPolicy: "R12_MINIMAL_PRIVATE_AND_ORIGINAL_STATE_V1", fullBackupRequired: false, offlineToolRequired: false, installationProtectionSavedReadbackRequired: true, postVerifyProtectedFrom: mode === "update" ? hex(APP_END) : null } : {},
        deviceQualification: "DEVICE_PENDING",
        tfResources: "NOT_CHECKED"
      });
      this.#receipt.plan = this.#plan;
      this.#diagnostics?.check("plan", this.#plan);
      this.#diagnostics?.check("writeLock", !this.#plan.hardwareWriteAllowed);
      this.#set("PLAN_READY");
      return this.#plan;
    });
  }
  async #samePlannedDevice() {
    await this.#identity();
    requireSafe((await this.#metadata()).fingerprint === this.#fingerprint, "STALE_PLAN");
    if (this.#allowCleanInstall) requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === this.#initialMd5, "BACKUP_STALE");
  }
  async captureBackup() {
    return this.#run(async () => {
      requireSafe(this.#candidate && this.#state === "PLAN_READY", "STATE");
      this.#set("BACKUP_READING");
      await this.#samePlannedDevice();
      const before = await this.#driver.flashMd5(0, PINS.flashBytes);
      const bytes = new Uint8Array(PINS.flashBytes);
      for (let at = 0; at < bytes.length; at += 65536) {
        this.#alive();
        const chunk = await this.#driver.readFlash(at, 65536);
        requireSafe(chunk.length === 65536, "READBACK");
        bytes.set(chunk, at);
        this.#emit("BACKUP_READING", { ...this.receipt(), backupProgress: (at + chunk.length) / bytes.length });
      }
      await this.#samePlannedDevice();
      requireSafe(md5(bytes) === before && await this.#driver.flashMd5(0, bytes.length) === before, "READBACK");
      this.#capturedSha256 = await sha256(bytes);
      this.#backupMd5 = void 0;
      this.#receipt.backup = "SAVED_FILE_REIMPORT_REQUIRED";
      this.#set("PLAN_READY");
      return bytes;
    });
  }
  async verifySavedBackup(input) {
    return this.#run(async () => {
      requireSafe(this.#candidate && this.#state === "PLAN_READY", "STATE");
      requireSafe(input instanceof Uint8Array && input.length === PINS.flashBytes, "BACKUP_FILE");
      const bytes = input.slice();
      this.#set("BACKUP_VERIFYING");
      await this.#samePlannedDevice();
      const digest = md5(bytes);
      if (this.#capturedSha256) requireSafe(await sha256(bytes) === this.#capturedSha256, "BACKUP_FILE");
      requireSafe(await this.#driver.flashMd5(0, bytes.length) === digest, "BACKUP_FILE");
      this.#backupMd5 = digest;
      this.#receipt.backup = "SAVED_FILE_REIMPORTED_AND_DEVICE_MD5_MATCHED";
      this.#set("PLAN_READY");
      return true;
    });
  }
  async execute({ backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed = false, currentApp, protection } = {}) {
    return this.#run(async () => {
      requireSafe(this.#state === "PLAN_READY", "STATE");
      requireSafe(tfRemoved === true && planConfirmed === true, "BACKUP");
      const protectedPoint = this.#productProtection ? savedProtection(protection, { mode: this.#plan.mode, mac: this.#mac }) : void 0;
      requireSafe(this.#productProtection || backupConfirmed === true && protection === void 0, "BACKUP");
      const fresh = this.#plan.mode === "fresh";
      requireSafe(fresh || eraseConfirmed === false, "ERASE_FORBIDDEN");
      requireSafe(!fresh || eraseConfirmed === true, "ERASE_CONFIRM");
      if (this.#candidate) {
        requireSafe(this.#allowCleanInstall || this.#plan.initialState !== "blank", "UNQUALIFIED_BLANK");
        requireSafe(this.#productProtection || this.backupVerified, "BACKUP_FILE");
      } else requireSafe(this.#driver.kind === "simulation" || (fresh ? this.#freshEnabled : this.#writesEnabled), fresh ? "FRESH_DEVICE_PENDING" : "DEVICE_WRITES_LOCKED");
      this.#set("RECHECKING");
      await this.#identity();
      const metadata = await this.#metadata();
      requireSafe(metadata.fingerprint === this.#fingerprint, "STALE_PLAN");
      if (fresh && !this.#allowCleanInstall) await verifyInitialFlash(this.#driver, this.#plan.initialState);
      if (!fresh) {
        tableMatchesRelease(this.#release, metadata.table);
        requireSafe(`app${activeSlot(metadata.ota)}` === this.#plan.appSlot, "STALE_PLAN");
      }
      const offset = Number(this.#plan.appOffset);
      if (currentApp) {
        requireSafe(!fresh && currentApp instanceof Uint8Array && currentApp.length >= 24 && currentApp.length <= PINS.slotCapacity, "CURRENT_APP");
        const previous = currentApp.slice();
        requireSafe(await this.#driver.flashMd5(offset, previous.length) === md5(previous), "CURRENT_APP");
      }
      const protectedEnd = this.#productProtection ? APP_END : offset + PINS.slotCapacity;
      const protectedRanges = fresh ? [] : [[0, offset], [protectedEnd, PINS.flashBytes - protectedEnd]];
      const before = [];
      for (const [start, size] of protectedRanges) before.push(await this.#driver.flashMd5(start, size));
      this.#alive();
      if (this.#candidate) {
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === (protectedPoint?.originalMd5 ?? this.#backupMd5), this.#productProtection ? "PROTECTION_STALE" : "BACKUP_STALE");
        await this.#driver.grantExclusiveMode({ mode: this.#plan.mode, initialState: this.#plan.initialState, boardConfirmed: true, cleanInstall: this.#allowCleanInstall, expectedMac: this.#mac, metadataFingerprint: this.#fingerprint, backupMd5: this.#backupMd5, protection });
      }
      if (fresh) {
        this.#receipt.eraseStarted = true;
        this.#set("ERASING");
        await this.#driver.eraseAll({ source: this.#plan.initialState, cleanInstall: this.#allowCleanInstall, expectedOriginalMd5: this.#backupMd5, expectedMac: this.#mac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed, protection });
        this.#alive();
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "ERASE_VERIFY");
        this.#receipt.erased = true;
      }
      this.#receipt.writeStarted = true;
      this.#set("WRITING");
      for (const row of this.#plan.writes) {
        this.#alive();
        const data = releaseFile(this.#release, row.name);
        requireSafe(data.length === row.bytes && await sha256(data) === PINS.files[row.name].sha256, "ASSET");
        await this.#driver.writeImage(row.offset, data, { mode: this.#plan.mode, slotOffset: offset, progress: (value) => this.#emit("WRITING", { ...this.receipt(), progress: value }) });
        if (fresh) {
          this.#alive();
          requireSafe(await this.#driver.flashMd5(row.offset, row.bytes) === PINS.files[row.name].md5, "VERIFY");
        }
      }
      this.#set("VERIFYING");
      for (const row of this.#plan.writes) {
        this.#alive();
        requireSafe(await this.#driver.flashMd5(row.offset, row.bytes) === PINS.files[row.name].md5, "VERIFY");
      }
      if (fresh) {
        const after = await this.#metadata();
        tableMatchesRelease(this.#release, after.table);
        requireSafe(after.table.slice(3072).every((b) => b === 255) && bytesEqual(after.ota, releaseFile(this.#release, "boot_app0.bin")) && activeSlot(after.ota) === 0, "READBACK");
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === this.#plan.expectedFullMd5, "INITIALIZATION_VERIFY");
        await this.#identity();
        this.#receipt.fullFlashInitialized = true;
        this.#receipt.partitionOtaReadback = true;
      } else {
        const after = await this.#metadata();
        requireSafe(after.fingerprint === this.#fingerprint, "PROTECTED_CHANGED");
        for (let i = 0; i < protectedRanges.length; i++) {
          const [start, size] = protectedRanges[i];
          requireSafe(await this.#driver.flashMd5(start, size) === before[i], "PROTECTED_CHANGED");
        }
        this.#receipt.protectedRegionsUnchanged = true;
      }
      this.#alive();
      this.#receipt.independentVerification = true;
      this.#receipt.flashOutcome = "VERIFIED_CONTENT_BOOT_PENDING";
      this.#set("VERIFIED_BOOT_PENDING");
      return this.receipt();
    });
  }
  cancel(code = "CANCELLED") {
    this.#cancelled = true;
    this.#mac = void 0;
    this.#backupMd5 = void 0;
    this.#capturedSha256 = void 0;
    this.#fingerprint = void 0;
    this.#initialMd5 = void 0;
    this.#driver.cancel?.();
    this.#receipt.error = code;
    if (this.receipt().mutationStarted) this.#receipt.flashOutcome = "POSSIBLY_PARTIAL";
    this.#set("STOP");
  }
  async close() {
    this.#mac = void 0;
    this.#backupMd5 = void 0;
    this.#capturedSha256 = void 0;
    this.#fingerprint = void 0;
    this.#initialMd5 = void 0;
    try {
      await this.#driver.disconnect();
    } finally {
      this.#diagnostics?.check("cleanup", { sessionPrivateReferencesCleared: true, physicalMemoryErasureGuaranteed: false });
    }
  }
};

// src/public-install-session.js
function publicPlan(plan) {
  if (!plan) return void 0;
  const { candidateOnly, publicApproval, blankHardwareQualification, deviceQualification, ...p } = structuredClone(plan);
  p.releaseScope = "WAVESHARE_ST7305_CLEAN_INSTALL_OR_ALPHA3_APP0_REINSTALL";
  p.qualificationEvidence = p.mode === "fresh" ? "GENERIC_CLEAN_SOFTWARE_TESTED_HARDWARE_PENDING" : "R4_APP0_SAME_VERSION_INHERITED";
  p.runtimeWritePermission = "NOT_GRANTED_BY_PLAN";
  return deepFreeze(p);
}
var PublicInstallSession = class {
  #driver;
  #install;
  #backup;
  #identity;
  #diag;
  #emit;
  #build;
  #source;
  #toolHash;
  #busy = false;
  #closed = false;
  #stopped = false;
  #phase;
  #saved = false;
  #tool = false;
  constructor(driver, release, { diagnostics, onState = () => {
  }, build, sourceCommit, offlineToolSha256, platformQualified = false } = {}) {
    requireSafe(platformQualified === true, "PLATFORM");
    requireSafe(/^[0-9a-f]{64}$/.test(offlineToolSha256), "ASSET");
    this.#driver = driver;
    this.#diag = diagnostics;
    this.#emit = onState;
    this.#build = build;
    this.#source = sourceCommit;
    this.#toolHash = offlineToolSha256;
    this.#diag?.check("writeLock", true);
    this.#install = new InstallSession(driver, release, {
      candidate: true,
      bindSelectedIdentity: true,
      allowCleanInstall: true,
      diagnostics,
      onState: (phase, receipt) => {
        this.#diag?.check("writeLock", !["ERASING", "WRITING", "VERIFYING"].includes(phase));
        if (this.#install?.plan) this.#diag?.check("plan", this.plan);
        this.#emit(phase, this.receipt(receipt));
      }
    });
  }
  get state() {
    return this.#stopped ? "STOP" : this.#phase || this.#install.state;
  }
  get plan() {
    return publicPlan(this.#install.plan);
  }
  get selectedIdentity() {
    return this.#closed ? void 0 : this.#install.selectedIdentity;
  }
  get backupVerified() {
    return !this.#closed && !this.#stopped && (this.#saved || this.plan?.initialState === "blank") && this.#install.backupVerified;
  }
  get offlineToolVerified() {
    return !this.#closed && !this.#stopped && this.#tool;
  }
  receipt(existing) {
    const r = existing || this.#install?.receipt() || {};
    return structuredClone({
      ...r,
      plan: this.plan,
      backup: this.plan?.initialState === "blank" ? "PROVEN_BLANK_NO_BACKUP_REQUIRED" : this.#saved ? "R9_SAVED_PACKAGE_REIMPORTED_AND_CURRENT_FLASH_MATCHED" : r.backup,
      offlineTool: this.#tool ? "EXACT_DISTRIBUTED_HTML_HASH_VERIFIED" : "NOT_VERIFIED",
      qualificationEvidence: this.plan?.qualificationEvidence,
      deviceQualification: this.plan?.qualificationEvidence,
      ...this.#diag ? { diagnostics: this.#diag.snapshot() } : {},
      status: this.state
    });
  }
  #ready() {
    requireSafe(!this.#closed && !this.#stopped, "STATE");
    requireSafe(!this.#busy, "BUSY");
  }
  #phaseChanged(phase, progress) {
    this.#phase = phase;
    this.#diag?.stage(phase);
    this.#emit(phase, { ...this.receipt(), ...progress === void 0 ? {} : { backupProgress: progress } });
  }
  async prepare({ boardConfirmed, tfRemoved } = {}) {
    this.#ready();
    requireSafe(tfRemoved === true, "BACKUP");
    this.#busy = true;
    try {
      const p = await this.#install.prepare({ boardConfirmed });
      requireSafe(p.hardwareWriteAllowed, "DEVICE_WRITES_LOCKED");
      this.#identity = checkedIdentity(await this.#driver.identity());
      const original = this.#identity, driver = this.#driver, owner = this.#install;
      const borrowedReads = {
        kind: driver.kind,
        connect: async () => requireSafe(owner.state === "PLAN_READY", "STATE"),
        identity: async () => {
          const info = await driver.identity();
          sameOriginalIdentity(info, original);
          return info;
        },
        readFlash: (...args) => driver.readFlash(...args),
        flashMd5: (...args) => driver.flashMd5(...args),
        cancel: () => driver.cancel(),
        disconnect: async () => {
        }
      };
      this.#backup = new RecoverySession(borrowedReads, { mode: "backup", diagnostics: this.#diag, build: this.#build, sourceCommit: this.#source });
      const state = await this.#backup.prepare({ boardConfirmed, tfRemoved });
      requireSafe(state.firmware === (p.mode === "fresh" ? p.initialState : "draftor-alpha3-app0"), "STALE_PLAN");
      this.#diag?.check("originalIdentityMatch", true);
      this.#diag?.check("plan", this.plan);
      this.#diag?.check("writeLock", true);
      this.#phaseChanged("PLAN_READY");
      return this.plan;
    } catch (e) {
      this.cancel(e.code || "IO");
      throw e;
    } finally {
      this.#busy = false;
    }
  }
  async captureBackup() {
    this.#ready();
    requireSafe(this.state === "PLAN_READY" && !this.#saved, "STATE");
    this.#busy = true;
    try {
      this.#phaseChanged("BACKUP_READING");
      const bytes = await this.#backup.capture({ progress: (p) => this.#phaseChanged("BACKUP_READING", p) });
      this.#phaseChanged("PLAN_READY");
      return bytes;
    } catch (e) {
      this.cancel(e.code || "IO");
      throw e;
    } finally {
      this.#busy = false;
    }
  }
  async importExistingRaw(input) {
    this.#ready();
    requireSafe(this.state === "PLAN_READY" && !this.#saved, "STATE");
    this.#busy = true;
    try {
      const bytes = await this.#backup.useExistingRaw(input);
      this.#phaseChanged("PLAN_READY");
      return bytes;
    } catch (e) {
      if (this.#backup.state === "STOP") this.cancel(e.code || "IO");
      throw e;
    } finally {
      this.#busy = false;
    }
  }
  invalidateSavedPackage() {
    this.#saved = false;
    this.#diag?.check("savedPackageReimported", "NOT_RUN");
    if (this.#busy || !["PLAN_READY", "BACKUP_VERIFYING"].includes(this.state)) this.cancel("PACKAGE_CHANGED");
  }
  async verifySavedBackup(input) {
    this.invalidateSavedPackage();
    this.#ready();
    requireSafe(this.state === "PLAN_READY", "STATE");
    this.#busy = true;
    let h, image, communicating = false;
    try {
      h = await validateRecoveryPackage(input);
      requireSafe(!this.#closed && !this.#stopped, "CANCELLED");
      const snap = recoverySnapshot(h);
      image = snap.image;
      this.#phaseChanged("BACKUP_VERIFYING");
      communicating = true;
      const info = await this.#driver.identity();
      sameOriginalIdentity(info, this.#identity);
      sameOriginalIdentity(info, snap.manifest.device);
      requireSafe(snap.manifest.original.firmware === (this.plan.mode === "fresh" ? this.plan.initialState : "draftor-alpha3-app0"), "PACKAGE_METADATA");
      await this.#install.verifySavedBackup(image);
      sameOriginalIdentity(await this.#driver.identity(), snap.manifest.device);
      this.#saved = true;
      this.#diag?.check("packageValidated", true);
      this.#diag?.check("savedPackageReimported", true);
      this.#diag?.check("fullFlashMd5Match", true);
      this.#diag?.check("originalIdentityMatch", true);
      this.#phaseChanged("PLAN_READY");
      return true;
    } catch (e) {
      if (communicating) this.cancel(e.code || "IO");
      throw e;
    } finally {
      image?.fill(0);
      destroyRecoveryPackage(h);
      this.#busy = false;
    }
  }
  async verifyOfflineTool(bytes) {
    this.#ready();
    this.#tool = false;
    requireSafe(bytes instanceof Uint8Array && await sha256(bytes) === this.#toolHash, "RECOVERY_TOOL");
    requireSafe(!this.#closed && !this.#stopped, "STATE");
    this.#tool = true;
    return true;
  }
  invalidateOfflineTool() {
    this.#tool = false;
  }
  async execute({ offlineToolSaved, ...consent } = {}) {
    this.#ready();
    requireSafe(this.backupVerified, "BACKUP_FILE");
    requireSafe(this.plan?.initialState === "blank" || this.#tool && offlineToolSaved === true, "RECOVERY_TOOL");
    this.#busy = true;
    this.#phase = void 0;
    try {
      sameOriginalIdentity(await this.#driver.identity(), this.#identity);
      const result = await this.#install.execute(consent);
      return this.receipt(result);
    } catch (e) {
      this.cancel(e.code || "IO");
      throw e;
    } finally {
      this.#busy = false;
      this.#saved = false;
      this.#tool = false;
    }
  }
  cancel(code = "CANCELLED") {
    this.#diag?.check("writeLock", true);
    this.#stopped = true;
    this.#saved = false;
    this.#tool = false;
    this.#install.cancel(code);
  }
  async close() {
    if (this.#closed) return;
    this.#diag?.check("writeLock", true);
    this.#closed = true;
    this.#saved = false;
    this.#tool = false;
    this.#identity = void 0;
    try {
      await this.#backup?.close();
    } finally {
      await this.#install.close();
      this.#backup = void 0;
    }
  }
};

// src/diagnostics.js
var OPS = { 2: "FLASH_BEGIN", 3: "FLASH_DATA", 8: "SYNC", 9: "WRITE_REG", 10: "READ_REG", 11: "SPI_PARAMS", 13: "SPI_ATTACH", 14: "READ_FLASH_SLOW", 19: "MD5", 20: "SECURITY" };
var EVENTS = ["requestPortAttempts", "requestPortCancelled", "requestPortErrors", "openAttempts", "openSuccess", "openErrors", "closeAttempts", "closeSuccess", "closeErrors", "manualReconnects", "cancelRequests", "stubRequests", "resetRequests", "blockedWriteRequests"];
var CHECKS = ["chip", "flashBytes", "security", "originalIdentityMatch", "partitionRead", "partitionMd5Match", "otaRead", "otaMd5Match", "fullFlashMd5Match", "packageValidated", "savedPackageReimported", "originalState", "plan", "writeLock", "cleanup"];
var Diagnostics = class {
  #data;
  constructor({ build = "source-development", sourceCommit = "uncommitted", originType = "test", origin = "TEST", mode = "read-only", simulation = false } = {}) {
    this.#data = {
      schema: "draftor-diagnostics/1",
      schemaVersion: 1,
      build,
      sourceCommit,
      originType,
      origin,
      simulation,
      mode,
      stage: "IDLE",
      startedAt: (/* @__PURE__ */ new Date()).toISOString(),
      evidenceType: "PROGRAM_CONTROL_POINTS_NOT_INDEPENDENT_WIRE_CAPTURE",
      events: Object.fromEntries(EVENTS.map((k) => [k, 0])),
      commands: Object.fromEntries(Object.values(OPS).map((k) => [k, { attempted: 0, sent: 0, acceptedReplies: 0 }])),
      pendingCommands: 0,
      checks: Object.fromEntries(CHECKS.map((k) => [k, "NOT_RUN"])),
      firstError: null,
      lastError: null,
      closeResult: "NOT_RUN"
    };
  }
  event(name) {
    if (!EVENTS.includes(name)) throw Error("Unknown diagnostic event");
    this.#data.events[name]++;
  }
  stage(stage) {
    if (!/^[A-Z_]{2,64}$/.test(stage)) throw Error("Invalid stage");
    this.#data.stage = stage;
  }
  check(name, value) {
    if (!CHECKS.includes(name)) throw Error("Unknown diagnostic check");
    this.#data.checks[name] = structuredClone(value);
  }
  command(op, step) {
    const row = this.#data.commands[OPS[op]];
    if (!row || !["attempted", "sent", "acceptedReplies"].includes(step)) throw Error("Invalid counter");
    row[step]++;
  }
  pending(delta) {
    this.#data.pendingCommands += delta;
  }
  error(error) {
    const entry = { code: /^[A-Z_0-9]{1,48}$/.test(error?.code || "") ? error.code : "IO", stage: this.#data.stage };
    this.#data.firstError ??= entry;
    this.#data.lastError = entry;
  }
  closed(ok) {
    this.#data.closeResult = ok ? "CLOSED" : "FAILED_POSSIBLY_OPEN";
  }
  snapshot() {
    const d = this.#data;
    return structuredClone({ ...d, recordedAt: (/* @__PURE__ */ new Date()).toISOString(), nextStep: d.closeResult === "FAILED_POSSIBLY_OPEN" ? "SAVE_RECEIPT_CLOSE_TAB_CHECK_PORT_OWNER" : d.stage === "STOP" ? "PRESERVE_STATE_NO_AUTOMATIC_RETRY" : d.stage === "VERIFIED_BOOT_PENDING" ? "CLOSE_THEN_MANUAL_POWER_OFF_TF_AND_NORMAL_BOOT" : "FOLLOW_VISIBLE_READ_ONLY_OR_CONFIRMATION_STEP" });
  }
};
async function requestSerialPort(serial, diagnostics) {
  diagnostics.event("requestPortAttempts");
  try {
    return await serial.requestPort({ filters: [{ usbVendorId: 12346, usbProductId: 4097 }] });
  } catch (error) {
    diagnostics.event(error?.name === "NotFoundError" ? "requestPortCancelled" : "requestPortErrors");
    throw error;
  }
}

// src/public-ui.js
function startPublicUI({ build, sourceCommit, offlineToolSha256, requestDriver, offlineToolHref = "../offline/Draftor_Recovery_R10_Offline.html", offlineToolName = "Draftor_Recovery_R10_Offline.html" }) {
  const $ = (id) => document.getElementById(id);
  let lang = "zh", phase = "IDLE", error = "", release, session, diagnostics, lastReceipt, busy = false, activeTask, zip, packageName = "", infoKey, closed = false;
  const urls = /* @__PURE__ */ new Set(), t = (k) => k === "unsupported" ? platformMessage(lang) : installationCopy[lang][k] || dict[lang][k] || k;
  const modal = document.createElement("dialog");
  modal.id = "install-dialog";
  modal.setAttribute("aria-labelledby", "install-heading");
  const check = (id, key) => `<label class="confirm"><input type="checkbox" id="${id}"><span data-r10="${key}"></span></label>`;
  const field = (id, key, accept) => `<label class="field"><span data-r10="${key}"></span><input type="file" id="${id}" accept="${accept}"></label>`;
  modal.innerHTML = `<div class="dialog-inner"><div class="dialog-title"><h3 id="install-heading" data-r10="heading"></h3><button class="close" id="install-close" aria-label="Close">\xD7</button></div><div class="dialog-body">
 <p id="workflow-status" role="status" aria-live="polite"></p><p id="workflow-error" class="notice" role="alert" hidden></p>
 <section id="connect-section"><p data-r10="prepare"></p>${check("physical-board", "board")}${check("prepared-tf", "tf")}<button id="connect-read" class="install-primary" data-r10="connect" disabled></button></section>
 <section id="plan-section" hidden><h4 id="plan-title"></h4><p id="plan-summary"></p><p id="original-state" class="notice"></p><details id="plan-details"><summary data-r10="plan"></summary><pre id="plan-json"></pre></details>
 <section id="backup-workflow"><p data-r10="backupHelp"></p><button id="capture-backup" data-r10="capture"></button><button id="save-backup" data-r10="save" disabled></button><p data-r10="saveWarning"></p><p id="package-name"></p><details><summary data-r10="raw"></summary>${field("existing-raw", "raw", ".bin")}</details>${field("saved-backup", "reselect", ".zip")}<p id="backup-ready" class="notice" data-r10="saved" hidden></p></section>
 <section id="tool-section"><p data-r10="toolHelp"></p><button id="download-tool" data-r10="tool"></button><button id="tool-problem" data-r10="toolProblem"></button><p id="tool-ready" data-r10="toolReady" hidden></p>${check("tool-saved", "toolKeep")}</section>
 <section id="confirm-section">${check("plan-confirmed", "planConsent")}<div id="current-app-label">${field("current-app", "oldApp", ".bin")}</div><button id="execute-plan" class="install-primary" data-r10="execute" disabled></button></section></section>
 <progress id="write-progress" max="1" value="0" hidden></progress><p id="finished-note" class="notice" data-r10="finished" hidden></p><p><a href="recovery.html" data-r10="recoveryLink"></a> \xB7 <a href="docs/TF_GUIDE.html">TF</a> \xB7 <a href="docs/PUBLIC_STATUS.html" data-r10="scopeLink"></a></p><div class="workflow-actions"><button id="save-receipt" data-r10="receipt" disabled></button><button id="stop-close" data-r10="close"></button></div></div></div>`;
  document.body.append(modal);
  const downloads = downloadPanel({ anchor: $("workflow-error"), onChange: () => render(), onRetry: () => prepareFiles() });
  const platformNotice = document.createElement("p");
  platformNotice.id = "platform-notice";
  platformNotice.className = "notice";
  $("install-btn").after(platformNotice);
  const externalReceipt = document.createElement("button");
  externalReceipt.id = "closed-receipt";
  externalReceipt.dataset.r10 = "receipt";
  externalReceipt.hidden = true;
  document.querySelector(".footer nav").append(externalReceipt);
  function renderInfo() {
    const target = $("dialog-body");
    if (infoKey === "help" || infoKey === "advanced") {
      $("dialog-heading").textContent = t(infoKey === "help" ? "helpTitle" : "advancedTitle");
      target.replaceChildren();
      for (const text2 of [t("publicScope"), t("prepare"), t("backupHelp"), t("toolHelp")]) {
        const p = document.createElement("p");
        p.textContent = text2;
        target.append(p);
      }
      const links = document.createElement("p");
      links.innerHTML = '<a href="docs/PUBLIC_STATUS.html">\u8303\u56F4 / Scope</a> \xB7 <a href="docs/TF_GUIDE.html">TF</a> \xB7 <a href="recovery.html">\u5907\u4EFD\u4E0E\u6062\u590D / Recovery</a> \xB7 <a href="docs/SOURCE.html">Source & licenses</a>';
      const windowsLink = document.createElement("a");
      windowsLink.href = `docs/WINDOWS_FIRST_USER_GUIDE.${lang === "zh" ? "zh-CN" : "en"}.md`;
      windowsLink.textContent = lang === "zh" ? "Windows \u9996\u6B21\u4F7F\u7528" : "Windows first-use guide";
      links.append(" \xB7 ", windowsLink);
      target.append(links);
    } else {
      const c = dict[lang].dialogs[infoKey];
      $("dialog-heading").textContent = c.title;
      target.innerHTML = c.body;
    }
  }
  function render() {
    platformNotice.textContent = platformMessage(lang);
    document.documentElement.lang = lang === "zh" ? "zh-CN" : "en";
    document.title = lang === "zh" ? "Draftor \u8349\u7A3F\u673A \xB7 \u56FA\u4EF6\u5B89\u88C5" : "Draftor \xB7 Firmware Installer";
    for (const e of document.querySelectorAll("[data-i18n]")) e.textContent = t(e.dataset.i18n);
    for (const e of document.querySelectorAll("[data-r10]")) e.textContent = t(e.dataset.r10);
    $("board-name").textContent = lang === "zh" ? "\u5FAE\u96EA ESP32-S3-RLCD-4.2" : "Waveshare ESP32-S3-RLCD-4.2";
    const modified = (session?.receipt() || lastReceipt)?.mutationStarted === true;
    downloads.render(lang, ["loading", "DOWNLOAD_FAILED", "DOWNLOAD_CANCELLED"].includes(phase));
    $("workflow-status").textContent = ["loading", "DOWNLOAD_FAILED", "DOWNLOAD_CANCELLED"].includes(phase) ? downloads.status(lang) : phase === "STOP" ? t(modified ? "stoppedHelp" : "notModified") : installationCopy[lang].phases[phase] || t(phase);
    $("workflow-error").hidden = !error;
    $("workflow-error").textContent = error ? `${error} \xB7 ${serialErrorHelp(error, lang) || t(error === "PACKAGE_METADATA" ? "wrongOriginal" : error.startsWith("PACKAGE_") ? "badPackage" : modified ? "stoppedHelp" : "notModified")}` : "";
    $("connect-section").hidden = !!session || closed || !release || !browserIsQualified();
    $("connect-read").disabled = busy || !release || !$("physical-board").checked || !$("prepared-tf").checked;
    const plan = session?.plan;
    if (plan) {
      const blank = plan.initialState === "blank";
      $("plan-section").hidden = closed;
      $("plan-title").textContent = t(blank ? "blankFresh" : plan.mode === "fresh" ? "fresh" : "update");
      $("plan-summary").textContent = t(blank ? "blankImpact" : plan.mode === "fresh" ? "freshImpact" : "updateImpact");
      $("plan-json").textContent = JSON.stringify(plan, null, 2);
      $("backup-workflow").hidden = blank;
      $("tool-section").hidden = blank;
      $("original-state").textContent = t(blank ? "blank" : plan.initialState === "unrecognized" ? "unrecognized" : plan.mode === "fresh" ? "knownFactory" : "knownDraftor");
      document.querySelector("#plan-confirmed + span").textContent = t(plan.mode === "fresh" ? "eraseConsent" : "planConsent");
      $("current-app-label").hidden = plan.mode === "fresh";
    }
    const prepared = session?.state === "PLAN_READY" && !closed;
    for (const id of ["capture-backup", "existing-raw"]) $(id).disabled = busy || !prepared || !!zip || session?.backupVerified;
    $("save-backup").disabled = busy || !zip || closed;
    $("saved-backup").disabled = busy || !prepared;
    $("backup-ready").hidden = !session?.backupVerified;
    $("package-name").textContent = packageName;
    $("download-tool").disabled = busy || !prepared;
    $("tool-ready").hidden = !session?.offlineToolVerified;
    $("tool-saved").disabled = busy || !session?.offlineToolVerified;
    $("execute-plan").disabled = busy || !prepared || !session?.backupVerified || plan?.initialState !== "blank" && (!session?.offlineToolVerified || !$("tool-saved").checked) || !["plan-confirmed", "prepared-tf"].every((id) => $(id).checked);
    $("save-receipt").disabled = !diagnostics && !downloads.snapshot().attempt;
    $("stop-close").textContent = t(busy ? "stop" : "close");
    if ($("info-dialog").open) renderInfo();
  }
  function clearPrivate() {
    zip?.fill(0);
    zip = void 0;
    packageName = "";
    for (const id of ["existing-raw", "saved-backup", "current-app"]) $(id).value = "";
    for (const id of ["physical-board", "prepared-tf", "plan-confirmed", "tool-saved"]) $(id).checked = false;
    for (const u of urls) URL.revokeObjectURL(u);
    urls.clear();
  }
  async function releaseSession() {
    if (closed) return;
    try {
      await session?.close();
    } finally {
      closed = true;
      clearPrivate();
      diagnostics?.check("cleanup", { sessionPrivateReferencesCleared: true, domIdentityCleared: true, fileInputsCleared: true, objectUrlsRevoked: true, physicalMemoryErasureGuaranteed: false });
      lastReceipt = session?.receipt() || { diagnostics: diagnostics?.snapshot() };
      session = void 0;
      $("plan-section").hidden = true;
      $("plan-json").textContent = "";
      externalReceipt.hidden = !diagnostics && !downloads.snapshot().attempt;
    }
  }
  function download(bytes, name, type) {
    const u = URL.createObjectURL(new Blob([bytes], { type }));
    urls.add(u);
    const a = document.createElement("a");
    a.href = u;
    a.download = name;
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => {
      URL.revokeObjectURL(u);
      urls.delete(u);
    }, 3e4);
  }
  async function run(fn) {
    if (busy) return;
    busy = true;
    render();
    activeTask = (async () => {
      try {
        await fn();
      } catch (e) {
        error = publicErrorCode(e);
        diagnostics?.error({ code: error });
        session?.cancel(error);
        try {
          await releaseSession();
        } catch {
          error += " / CLOSE_FAILED";
        }
        phase = "STOP";
      } finally {
        busy = false;
        render();
      }
    })();
    return activeTask;
  }
  function onState(next, data = {}) {
    phase = next;
    if (data.progress !== void 0 || data.backupProgress !== void 0) {
      $("write-progress").hidden = false;
      $("write-progress").value = data.progress ?? data.backupProgress;
    }
    render();
  }
  async function prepareFiles() {
    if (busy || session || closed || !["loading", "DOWNLOAD_FAILED"].includes(phase)) return;
    busy = true;
    phase = "loading";
    error = "";
    render();
    activeTask = (async () => {
      try {
        release = await downloads.load();
        phase = "ready";
      } catch (e) {
        release = void 0;
        phase = e.detail?.category === "CANCELLED" ? "DOWNLOAD_CANCELLED" : "DOWNLOAD_FAILED";
      } finally {
        busy = false;
        render();
      }
    })();
    return activeTask;
  }
  $("install-btn").addEventListener("click", () => {
    if (modal.open || busy) return;
    closed = false;
    phase = browserIsQualified() ? "loading" : "unsupported";
    error = "";
    release = void 0;
    diagnostics = void 0;
    lastReceipt = void 0;
    downloads.reset();
    $("plan-section").hidden = true;
    $("finished-note").hidden = true;
    $("write-progress").hidden = true;
    clearPrivate();
    modal.showModal();
    render();
    if (browserIsQualified()) prepareFiles();
  });
  $("connect-read").addEventListener("click", () => {
    if ($("connect-read").disabled) return;
    diagnostics = new Diagnostics({ build, sourceCommit, originType: location.protocol === "https:" ? "HTTPS" : "LOOPBACK", origin: location.origin, mode: "installation" });
    run(async () => {
      requireSafe(browserIsQualified(), "PLATFORM");
      const driver = await requestDriver(diagnostics);
      session = new PublicInstallSession(driver, release, { build, sourceCommit, offlineToolSha256, platformQualified: browserIsQualified(), diagnostics, onState });
      await session.prepare({ boardConfirmed: $("physical-board").checked, tfRemoved: $("prepared-tf").checked });
    });
  });
  $("capture-backup").addEventListener("click", () => {
    if (!$("capture-backup").disabled) run(async () => {
      zip = await session.captureBackup();
      packageName = `Draftor-private-${session.plan.mode === "fresh" ? "device-original" : "draftor-original"}-${(/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-")}.zip`;
    });
  });
  $("existing-raw").addEventListener("change", () => {
    const f = $("existing-raw").files[0];
    if (!f) return;
    run(async () => {
      requireSafe(f.size === PINS.flashBytes, "PACKAGE_SIZE");
      let b;
      try {
        b = new Uint8Array(await f.arrayBuffer());
        zip = await session.importExistingRaw(b);
        packageName = `Draftor-private-${session.plan.mode === "fresh" ? "device-original" : "draftor-original"}-${(/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-")}.zip`;
      } finally {
        b?.fill(0);
        $("existing-raw").value = "";
      }
    });
  });
  $("save-backup").addEventListener("click", () => {
    if (zip && !busy) download(zip, packageName, "application/zip");
  });
  $("saved-backup").addEventListener("change", () => {
    session?.invalidateSavedPackage();
    $("plan-confirmed").checked = false;
    const f = $("saved-backup").files[0];
    render();
    if (!f) return;
    run(async () => {
      requireSafe(f.size > PINS.flashBytes && f.size <= MAX_PACKAGE_BYTES, "PACKAGE_SIZE");
      let b;
      try {
        b = new Uint8Array(await f.arrayBuffer());
        await session.verifySavedBackup(b);
      } finally {
        b?.fill(0);
        $("saved-backup").value = "";
      }
    });
  });
  $("download-tool").addEventListener("click", () => {
    if ($("download-tool").disabled) return;
    session.invalidateOfflineTool();
    $("tool-saved").checked = false;
    run(async () => {
      const r = await fetch(new URL(offlineToolHref, import.meta.url), { cache: "no-store" });
      requireSafe(r.ok, "RECOVERY_TOOL");
      const b = new Uint8Array(await r.arrayBuffer());
      requireSafe(b.length < 4e6, "RECOVERY_TOOL");
      await session.verifyOfflineTool(b);
      download(b, offlineToolName, "text/html");
    });
  });
  $("tool-problem").addEventListener("click", () => {
    session?.invalidateOfflineTool();
    $("tool-saved").checked = false;
    render();
  });
  $("current-app").addEventListener("change", () => {
    $("plan-confirmed").checked = false;
    if (busy) session?.cancel("PACKAGE_CHANGED");
    render();
  });
  $("execute-plan").addEventListener("click", () => {
    if ($("execute-plan").disabled) return;
    run(async () => {
      const f = $("current-app").files[0];
      let b;
      try {
        if (f) {
          requireSafe(f.size >= 24 && f.size <= PINS.slotCapacity, "CURRENT_APP");
          b = new Uint8Array(await f.arrayBuffer());
        }
        await session.execute({ backupConfirmed: session.backupVerified, tfRemoved: $("prepared-tf").checked, planConfirmed: $("plan-confirmed").checked, eraseConfirmed: session.plan.mode === "fresh" && $("plan-confirmed").checked, offlineToolSaved: $("tool-saved").checked, currentApp: b });
        await releaseSession();
        $("finished-note").hidden = false;
      } finally {
        b?.fill(0);
      }
    });
  });
  for (const id of ["physical-board", "prepared-tf", "plan-confirmed", "tool-saved"]) $(id).addEventListener("change", () => {
    if (busy) session?.cancel("CONSENT_CHANGED");
    render();
  });
  async function close() {
    downloads.cancel();
    if (busy) {
      session?.cancel();
      await activeTask;
    }
    try {
      await releaseSession();
      downloads.dispose();
      release = void 0;
      modal.close();
    } catch {
      error = "CLOSE_FAILED";
      phase = "STOP";
      render();
    }
  }
  $("install-close").addEventListener("click", close);
  $("stop-close").addEventListener("click", close);
  modal.addEventListener("cancel", (e) => {
    e.preventDefault();
    close();
  });
  function receipt() {
    const r = { schema: "draftor-install-download-receipt/1", build, sourceCommit, flashOutcome: "NO_FLASH_WRITE_ATTEMPT", ...session?.receipt() || lastReceipt, downloadPreparation: downloads.snapshot() };
    if (r) download(new TextEncoder().encode(JSON.stringify(r, null, 2) + "\n"), "Draftor-install-sanitized-receipt.json", "application/json");
  }
  $("save-receipt").addEventListener("click", receipt);
  externalReceipt.addEventListener("click", receipt);
  $("lang-toggle").addEventListener("click", () => {
    lang = lang === "zh" ? "en" : "zh";
    render();
  });
  $("board-change").addEventListener("click", () => {
    $("board-chooser").hidden = !$("board-chooser").hidden;
    $("board-change").setAttribute("aria-expanded", String(!$("board-chooser").hidden));
  });
  for (const e of document.querySelectorAll("[data-modal]")) e.addEventListener("click", () => {
    infoKey = e.dataset.modal;
    renderInfo();
    $("info-dialog").showModal();
  });
  $("dialog-close").addEventListener("click", () => $("info-dialog").close());
  addEventListener("beforeunload", (e) => {
    if (busy || session) {
      e.preventDefault();
      e.returnValue = "";
    }
  });
  addEventListener("pagehide", () => {
    downloads.dispose();
    release = void 0;
    session?.cancel();
    session?.close().catch(() => {
    });
    clearPrivate();
  });
  render();
}

// node_modules/esptool-js/lib/types/error.js
var ESPError = class extends Error {
  constructor(message) {
    super(message);
    this.name = new.target.name;
    Object.setPrototypeOf(this, new.target.prototype);
  }
};

// node_modules/esptool-js/lib/util.js
function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// node_modules/esptool-js/lib/webserial.js
var Transport = class {
  constructor(device, tracing = false, enableSlipReader = true) {
    this.device = device;
    this.tracing = tracing;
    this.slipReaderEnabled = false;
    this.baudrate = 0;
    this.traceLog = "";
    this.lastTraceTime = Date.now();
    this.buffer = new Uint8Array(0);
    this.onDeviceLostCallback = null;
    this.SLIP_END = 192;
    this.SLIP_ESC = 219;
    this.SLIP_ESC_END = 220;
    this.SLIP_ESC_ESC = 221;
    this._DTR_state = false;
    this.slipReaderEnabled = enableSlipReader;
  }
  /**
   * Set callback for when device is lost
   * @param {Function} callback Function to call when device is lost
   */
  setDeviceLostCallback(callback) {
    this.onDeviceLostCallback = callback;
  }
  /**
   * Update the device reference (used when re-selecting device after reset)
   * @param {typeof import("w3c-web-serial").SerialPort} newDevice New SerialPort device
   */
  updateDevice(newDevice) {
    this.device = newDevice;
    this.trace("Device reference updated");
  }
  /**
   * Request the serial device vendor ID and Product ID as string.
   * @returns {string} Return the device VendorID and ProductID from SerialPortInfo as formatted string.
   */
  getInfo() {
    const info = this.device.getInfo();
    return info.usbVendorId && info.usbProductId ? `WebSerial VendorID 0x${info.usbVendorId.toString(16)} ProductID 0x${info.usbProductId.toString(16)}` : "";
  }
  /**
   * Request the serial device vendor id from SerialPortInfo.
   * @returns {number | undefined} Return the vendor ID.
   */
  getVid() {
    return this.device.getInfo().usbVendorId;
  }
  /**
   * Request the serial device product id from SerialPortInfo.
   * @returns {number | undefined} Return the product ID.
   */
  getPid() {
    return this.device.getInfo().usbProductId;
  }
  /**
   * Format received or sent data for tracing output.
   * @param {string} message Message to format as trace line.
   */
  trace(message) {
    const delta = Date.now() - this.lastTraceTime;
    const prefix = `TRACE ${delta.toFixed(3)}`;
    const traceMessage = `${prefix} ${message}`;
    console.log(traceMessage);
    this.traceLog += traceMessage + "\n";
  }
  async returnTrace() {
    try {
      await navigator.clipboard.writeText(this.traceLog);
      console.log("Text copied to clipboard!");
    } catch (err) {
      console.error("Failed to copy text:", err);
    }
  }
  hexify(s) {
    return Array.from(s).map((byte) => byte.toString(16).padStart(2, "0")).join("").padEnd(16, " ");
  }
  hexConvert(uint8Array, autoSplit = true) {
    if (autoSplit && uint8Array.length > 16) {
      let result = "";
      let s = uint8Array;
      while (s.length > 0) {
        const line = s.slice(0, 16);
        const asciiLine = String.fromCharCode(...line).split("").map((c) => c === " " || c >= " " && c <= "~" && c !== "  " ? c : ".").join("");
        s = s.slice(16);
        result += `
    ${this.hexify(line.slice(0, 8))} ${this.hexify(line.slice(8))} | ${asciiLine}`;
      }
      return result;
    } else {
      return this.hexify(uint8Array);
    }
  }
  /**
   * Format data packet using the Serial Line Internet Protocol (SLIP).
   * @param {Uint8Array} data Binary unsigned 8 bit array data to format.
   * @returns {Uint8Array} Formatted unsigned 8 bit data array.
   */
  slipWriter(data) {
    const outData = [];
    outData.push(192);
    for (let i = 0; i < data.length; i++) {
      if (data[i] === 219) {
        outData.push(219, 221);
      } else if (data[i] === 192) {
        outData.push(219, 220);
      } else {
        outData.push(data[i]);
      }
    }
    outData.push(192);
    return new Uint8Array(outData);
  }
  /**
   * Write binary data to device using the WebSerial device writable stream.
   * @param {Uint8Array} data 8 bit unsigned data array to write to device.
   */
  async write(data) {
    const outData = this.slipWriter(data);
    if (this.device.writable) {
      const writer = this.device.writable.getWriter();
      if (this.tracing) {
        this.trace(`Write ${outData.length} bytes: ${this.hexConvert(outData)}`);
      }
      await writer.write(outData);
      writer.releaseLock();
    }
  }
  /**
   * Append a buffer array after another buffer array
   * @param {Uint8Array} arr1 - First array buffer.
   * @param {Uint8Array} arr2 - magic hex number to select ROM.
   * @returns {Uint8Array} Return a 8 bit unsigned array.
   */
  appendArray(arr1, arr2) {
    const combined = new Uint8Array(arr1.length + arr2.length);
    combined.set(arr1);
    combined.set(arr2, arr1.length);
    return combined;
  }
  /**
   * Read from serial device and append to buffer
   */
  async readLoop() {
    var _a;
    while (this.device.readable) {
      this.reader = (_a = this.device.readable) === null || _a === void 0 ? void 0 : _a.getReader();
      try {
        const { value, done } = await this.reader.read();
        if (done) {
          this.trace(`Serial port done`);
          break;
        }
        if (value && value.length) {
          const newValue = Uint8Array.from(value);
          this.buffer = this.appendArray(this.buffer, newValue);
        }
      } catch (error) {
        if (error instanceof Error) {
          const nonFatal = ["BufferOverrunError", "FramingError", "BreakError", "ParityError"];
          if (nonFatal.includes(error.name)) {
            this.trace(`Recoverable serial port error: ${error.message}`);
            continue;
          }
          this.trace(`Unrecoverable serial port error: ${error.message}`);
          break;
        }
        if (error instanceof DOMException) {
          if (this.onDeviceLostCallback) {
            this.onDeviceLostCallback();
          } else {
            this.trace(`Unrecoverable serial port error: ${error.message}`);
          }
          break;
        }
        this.trace(`Unrecoverable serial port error: ${error}`);
        break;
      } finally {
        this.reader.releaseLock();
      }
    }
    this.trace(`readLoop exited`);
  }
  flushInput() {
    this.buffer = new Uint8Array(0);
  }
  /**
   * Actively drain the input buffer: wait for in-flight bytes to arrive
   * (the ROM repeats an unsupported command response 8 times) and discard them.
   * Flushing alone only drops the bytes already buffered.
   * @param {number} quietMs Time without new bytes before considering the stream drained
   * @param {number} maxMs Upper bound on the total wait
   */
  async drainInput(quietMs = 100, maxMs = 400) {
    const deadline = Date.now() + maxMs;
    let lastLength = -1;
    while (Date.now() < deadline && this.buffer.length !== lastLength) {
      lastLength = this.buffer.length;
      await sleep(quietMs);
    }
    if (this.tracing) {
      this.trace(`Drained ${this.buffer.length} bytes from serial buffer`);
    }
    this.flushInput();
  }
  async flushOutput() {
    try {
      if (this.device.writable) {
        const writer = this.device.writable.getWriter();
        await writer.close();
        writer.releaseLock();
      }
    } catch (error) {
      this.trace(`Error while flushing output: ${error}`);
    }
  }
  // `inWaiting` returns the count of bytes in the buffer
  inWaiting() {
    return this.buffer.length;
  }
  // peek at the buffer without removing the data from the buffer
  peek() {
    return this.buffer;
  }
  /**
   * Detect if the data read from device is a Fatal or Guru meditation error.
   * @param {Uint8Array} input Data read from device
   */
  detectPanicHandler(input) {
    const guruMeditationRegex = /G?uru Meditation Error: (?:Core \d panic'ed \(([a-zA-Z ]*)\))?/;
    const fatalExceptionRegex = /F?atal exception \(\d+\): (?:([a-zA-Z ]*)?.*epc)?/;
    const inputString = new TextDecoder("utf-8").decode(input);
    const match = inputString.match(guruMeditationRegex) || inputString.match(fatalExceptionRegex);
    if (match) {
      const cause = match[1] || match[2];
      const msg = `Guru Meditation Error detected${cause ? ` (${cause})` : ""}`;
      throw new Error(msg);
    }
  }
  /**
   * Take a data array and return the first well formed packet after
   * replacing the escape sequence. Reads at least 8 bytes.
   * @param {number} timeout Timeout read data.
   * @returns {Uint8Array} Formatted packet using SLIP escape sequences.
   */
  async read(timeout) {
    let partialPacket = null;
    let isEscaping = false;
    let readBytes = null;
    while (true) {
      const timeStamp = Date.now();
      readBytes = new Uint8Array(0);
      while (Date.now() - timeStamp < timeout) {
        if (this.buffer.length > 0) {
          readBytes = this.buffer;
          this.buffer = new Uint8Array(0);
          break;
        } else {
          await sleep(1);
        }
      }
      if (!readBytes || readBytes.length === 0) {
        const msg = partialPacket === null ? "Serial data stream stopped: Possible serial noise or corruption." : "No serial data received.";
        if (this.tracing) {
          this.trace(msg);
        }
        throw new ESPError(msg);
      }
      if (this.tracing) {
        this.trace(`Read ${readBytes.length} bytes: ${this.hexConvert(readBytes)}`);
      }
      for (let i = 0; i < readBytes.length; i++) {
        const byte = readBytes[i];
        if (partialPacket === null) {
          if (byte === this.SLIP_END) {
            partialPacket = new Uint8Array(0);
          } else {
            if (this.tracing) {
              this.trace(`Read invalid data: ${this.hexConvert(readBytes)}`);
            }
            const remainingData = this.buffer;
            if (this.tracing) {
              this.trace(`Remaining data in serial buffer: ${this.hexConvert(remainingData)}`);
            }
            this.detectPanicHandler(new Uint8Array([...readBytes, ...remainingData || []]));
            throw new ESPError(`Invalid head of packet (0x${byte.toString(16)}): Possible serial noise or corruption.`);
          }
        } else if (isEscaping) {
          isEscaping = false;
          if (byte === this.SLIP_ESC_END) {
            partialPacket = this.appendArray(partialPacket, new Uint8Array([this.SLIP_END]));
          } else if (byte === this.SLIP_ESC_ESC) {
            partialPacket = this.appendArray(partialPacket, new Uint8Array([this.SLIP_ESC]));
          } else {
            if (this.tracing) {
              this.trace(`Read invalid data: ${this.hexConvert(readBytes)}`);
            }
            const remainingData = this.buffer;
            if (this.tracing) {
              this.trace(`Remaining data in serial buffer: ${this.hexConvert(remainingData)}`);
            }
            this.detectPanicHandler(new Uint8Array([...readBytes, ...remainingData || []]));
            throw new ESPError(`Invalid SLIP escape (0xdb, 0x${byte.toString(16)})`);
          }
        } else if (byte === this.SLIP_ESC) {
          isEscaping = true;
        } else if (byte === this.SLIP_END) {
          if (this.tracing) {
            this.trace(`Received full packet: ${this.hexConvert(partialPacket)}`);
          }
          if (i + 1 < readBytes.length) {
            const remainingBytes = readBytes.slice(i + 1);
            this.buffer = this.appendArray(remainingBytes, this.buffer);
          }
          return partialPacket;
        } else {
          partialPacket = this.appendArray(partialPacket, new Uint8Array([byte]));
        }
      }
    }
  }
  /**
   * Read from serial device without SLIP formatting. Calls onData for each chunk.
   * Stops when isClosed() returns true, the stream ends/errors, or disconnect() is called.
   *
   * The reader is stored on the instance so that disconnect() can cancel a pending
   * read: isClosed() is only evaluated after each chunk arrives, so on an idle
   * device a function-local reader would keep the stream locked forever and make
   * disconnect() block in waitForUnlock().
   * @param {Function} onData Callback for each chunk of data read
   * @param {Function} isClosed Function that returns true when reading should stop (e.g. when console is closed)
   */
  async rawRead(onData, isClosed) {
    let reader;
    try {
      if (!this.device.readable) {
        return;
      }
      reader = this.device.readable.getReader();
      this.reader = reader;
      while (!isClosed()) {
        const { value, done } = await reader.read();
        if (done || !value)
          break;
        if (this.tracing) {
          this.trace(`Read ${value.length} bytes: ${this.hexConvert(value)}`);
        }
        onData(value);
      }
    } catch (error) {
      this.trace(`Error reading from serial port: ${error}`);
      if (error instanceof Error && error.name === "NetworkError" && error.message.includes("device has been lost")) {
        this.trace("Device lost detected (NetworkError)");
        if (this.onDeviceLostCallback) {
          this.onDeviceLostCallback();
        }
      }
    } finally {
      reader === null || reader === void 0 ? void 0 : reader.releaseLock();
      if (this.reader === reader) {
        this.reader = void 0;
      }
    }
  }
  /**
   * Send the RequestToSend (RTS) signal to given state
   * # True for EN=LOW, chip in reset and False EN=HIGH, chip out of reset
   * @param {boolean} state Boolean state to set the signal
   */
  async setRTS(state) {
    await this.device.setSignals({ requestToSend: state });
    await this.setDTR(this._DTR_state);
  }
  /**
   * Send the dataTerminalReady (DTS) signal to given state
   * # True for IO0=LOW, chip in reset and False IO0=HIGH
   * @param {boolean} state Boolean state to set the signal
   */
  async setDTR(state) {
    this._DTR_state = state;
    await this.device.setSignals({ dataTerminalReady: state });
  }
  /**
   * Set the device signals to given states
   * # True for IO0=LOW, chip in reset and False IO0=HIGH
   * @param {boolean} dtr dataTerminalReady (DTS) signal boolean state
   * @param {boolean} rts requestToSend (RTS) signal boolean state
   * @param {boolean} breakSignal break signal boolean state
   */
  async setSignals(dtr, rts, breakSignal) {
    if (dtr) {
      this._DTR_state = dtr;
    }
    await this.device.setSignals({ dataTerminalReady: dtr, requestToSend: rts, break: breakSignal });
  }
  /**
   * Connect to serial device using the Webserial open method.
   * @param {number} baud Number baud rate for serial connection. Default is 115200.
   * @param {typeof import("w3c-web-serial").SerialOptions} serialOptions Serial Options for WebUSB SerialPort class.
   */
  async connect(baud = 115200, serialOptions = {}) {
    await this.device.open({
      baudRate: baud,
      dataBits: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.dataBits,
      stopBits: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.stopBits,
      bufferSize: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.bufferSize,
      parity: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.parity,
      flowControl: serialOptions === null || serialOptions === void 0 ? void 0 : serialOptions.flowControl
    });
    this.baudrate = baud;
  }
  /**
   * Change the host-side baud rate, preferring an in-place capability when the
   * serial-port implementation provides one.
   * @param {number} baud New baud rate.
   * @param {SerialOptions} serialOptions Options to preserve when a reopen is required.
   * @returns {boolean} True when the port had to be closed and reopened.
   */
  async changeBaudrate(baud, serialOptions = {}) {
    const configurableDevice = this.device;
    if (typeof configurableDevice.setBaudRate === "function") {
      if (this.tracing) {
        this.trace(`Changing host baud rate to ${baud} in place`);
      }
      await configurableDevice.setBaudRate(baud);
      this.baudrate = baud;
      return false;
    }
    if (this.tracing) {
      this.trace(`Reopening serial port at ${baud} baud`);
    }
    await this.disconnect();
    await sleep(50);
    await this.connect(baud, serialOptions);
    await sleep(50);
    this.readLoop();
    return true;
  }
  /**
   * Wait for a given timeout ms for serial device unlock.
   * @param {number} timeout Timeout time in milliseconds (ms) to sleep
   */
  async waitForUnlock(timeout) {
    while (this.device.readable && this.device.readable.locked || this.device.writable && this.device.writable.locked) {
      await sleep(timeout);
    }
  }
  /**
   * Disconnect from serial device by running SerialPort.close() after streams unlock.
   */
  async disconnect() {
    var _a, _b;
    if ((_a = this.device.readable) === null || _a === void 0 ? void 0 : _a.locked) {
      await ((_b = this.reader) === null || _b === void 0 ? void 0 : _b.cancel());
    }
    await this.waitForUnlock(400);
    await this.device.close();
    this.reader = void 0;
  }
};

// src/serial.js
var StrictSerialTransport = class {
  #port;
  #codec;
  #reader;
  #writer;
  #pendingRead;
  #opened = false;
  #dead = false;
  #bytes = [];
  #inFrame = false;
  #escape = false;
  #packets = [];
  #writeTimeout;
  #closeTimeout;
  #diagnostics;
  constructor(port, { writeTimeout = 1e4, closeTimeout = 2e3, diagnostics } = {}) {
    this.#diagnostics = diagnostics;
    this.#port = port;
    this.#codec = new Transport(port, false);
    this.#writeTimeout = writeTimeout;
    this.#closeTimeout = closeTimeout;
  }
  async #bounded(promise, ms) {
    let timer;
    try {
      return await Promise.race([promise, new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Stop("TIMEOUT")), ms);
      })]);
    } finally {
      clearTimeout(timer);
    }
  }
  async #chunk(ms, quietAllowed = false) {
    if (!this.#pendingRead) this.#pendingRead = this.#reader.read();
    try {
      const chunk = await this.#bounded(this.#pendingRead, ms);
      this.#pendingRead = void 0;
      return chunk;
    } catch (error) {
      if (quietAllowed && error instanceof Stop && error.code === "TIMEOUT") return void 0;
      throw error;
    }
  }
  getInfo() {
    return this.#port.getInfo();
  }
  async connect() {
    requireSafe(!this.#opened && !this.#dead, "STATE");
    try {
      this.#diagnostics?.event("openAttempts");
      await this.#port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: "none", flowControl: "none", bufferSize: 65536 });
      this.#opened = true;
      this.#diagnostics?.event("openSuccess");
      requireSafe(this.#port.readable && this.#port.writable, "IO");
      this.#reader = this.#port.readable.getReader();
      const deadline = Date.now() + 500;
      let discarded = 0;
      while (true) {
        requireSafe(Date.now() < deadline, "PROTOCOL");
        const chunk = await this.#chunk(Math.min(50, deadline - Date.now()), true);
        if (!chunk) break;
        requireSafe(!chunk.done && chunk.value, "DISCONNECTED");
        discarded += chunk.value.length;
        requireSafe(discarded <= 4096, "PROTOCOL");
      }
    } catch {
      if (!this.#opened) this.#diagnostics?.event("openErrors");
      this.#dead = true;
      throw new Stop("CONNECTION");
    }
  }
  async write(packet) {
    requireSafe(this.#opened && !this.#dead && this.#port.writable, "IO");
    const writer = this.#port.writable.getWriter();
    this.#writer = writer;
    try {
      await this.#bounded(writer.write(this.#codec.slipWriter(packet)), this.#writeTimeout);
      requireSafe(!this.#dead, "CANCELLED");
    } catch {
      this.#dead = true;
      writer.abort().catch(() => {
      });
      throw new Stop("IO");
    } finally {
      writer.releaseLock();
      this.#writer = void 0;
    }
  }
  #consume(chunk) {
    requireSafe(chunk instanceof Uint8Array && chunk.length <= 65536, "PROTOCOL");
    for (const value of chunk) {
      if (value === 192) {
        requireSafe(!this.#escape, "PROTOCOL");
        if (this.#inFrame && this.#bytes.length) {
          this.#packets.push(Uint8Array.from(this.#bytes));
          requireSafe(this.#packets.length <= 8, "PROTOCOL");
        }
        this.#inFrame = true;
        this.#bytes = [];
        continue;
      }
      requireSafe(this.#inFrame, "PROTOCOL");
      if (this.#escape) {
        requireSafe(value === 220 || value === 221, "PROTOCOL");
        this.#bytes.push(value === 220 ? 192 : 219);
        this.#escape = false;
      } else if (value === 219) this.#escape = true;
      else this.#bytes.push(value);
      requireSafe(this.#bytes.length <= 512, "PROTOCOL");
    }
  }
  async read(timeout = 1e4) {
    requireSafe(this.#opened && !this.#dead, "IO");
    const deadline = Date.now() + timeout;
    try {
      while (!this.#packets.length) {
        requireSafe(Date.now() < deadline, "TIMEOUT");
        const chunk = await this.#chunk(Math.max(1, deadline - Date.now()));
        requireSafe(!chunk.done && chunk.value, "DISCONNECTED");
        this.#consume(chunk.value);
      }
      return this.#packets.shift();
    } catch (error) {
      this.#dead = true;
      this.#reader?.cancel().catch(() => {
      });
      throw error instanceof Stop ? error : new Stop("IO");
    }
  }
  cancel() {
    this.#diagnostics?.event("cancelRequests");
    this.#dead = true;
    this.#reader?.cancel().catch(() => {
    });
    this.#writer?.abort().catch(() => {
    });
  }
  async disconnect() {
    this.#dead = true;
    try {
      if (this.#reader) {
        await this.#bounded(this.#reader.cancel().catch(() => {
        }), this.#closeTimeout);
        this.#reader.releaseLock();
        this.#reader = void 0;
      }
      if (this.#opened) {
        this.#diagnostics?.event("closeAttempts");
        try {
          await this.#bounded(this.#port.close(), this.#closeTimeout);
          this.#opened = false;
          this.#diagnostics?.event("closeSuccess");
        } catch (error) {
          this.#diagnostics?.event("closeErrors");
          throw error;
        }
      }
      this.#pendingRead = void 0;
      this.#bytes.fill(0);
      this.#bytes = [];
      for (const packet of this.#packets) packet.fill(0);
      this.#packets = [];
      this.#codec = void 0;
      this.#port = void 0;
      this.#diagnostics?.closed(true);
    } catch (error) {
      this.#diagnostics?.closed(false);
      this.#diagnostics?.error(error);
      throw error;
    }
  }
};

// src/rom.js
var OP = Object.freeze({ FLASH_BEGIN: 2, FLASH_DATA: 3, SYNC: 8, WRITE_REG: 9, READ_REG: 10, SPI_PARAMS: 11, SPI_ATTACH: 13, READ_FLASH_SLOW: 14, MD5: 19, SECURITY: 20 });
function parseSecurity(data) {
  requireSafe(data.length === 20, "SECURITY");
  requireSafe(u32(data, 12) === 9, "CHIP");
  const flags = u32(data), count = data[4];
  requireSafe((flags & ~2047) === 0 && count <= 7, "SECURITY");
  const encryption = (count.toString(2).split("1").length - 1) % 2 === 1;
  requireSafe((flags & 5) === 0 && !encryption, "SECURITY");
  return {
    chip: "ESP32-S3",
    securityKnown: true,
    secureBoot: false,
    flashEncryption: false,
    securityRule: "ESP32S3_ROM20_ESPt531_V1",
    securityFlags: flags,
    cryptCount: count,
    romChipId: u32(data, 12)
  };
}
var S3Rom = class {
  kind = "hardware";
  #wire;
  #fatal = false;
  #connected = false;
  #inFlight = false;
  #writesAllowed;
  #writeUsed = false;
  #firstAllowed;
  #firstStage = "IDLE";
  #firstNext = 0;
  #candidate;
  #candidateMode;
  #cleanInstall = false;
  #grantedBackupMd5;
  #mutationStarted = false;
  get flashMutationStarted() {
    return this.#mutationStarted;
  }
  #recoveryAllowed;
  #recoveryStage = "IDLE";
  #recoveryHandle;
  #recoveryImage;
  #recoveryManifest;
  #diagnostics;
  constructor(wire, { allowUpdate = false, allowFirstInstall = false, candidate = false, allowRecovery = false, diagnostics } = {}) {
    this.#diagnostics = diagnostics;
    requireSafe(!(allowUpdate && allowFirstInstall), "BUILD_SCOPE");
    requireSafe(!(candidate && (allowUpdate || allowFirstInstall)), "BUILD_SCOPE");
    requireSafe(!(allowRecovery && (candidate || allowUpdate || allowFirstInstall)), "BUILD_SCOPE");
    this.#recoveryAllowed = allowRecovery;
    this.#candidate = candidate;
    this.#wire = wire;
    this.#writesAllowed = allowUpdate;
    this.#firstAllowed = allowFirstInstall;
  }
  #alive() {
    requireSafe(!this.#fatal, "STATE");
  }
  async #reply(op, dataBytes, timeout) {
    const packet = await this.#wire.read(timeout);
    requireSafe(packet.length === 8 + dataBytes + 4 && packet[0] === 1 && packet[1] === op && (packet[2] | packet[3] << 8) === dataBytes + 4, "PROTOCOL");
    requireSafe(packet[8 + dataBytes] === 0, "ROM_STATUS");
    this.#diagnostics?.command(op, "acceptedReplies");
    return { value: u32(packet, 4), data: packet.slice(8, 8 + dataBytes) };
  }
  async #command(op, payload = new Uint8Array(), { dataBytes = 0, checksum = 0, timeout = 1e4, responses = 1 } = {}) {
    this.#alive();
    requireSafe(!this.#inFlight, "BUSY");
    this.#inFlight = true;
    this.#diagnostics?.pending(1);
    try {
      requireSafe(Object.values(OP).includes(op) && payload.length <= 1040, "PROTOCOL");
      const header = new Uint8Array(8);
      header[1] = op;
      new DataView(header.buffer).setUint16(2, payload.length, true);
      new DataView(header.buffer).setUint32(4, checksum, true);
      this.#diagnostics?.command(op, "attempted");
      if (op === OP.FLASH_BEGIN || op === OP.FLASH_DATA) {
        this.#mutationStarted = true;
      }
      await this.#wire.write(concat(header, payload));
      this.#diagnostics?.command(op, "sent");
      let reply;
      for (let i = 0; i < responses; i++) {
        reply = await this.#reply(op, dataBytes, timeout);
        if (op === OP.SYNC) requireSafe(reply.value !== 0, "STUB");
      }
      return reply;
    } catch (error) {
      this.#fatal = true;
      this.#diagnostics?.error(error);
      throw error instanceof Stop ? error : new Stop("IO");
    } finally {
      this.#inFlight = false;
      this.#diagnostics?.pending(-1);
    }
  }
  async #readReg(address) {
    return (await this.#command(OP.READ_REG, le32(address))).value;
  }
  async #writeReg(address, value) {
    requireSafe([1610645680, 1610645656, 1610645688, 1610645684, 1610620928, 1610620952, 1610620960, 1610620968, 1610621016].includes(address), "REGISTER");
    await this.#command(OP.WRITE_REG, le32(address, value, 4294967295, 0));
  }
  async #security() {
    return parseSecurity((await this.#command(OP.SECURITY, new Uint8Array(), { dataBytes: 20 })).data);
  }
  async #flashId() {
    const usr = await this.#readReg(1610620952), usr2 = await this.#readReg(1610620960);
    await this.#writeReg(1610620968, 23);
    await this.#writeReg(1610620952, 2415919104);
    await this.#writeReg(1610620960, 1879048351);
    await this.#writeReg(1610621016, 0);
    await this.#writeReg(1610620928, 1 << 18);
    let complete = false;
    for (let poll = 0; poll < 10; poll++) {
      if ((await this.#readReg(1610620928) & 1 << 18) === 0) {
        complete = true;
        break;
      }
    }
    requireSafe(complete, "FLASH");
    const id = await this.#readReg(1610621016);
    await this.#writeReg(1610620952, usr);
    await this.#writeReg(1610620960, usr2);
    requireSafe((id & 255) !== 0 && (id & 255) !== 255 && [24, 56].includes(id >>> 16 & 255), "FLASH");
    return PINS.flashBytes;
  }
  async connect() {
    this.#alive();
    requireSafe(!this.#connected, "STATE");
    try {
      const usb = this.#wire.getInfo();
      requireSafe(usb.usbVendorId === 12346 && usb.usbProductId === 4097, "USB_TRANSPORT");
      await this.#wire.connect();
      await this.#command(OP.SYNC, concat(Uint8Array.of(7, 7, 18, 32), new Uint8Array(32).fill(85)), { responses: 8 });
      await this.#security();
      await this.#writeReg(1610645680, 1356348065);
      await this.#writeReg(1610645656, 0);
      await this.#writeReg(1610645680, 0);
      await this.#writeReg(1610645688, 2401055018);
      await this.#writeReg(1610645684, (await this.#readReg(1610645684) | 2147483648) >>> 0);
      await this.#writeReg(1610645688, 0);
      await this.#command(OP.SPI_ATTACH, le32(0, 0));
      await this.#flashId();
      await this.#command(OP.SPI_PARAMS, le32(0, PINS.flashBytes, 65536, 4096, 256, 65535));
      this.#connected = true;
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
  }
  async identity() {
    this.#alive();
    requireSafe(this.#connected, "STATE");
    try {
      const security = await this.#security();
      const flashBytes = await this.#flashId();
      const lo = await this.#readReg(1610641476), hi = await this.#readReg(1610641480);
      const mac = [hi >>> 8 & 255, hi & 255, lo >>> 24, lo >>> 16 & 255, lo >>> 8 & 255, lo & 255].map((v) => v.toString(16).padStart(2, "0")).join(":");
      this.#diagnostics?.check("chip", security.chip);
      this.#diagnostics?.check("flashBytes", flashBytes);
      this.#diagnostics?.check("security", { known: security.securityKnown, secureBoot: security.secureBoot, flashEncryption: security.flashEncryption, rule: security.securityRule, flags: security.securityFlags, cryptCount: security.cryptCount, romChipId: security.romChipId });
      return { ...security, flashBytes, mac };
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
  }
  #range(offset, size) {
    this.#alive();
    requireSafe(this.#connected && Number.isSafeInteger(offset) && Number.isSafeInteger(size) && offset >= 0 && size > 0 && offset + size <= PINS.flashBytes, "RANGE");
  }
  async readFlash(offset, size) {
    this.#range(offset, size);
    const out = new Uint8Array(size);
    for (let at = 0; at < size; at += 64) {
      const n = Math.min(64, size - at);
      const { data } = await this.#command(OP.READ_FLASH_SLOW, le32(offset + at, n), { dataBytes: 64 });
      out.set(data.slice(0, n), at);
    }
    return out;
  }
  async flashMd5(offset, size) {
    this.#range(offset, size);
    const { data } = await this.#command(OP.MD5, le32(offset, size, 0, 0), { dataBytes: 32, timeout: 12e4 });
    const digest = new TextDecoder("ascii").decode(data).toLowerCase();
    try {
      requireSafe(/^[a-f0-9]{32}$/.test(digest), "PROTOCOL");
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
    return digest;
  }
  async grantExclusiveMode({ mode, initialState, boardConfirmed, cleanInstall = false, expectedMac, metadataFingerprint, backupMd5, protection }) {
    this.#alive();
    requireSafe(this.#candidate && this.#connected && this.#candidateMode === void 0 && !this.#writesAllowed && !this.#firstAllowed && !this.#writeUsed, "BUILD_SCOPE");
    this.#candidateMode = "RESERVING";
    try {
      requireSafe(["update", "fresh"].includes(mode), "BUILD_SCOPE");
      const identity = await this.identity();
      requireSafe(normalizeMac(identity.mac) === normalizeMac(expectedMac), "IDENTITY");
      if (protection !== void 0) {
        const p = savedProtection(protection, { mode, mac: normalizeMac(expectedMac) });
        sameOriginalIdentity(identity, p.device);
        backupMd5 = p.originalMd5;
      }
      const table = await this.readFlash(32768, 4096), ota = await this.readFlash(57344, 8192);
      requireSafe(await this.flashMd5(32768, 4096) === md5(table) && await this.flashMd5(57344, 8192) === md5(ota), "READBACK");
      requireSafe(`${await sha256(table)}:${await sha256(ota)}` === metadataFingerprint, "STALE_PLAN");
      requireSafe(/^[a-f0-9]{32}$/.test(backupMd5) && await this.flashMd5(0, PINS.flashBytes) === backupMd5, "BACKUP_STALE");
      if (mode === "update") {
        requireSafe(initialState === "compatible-draftor", "UNKNOWN");
        checkPartitions(table);
        requireSafe(await sha256(table.slice(0, 3072)) === PINS.files["partitions.bin"].sha256, "PARTITIONS");
        requireSafe(activeSlot(ota) === 0, "UNQUALIFIED_SLOT");
        requireSafe(await this.flashMd5(65536, PINS.files["app.bin"].bytes) === PINS.files["app.bin"].md5, "UNQUALIFIED_FIRMWARE");
        this.#writesAllowed = true;
      } else {
        if (cleanInstall) {
          requireSafe(boardConfirmed === true && cleanStateAllowed(initialState) && protection === void 0, "BOARD_CONFIRM");
          requireSafe(initialState !== "blank" || backupMd5 === PINS.blankFlashMd5, "BACKUP_STALE");
          this.#cleanInstall = true;
          this.#grantedBackupMd5 = backupMd5;
        } else {
          requireSafe(initialState === "waveshare-factory-20260120", "UNQUALIFIED_BLANK");
          await verifyInitialFlash(this, initialState);
        }
        this.#firstAllowed = true;
      }
      requireSafe(!(this.#writesAllowed && this.#firstAllowed), "BUILD_SCOPE");
      this.#candidateMode = mode;
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
  }
  async grantRecovery(handle, { boardConfirmed, tfRemoved, restoreConfirmed } = {}) {
    this.#alive();
    requireSafe(this.#recoveryAllowed && !this.#candidate && !this.#writesAllowed && !this.#firstAllowed && this.#connected && !this.#writeUsed && this.#recoveryStage === "IDLE", "DEVICE_WRITES_LOCKED");
    this.#recoveryStage = "RESERVING";
    try {
      requireSafe(boardConfirmed === true && tfRemoved === true && restoreConfirmed === true, "RESTORE_CONFIRM");
      const { manifest, image } = recoverySnapshot(handle);
      this.#recoveryImage = image;
      this.#recoveryManifest = manifest;
      this.#recoveryHandle = handle;
      requireSafe(image.length === PINS.flashBytes && await sha256(image) === manifest.image.sha256, "PACKAGE_HASH");
      sameOriginalIdentity(await this.identity(), manifest.device);
      this.#diagnostics?.check("originalIdentityMatch", true);
      this.#recoveryStage = "READY_FOR_CONFIRMATION";
    } catch (error) {
      this.#fatal = true;
      this.#clearRecovery();
      throw error;
    }
  }
  async restoreRecovery({ progress = () => {
  } } = {}) {
    this.#alive();
    requireSafe(this.#recoveryAllowed && this.#recoveryStage === "READY_FOR_CONFIRMATION" && !this.#writeUsed, "STATE");
    this.#writeUsed = true;
    this.#recoveryStage = "RESTORING";
    try {
      recoverySummary(this.#recoveryHandle);
      const data = this.#recoveryImage, manifest = this.#recoveryManifest;
      requireSafe(data?.length === PINS.flashBytes && await sha256(data) === manifest.image.sha256, "PACKAGE_HASH");
      sameOriginalIdentity(await this.identity(), manifest.device);
      this.#alive();
      this.#diagnostics?.stage("RESTORING");
      await this.#command(OP.FLASH_BEGIN, le32(PINS.flashBytes, PINS.flashBytes / 1024, 1024, 0, 0), { timeout: 48e4 });
      requireSafe(await this.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "ERASE_VERIFY");
      for (let sequence = 0; sequence < PINS.flashBytes / 1024; sequence++) {
        const block = data.subarray(sequence * 1024, (sequence + 1) * 1024);
        await this.#command(OP.FLASH_DATA, concat(le32(1024, sequence, 0, 0), block), { checksum: block.reduce((n, b) => n ^ b, 239) });
        if (sequence % 32 === 0 || sequence === 16383) progress((sequence + 1) / 16384);
      }
      this.#recoveryStage = "VERIFYING";
      this.#diagnostics?.stage("VERIFYING");
      const verification = await this.verifyRecoveryImage(this.#recoveryHandle);
      this.#recoveryStage = "VERIFIED_BOOT_PENDING";
      this.#diagnostics?.stage("VERIFIED_BOOT_PENDING");
      return verification;
    } catch (error) {
      this.#fatal = true;
      this.#recoveryStage = "STOP";
      this.#diagnostics?.error(error);
      throw error;
    } finally {
      this.#clearRecovery();
    }
  }
  async verifyRecoveryImage(handle) {
    this.#alive();
    const { manifest, image } = recoverySnapshot(handle);
    try {
      sameOriginalIdentity(await this.identity(), manifest.device);
      this.#diagnostics?.check("originalIdentityMatch", true);
      const full = await this.flashMd5(0, PINS.flashBytes) === manifest.image.md5;
      this.#diagnostics?.check("fullFlashMd5Match", full);
      requireSafe(full, "RESTORE_VERIFY");
      for (const [kind, range] of [["partition", manifest.original.partition], ["ota", manifest.original.ota]]) {
        if (range.applicable === false) {
          this.#diagnostics?.check("otaRead", "NOT_APPLICABLE_FACTORY");
          this.#diagnostics?.check("otaMd5Match", "NOT_APPLICABLE_FACTORY");
          continue;
        }
        const read = await this.readFlash(range.offset, range.bytes);
        this.#diagnostics?.check(`${kind}Read`, true);
        const match = md5(read) === await this.flashMd5(range.offset, range.bytes) && await sha256(read) === range.sha256;
        this.#diagnostics?.check(`${kind}Md5Match`, match);
        read.fill(0);
        requireSafe(match, "RESTORE_METADATA_VERIFY");
      }
      return { status: "VERIFIED_BOOT_PENDING", fullFlashMd5Match: true, metadataMatch: true, resetIssued: false };
    } finally {
      image.fill(0);
    }
  }
  #clearRecovery() {
    this.#recoveryImage?.fill(0);
    this.#recoveryImage = void 0;
    this.#recoveryManifest = void 0;
    this.#recoveryHandle = void 0;
  }
  async writeImage(offset, image, { mode, slotOffset, progress = () => {
  } } = {}) {
    if (mode === "fresh") return this.#writeFirstImage(offset, image, progress);
    const data = image.slice();
    this.#range(offset, data.length);
    requireSafe(this.#writesAllowed, "DEVICE_WRITES_LOCKED");
    requireSafe(!this.#candidate || this.#candidateMode === "update" && offset === 65536 && slotOffset === 65536, "UNQUALIFIED_SLOT");
    requireSafe(!this.#writeUsed && mode === "update" && [65536, 4259840].includes(offset) && slotOffset === offset, "RANGE");
    this.#writeUsed = true;
    requireSafe(data.length === PINS.files["app.bin"].bytes && await sha256(data) === PINS.files["app.bin"].sha256, "ASSET");
    requireSafe(Math.ceil(data.length / 4096) * 4096 <= PINS.slotCapacity, "CAPACITY");
    const blocks = Math.ceil(data.length / 1024);
    await this.#command(OP.FLASH_BEGIN, le32(data.length, blocks, 1024, offset, 0), { timeout: 12e4 });
    for (let sequence = 0; sequence < blocks; sequence++) {
      const block = new Uint8Array(1024).fill(255);
      block.set(data.subarray(sequence * 1024, Math.min(data.length, (sequence + 1) * 1024)));
      const checksum = block.reduce((sum, byte) => sum ^ byte, 239);
      await this.#command(OP.FLASH_DATA, concat(le32(1024, sequence, 0, 0), block), { checksum });
      if (sequence % 32 === 0 || sequence === blocks - 1) progress(Math.min(1, (sequence + 1) / blocks));
    }
  }
  async eraseAll({ source, cleanInstall = false, expectedOriginalMd5, expectedMac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed, protection } = {}) {
    this.#range(0, PINS.flashBytes);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, "FRESH_DEVICE_PENDING");
    requireSafe(!this.#candidate || this.#candidateMode === "fresh" && (this.#cleanInstall ? cleanInstall && cleanStateAllowed(source) : source === "waveshare-factory-20260120"), "UNKNOWN");
    requireSafe(tfRemoved === true && planConfirmed === true, "BACKUP");
    if (protection !== void 0) {
      requireSafe(this.#candidate, "BUILD_SCOPE");
      savedProtection(protection, { mode: "fresh", mac: normalizeMac(expectedMac) });
    } else requireSafe(backupConfirmed === true, "BACKUP");
    requireSafe(eraseConfirmed === true, "ERASE_CONFIRM");
    requireSafe(this.#firstStage === "IDLE" && !this.#writeUsed, "STATE");
    this.#firstStage = "RESERVED";
    this.#writeUsed = true;
    try {
      const info = await this.identity();
      requireSafe(normalizeMac(info.mac) === normalizeMac(expectedMac), "IDENTITY");
      if (this.#cleanInstall) {
        requireSafe(expectedOriginalMd5 === this.#grantedBackupMd5 && await this.flashMd5(0, PINS.flashBytes) === this.#grantedBackupMd5, "BACKUP_STALE");
      } else await verifyInitialFlash(this, source);
      await this.#command(OP.FLASH_BEGIN, le32(PINS.flashBytes, PINS.flashBytes / 1024, 1024, 0, 0), { timeout: 48e4 });
      requireSafe(await this.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "ERASE_VERIFY");
      this.#firstStage = "ERASED_VERIFIED";
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
  }
  async #writeFirstImage(offset, image, progress) {
    this.#range(offset, image.length);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, "FRESH_DEVICE_PENDING");
    requireSafe(this.#firstStage === "ERASED_VERIFIED" && this.#firstNext < FIRST_FILES.length, "STATE");
    this.#firstStage = "WRITING";
    try {
      const name = FIRST_FILES[this.#firstNext], pin = PINS.files[name], data = image.slice();
      requireSafe(offset === pin.offset && data.length === pin.bytes && await sha256(data) === pin.sha256, "ASSET");
      const blocks = Math.ceil(data.length / 1024), end = offset + Math.ceil(data.length / 4096) * 4096;
      requireSafe(end <= (this.#firstNext + 1 < FIRST_FILES.length ? PINS.files[FIRST_FILES[this.#firstNext + 1]].offset : 4259840), "RANGE");
      await this.#command(OP.FLASH_BEGIN, le32(data.length, blocks, 1024, offset, 0), { timeout: 12e4 });
      for (let sequence = 0; sequence < blocks; sequence++) {
        const block = new Uint8Array(1024).fill(255);
        block.set(data.subarray(sequence * 1024, (sequence + 1) * 1024));
        await this.#command(OP.FLASH_DATA, concat(le32(1024, sequence, 0, 0), block), { checksum: block.reduce((sum, b) => sum ^ b, 239) });
        if (sequence % 32 === 0 || sequence === blocks - 1) progress((this.#firstNext + (sequence + 1) / blocks) / FIRST_FILES.length);
      }
      requireSafe(await this.flashMd5(offset, data.length) === pin.md5, "VERIFY");
      this.#firstNext++;
      this.#firstStage = this.#firstNext === FIRST_FILES.length ? "COMPLETE" : "ERASED_VERIFIED";
    } catch (error) {
      this.#fatal = true;
      throw error;
    }
  }
  cancel() {
    this.#fatal = true;
    this.#clearRecovery();
    this.#wire.cancel();
  }
  async disconnect() {
    this.#fatal = true;
    this.#clearRecovery();
    await this.#wire.disconnect();
  }
};

// src/r11-main.js
var offlineName = false ? "Draftor_Recovery_R11_Offline.html" : "Draftor_Recovery_R13_Offline.html";
startPublicUI({
  build: "R13.1-public-651ff0719ac8",
  sourceCommit: "651ff0719ac8a89b5163fedf6b76b763516e13c6",
  offlineToolSha256: "92e69ad0679c7333cea8ccb45fd8e4119812d47044b9bc5c05432f134b4cce8b",
  offlineToolHref: "../offline/" + offlineName,
  offlineToolName: offlineName,
  requestDriver: async (diagnostics) => {
    requireSafe(browserIsQualified(), "PLATFORM");
    return new S3Rom(new StrictSerialTransport(await requestSerialPort(navigator.serial, diagnostics), { diagnostics }), { candidate: true, diagnostics });
  }
});
//# sourceMappingURL=app.js.map
