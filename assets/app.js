/*! Draftor Web R7 LOCKED. Combined browser program GPL-3.0. Corresponding source: ../sources/Draftor_Web_R7_source.zip ; licenses: ../docs/SOURCE.html */
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
async function loadRelease(base = new URL("../firmware/", import.meta.url), fetcher = fetch) {
  const inputs = {};
  for (const [name, pin] of Object.entries({ ...PINS.documents, ...PINS.files })) {
    const url = new URL(name, base);
    requireSafe(url.origin === base.origin, "ASSET");
    let response;
    try {
      response = await fetcher(url, { redirect: "error", credentials: "omit", cache: "no-store", signal: AbortSignal.timeout(6e4) });
      requireSafe(response.ok && !response.redirected && response.body, "DOWNLOAD");
      if (response.headers.get("content-length")) requireSafe(Number(response.headers.get("content-length")) === pin.bytes, "ASSET");
      const reader = response.body.getReader(), out = new Uint8Array(pin.bytes);
      let at = 0;
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          requireSafe(at + value.length <= out.length, "ASSET");
          out.set(value, at);
          at += value.length;
        }
        requireSafe(at === out.length, "DOWNLOAD");
      } catch (error) {
        await reader.cancel().catch(() => {
        });
        throw error;
      } finally {
        reader.releaseLock();
      }
      inputs[name] = out;
    } catch (error) {
      throw error instanceof Stop ? error : new Stop("DOWNLOAD");
    }
  }
  return verifyRelease(inputs);
}
function tableMatchesRelease(release, actual) {
  checkPartitions(actual);
  requireSafe(bytesEqual(actual.slice(0, 3072), releaseFile(release, "partitions.bin")), "PARTITIONS");
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
  constructor(driver, release, { hardwareWritesEnabled = false, hardwareFreshWritesEnabled = false, bindSelectedIdentity = false, onState = () => {
  } } = {}) {
    requireSafe(!(hardwareWritesEnabled && hardwareFreshWritesEnabled), "BUILD_SCOPE");
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
  receipt() {
    return structuredClone(this.#receipt);
  }
  #set(status) {
    this.#state = status;
    this.#receipt.status = status;
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
      if (this.#receipt.writeStarted || this.#receipt.eraseStarted) this.#receipt.flashOutcome = "POSSIBLY_PARTIAL";
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
    return info;
  }
  async #metadata() {
    this.#alive();
    const table = await this.#driver.readFlash(32768, 4096);
    this.#alive();
    const ota = await this.#driver.readFlash(57344, 8192);
    requireSafe(table.length === 4096 && ota.length === 8192, "READBACK");
    requireSafe(await this.#driver.flashMd5(32768, 4096) === md5(table), "READBACK");
    requireSafe(await this.#driver.flashMd5(57344, 8192) === md5(ota), "READBACK");
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
      if (table.every((v) => v === 255) && ota.every((v) => v === 255)) {
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
      const offset = slot === 0 ? 65536 : 4259840;
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
        interruption: "Active App may be partial; no automatic retry, alternate slot or reboot. Recover from verified private backups.",
        neverWrite: mode === "update" ? UPDATE_NEVER : ["TF card"],
        requires: mode === "update" ? ["physical Waveshare board", "matching private MAC", "verified recoverable Flash and TF backups", "TF removed", "explicit plan approval"] : ["proven blank or exact pinned factory fingerprint", "verified recoverable full Flash backup", "separate full internal Flash erase approval", "TF removed", "ordinary boot qualification pending"],
        ...mode === "fresh" ? {
          eraseRange: { offset: 0, bytes: PINS.flashBytes },
          eraseMethod: "ROM FLASH_BEGIN then complete all-FF MD5",
          factoryCommit: initialState === "blank" ? null : FACTORY.commit,
          clears: ["entire internal Flash", "old firmware", "settings", "NVS and pairing", "internal filesystem"],
          preservedOutsideFlash: ["TF card", "eFuses"],
          expectedFullMd5: FACTORY.draftorInitializedFullMd5
        } : {},
        hardwareWriteAllowed: mode === "update" ? this.#writesEnabled : this.#freshEnabled,
        deviceQualification: "DEVICE_PENDING",
        tfResources: "NOT_CHECKED"
      });
      this.#receipt.plan = this.#plan;
      this.#set("PLAN_READY");
      return this.#plan;
    });
  }
  async execute({ backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed = false, currentApp } = {}) {
    return this.#run(async () => {
      requireSafe(this.#state === "PLAN_READY", "STATE");
      requireSafe(backupConfirmed === true && tfRemoved === true && planConfirmed === true, "BACKUP");
      const fresh = this.#plan.mode === "fresh";
      requireSafe(fresh || eraseConfirmed === false, "ERASE_FORBIDDEN");
      requireSafe(!fresh || eraseConfirmed === true, "ERASE_CONFIRM");
      requireSafe(this.#driver.kind === "simulation" || (fresh ? this.#freshEnabled : this.#writesEnabled), fresh ? "FRESH_DEVICE_PENDING" : "DEVICE_WRITES_LOCKED");
      this.#set("RECHECKING");
      await this.#identity();
      const metadata = await this.#metadata();
      requireSafe(metadata.fingerprint === this.#fingerprint, "STALE_PLAN");
      if (fresh) await verifyInitialFlash(this.#driver, this.#plan.initialState);
      else {
        tableMatchesRelease(this.#release, metadata.table);
        requireSafe(`app${activeSlot(metadata.ota)}` === this.#plan.appSlot, "STALE_PLAN");
      }
      const offset = Number(this.#plan.appOffset);
      if (currentApp) {
        requireSafe(!fresh && currentApp instanceof Uint8Array && currentApp.length >= 24 && currentApp.length <= PINS.slotCapacity, "CURRENT_APP");
        const previous = currentApp.slice();
        requireSafe(await this.#driver.flashMd5(offset, previous.length) === md5(previous), "CURRENT_APP");
      }
      const protectedRanges = fresh ? [] : [[0, offset], [offset + PINS.slotCapacity, PINS.flashBytes - offset - PINS.slotCapacity]];
      const before = [];
      for (const [start, size] of protectedRanges) before.push(await this.#driver.flashMd5(start, size));
      this.#alive();
      if (fresh) {
        this.#receipt.eraseStarted = true;
        this.#set("ERASING");
        await this.#driver.eraseAll({ source: this.#plan.initialState, expectedMac: this.#mac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed });
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
        requireSafe(await this.#driver.flashMd5(0, PINS.flashBytes) === FACTORY.draftorInitializedFullMd5, "INITIALIZATION_VERIFY");
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
    this.#driver.cancel?.();
    this.#receipt.error = code;
    if (this.#receipt.writeStarted || this.#receipt.eraseStarted) this.#receipt.flashOutcome = "POSSIBLY_PARTIAL";
    this.#set("STOP");
  }
  async close() {
    this.#mac = void 0;
    await this.#driver.disconnect();
  }
};

// src/public-copy.js
var publicCopy = {
  zh: {
    previewHint: "v0.1.0-alpha.3 \xB7 \u6B63\u5F0F\u7F51\u7AD9 \xB7 \u771F\u5B9E\u5B89\u88C5\u6682\u672A\u516C\u5F00\u5F00\u653E",
    intro: "\u6B64\u7248\u672C\u63D0\u4F9B USB \u53EA\u8BFB\u68C0\u67E5\u4E0E\u5B89\u88C5\u8BA1\u5212\u3002\u771F\u5B9E\u5199\u5165\u5DF2\u9501\u5B9A\uFF1BTF \u8D44\u6E90\u53EF\u76F4\u63A5\u4E0B\u8F7D\u3002",
    locked: "\u5F53\u524D\u6B63\u5F0F\u7F51\u7AD9\u4EC5\u63D0\u4F9B\u53EA\u8BFB\u68C0\u67E5\u4E0E\u5B89\u88C5\u8BA1\u5212\uFF0C\u4E0D\u80FD\u6267\u884C\u771F\u5B9E\u5199\u5165\u3002\u8BF7\u4FDD\u5B58\u56DE\u6267\u540E\u5173\u95ED\u8FDE\u63A5\u3002",
    freshLocked: "\u5DF2\u8BC6\u522B\u9996\u88C5\u72B6\u6001\uFF0C\u4F46\u5F53\u524D\u6B63\u5F0F\u7F51\u7AD9\u9501\u5B9A\u5168\u7247\u64E6\u9664\u4E0E\u56FA\u4EF6\u5199\u5165\u3002",
    identityBound: "\u5F53\u524D USB \u8BBE\u5907\u8EAB\u4EFD\uFF08\u53EA\u4FDD\u5B58\u5728\u672C\u9875\u5185\u5B58\u4E2D\uFF1B\u5173\u95ED\u5373\u6E05\u9664\uFF09\uFF1A",
    publicScope: "\u5DF2\u9A8C\u6536\u8303\u56F4\uFF1A\u5FAE\u96EA ST7305\uFF0C\u51C6\u786E Alpha 3 app0 \u540C\u7248\u672C\u517C\u5BB9\u66F4\u65B0\uFF0C\u4EE5\u53CA FactoryOnly 20260120 \u51C6\u786E\u6307\u7EB9\u9996\u88C5\u3002\u516C\u5F00\u5B89\u88C5\u4ECD\u5F85\u5BA1\u6838\uFF1Bapp1\u3001\u5176\u4ED6\u5382\u5BB6\u6279\u6B21\u3001Windows/Edge \u5B9E\u673A\u3001\u65B0 TF \u5361\u548C\u5883\u5185\u4E0B\u8F7D\u672A\u9A8C\u6536\u3002"
  },
  en: {
    previewHint: "v0.1.0-alpha.3 \xB7 Official site \xB7 Hardware installation not yet public",
    intro: "USB read-only checks and installation plans are available. Hardware writes are locked. TF resources can be downloaded now.",
    locked: "This public site provides read-only checks and plans only. Hardware writing is locked. Save the receipt and close the connection.",
    freshLocked: "First-install state recognized. Full erasure and firmware writing are locked on this public site.",
    identityBound: "Selected USB identity (page memory only; cleared on close):",
    publicScope: "Qualified scope: Waveshare ST7305, exact Alpha 3 app0 same-version rewrite and the exact FactoryOnly 20260120 fingerprint. Public installation awaits review. app1, other factory batches, Windows/Edge hardware, new TF cards and mainland China downloads remain unverified."
  }
};

// src/ui.js
var copy = {
  zh: {
    previewHint: "v0.1.0-alpha.3 \xB7 \u8F6F\u4EF6\u9A8C\u6536\u7248 \xB7 \u5B9E\u673A\u5199\u5165\u9501\u5B9A",
    testHint: "v0.1.0-alpha.3 \xB7 \u4E13\u7528\u5B9E\u673A\u6D4B\u8BD5\u6784\u5EFA \xB7 \u4EC5\u517C\u5BB9\u66F4\u65B0",
    firstHint: "v0.1.0-alpha.3 \xB7 R6 \u9996\u88C5\u4E13\u9879\u6D4B\u8BD5 \xB7 \u5C06\u6E05\u9664\u5185\u90E8 Flash",
    factory: "\u5DF2\u8BC6\u522B\u5FAE\u96EA\u5382\u5BB6\u6F14\u793A\u7A0B\u5E8F\uFF1A\u521D\u59CB\u5316\u5B89\u88C5",
    intro: "\u901A\u8FC7 USB \u8FDE\u63A5\u8BBE\u5907\uFF0C\u5148\u68C0\u67E5\u517C\u5BB9\u6027\uFF0C\u518D\u786E\u8BA4\u5B89\u88C5\u3002",
    installHeading: "\u8FDE\u63A5\u8BBE\u5907\u5E76\u5B89\u88C5",
    preparing: "\u6B63\u5728\u6838\u9A8C\u672C\u5730\u53D1\u884C\u6587\u4EF6\u2026",
    preflight: "\u5148\u4FDD\u5B58\u6587\u7A3F\u5E76\u6B63\u5E38\u7ED3\u675F\u4F20\u8F93\uFF0C\u5B8C\u5168\u65AD\u7535\u540E\u53D6\u51FA TF\u3002\u65AD\u5F00\u5176\u4ED6\u5F00\u53D1\u677F\uFF0C\u6309\u4F4F BOOT \u63A5\u5165 USB\uFF0C\u7EA6\u4E24\u79D2\u540E\u677E\u5F00\u3002\u8BF7\u4FDD\u6301\u5F53\u524D\u7A97\u53E3\u6253\u5F00\u3002",
    boardConfirm: "\u6211\u5DF2\u6838\u5BF9\u5B9E\u7269\u662F\u5FAE\u96EA ESP32-S3-RLCD-4.2\uFF08ST7305\uFF09\uFF0C\u4E0D\u662F\u5176\u4ED6 ESP32-S3 \u677F\u3002",
    macLabel: "\u672C\u673A\u5907\u4EFD\u6216\u4E4B\u524D\u6838\u9A8C\u8BB0\u5F55\u4E2D\u7684\u8BBE\u5907 MAC",
    macHint: "\u53EA\u5728\u672C\u9875\u5185\u5B58\u4E2D\u6BD4\u5BF9\uFF1B\u4E0D\u4E0A\u4F20\u3001\u4E0D\u5199\u5165\u56DE\u6267\u3002\u8BF7\u52FF\u51ED\u82AF\u7247\u578B\u53F7\u731C\u6D4B\u677F\u578B\u3002",
    connectRead: "\u9009\u62E9\u4E32\u53E3\u5E76\u53EA\u8BFB\u68C0\u67E5",
    ready: "\u56DB\u4EFD\u56FA\u4EF6\u4E0E\u53D1\u884C\u6E05\u5355\u5DF2\u6838\u9A8C\uFF0C\u53EF\u4EE5\u8FDE\u63A5\u3002",
    unsupported: "\u5F53\u524D\u73AF\u5883\u4E0D\u652F\u6301\u5B89\u5168\u7684 Web Serial\u3002\u8BF7\u7528\u652F\u6301 Web Serial \u7684\u7535\u8111\u7AEF\u6D4F\u89C8\u5668\uFF0C\u4ECE localhost \u6216 HTTPS \u6253\u5F00\u672C\u9875\u3002\u5F53\u524D\u6CA1\u6709\u8FDE\u63A5\u8BBE\u5907\u3002",
    backup: "\u6211\u5DF2\u79C1\u4E0B\u4FDD\u5B58\u5E76\u6838\u9A8C\u53EF\u6062\u590D\u7684\u5B8C\u6574 Flash\uFF08\u542B\u8BBE\u7F6E\u3001\u914D\u5BF9\uFF09\u3001\u539F\u56FA\u4EF6\uFF0C\u4EE5\u53CA TF \u6587\u7A3F\u548C\u5B66\u4E60\u76EE\u5F55\u5907\u4EFD\u3002",
    tfRemoved: "TF \u5361\u5DF2\u5728\u5B8C\u5168\u65AD\u7535\u540E\u53D6\u51FA\uFF0C\u91CD\u8981\u6587\u7A3F\u5DF2\u4FDD\u5B58\u3002",
    approve: "\u6211\u5DF2\u9605\u8BFB\u4E0B\u65B9\u5B8C\u6574\u8BA1\u5212\uFF0C\u540C\u610F\u6267\u884C\u5176\u4E2D\u7684\u5199\u5165\u8303\u56F4\u3002",
    erase: "\u6211\u5355\u72EC\u786E\u8BA4\uFF1A\u5168\u65B0\u5B89\u88C5\u5C06\u64E6\u9664\u6574\u7247 16 MiB \u5185\u90E8 Flash\uFF0C\u6E05\u9664\u65E7\u56FA\u4EF6\u3001\u8BBE\u7F6E\u548C\u914D\u5BF9\u3002",
    currentApp: "\u53EF\u9009\uFF1A\u51C6\u786E\u7684\u65E7 App \u6587\u4EF6\uFF08\u4EC5\u672C\u5730\u6BD4\u5BF9\uFF0C\u4E0D\u4E0A\u4F20\uFF09",
    run: "\u6309\u5DF2\u6838\u9A8C\u8BA1\u5212\u5B89\u88C5",
    export: "\u4FDD\u5B58\u672C\u5730\u56DE\u6267",
    close: "\u5173\u95ED",
    stop: "\u505C\u6B62\u5E76\u4FDD\u7559\u56DE\u6267",
    update: "\u517C\u5BB9\u66F4\u65B0",
    fresh: "\u5DF2\u8BC1\u660E\u6574\u7247\u7A7A\u767D\uFF1A\u5168\u65B0\u5B89\u88C5",
    plan: "\u5B8C\u6574\u53EA\u8BFB\u8BA1\u5212",
    updateSummary: "\u53EA\u5199\u6D3B\u52A8 App\uFF1B\u64E6\u9664\u8303\u56F4\u4EC5\u4E3A\u8BE5 App \u8986\u76D6\u7684\u6247\u533A\u3002\u4FDD\u7559\u5176\u4ED6 App\u3001\u5206\u533A\u3001OTA\u3001NVS \u548C\u914D\u5BF9\uFF0C\u4E0D\u64CD\u4F5C TF\u3002\u66F4\u65B0\u4E2D\u65AD\u53EF\u80FD\u4F7F\u8BBE\u5907\u65E0\u6CD5\u542F\u52A8\uFF0C\u987B\u4FDD\u7559\u53EF\u6062\u590D\u5907\u4EFD\u3002",
    freshSummary: "\u5C06\u6E05\u9664\u6574\u7247 16 MiB \u5185\u90E8 Flash\uFF0C\u5305\u62EC\u65E7\u56FA\u4EF6\u3001\u8BBE\u7F6E\u3001\u914D\u5BF9\u53CA\u5185\u90E8\u6587\u4EF6\uFF0C\u518D\u5199\u5165\u51C6\u786E\u7684\u56DB\u4EFD Draftor \u56FA\u4EF6\u3002TF \u4E0D\u53D7\u64CD\u4F5C\u3002\u5FC5\u987B\u4FDD\u7559\u5DF2\u9A8C\u8BC1\u7684\u5B8C\u6574\u6062\u590D\u5907\u4EFD\u5E76\u5355\u72EC\u786E\u8BA4\u6E05\u9664\uFF1B\u4F20\u8F93\u4E2D\u65AD\u53EF\u80FD\u65E0\u6CD5\u5F00\u673A\uFF0C\u4E0D\u81EA\u52A8\u91CD\u8BD5\u6216\u6062\u590D\u3002",
    locked: "\u6B64\u4EA4\u4ED8\u6784\u5EFA\u53EA\u5141\u8BB8\u771F\u5B9E\u8BBE\u5907\u53EA\u8BFB\u8BA1\u5212\uFF0C\u771F\u5B9E\u5199\u5165\u5DF2\u9501\u5B9A\u3002\u540E\u7EED\u7ECF\u6388\u6743\u4F7F\u7528\u4E13\u7528\u6D4B\u8BD5\u6784\u5EFA\u9A8C\u8BC1\u517C\u5BB9\u66F4\u65B0\u3002",
    freshLocked: "\u6B64\u6784\u5EFA\u9501\u5B9A\u771F\u5B9E\u9996\u88C5\u3002\u4EC5\u72EC\u7ACB\u7684 R6 \u9996\u88C5\u6D4B\u8BD5\u6784\u5EFA\u53EF\u5728\u5B8C\u6574\u524D\u68C0\u3001\u6062\u590D\u51C6\u5907\u548C\u5355\u72EC\u6E05\u9664\u786E\u8BA4\u540E\u6267\u884C\u3002",
    sim: "\u8F6F\u4EF6\u6A21\u62DF \xB7 \u5408\u6210 Flash \xB7 \u4E0D\u8BBF\u95EE USB / TF",
    simHint: "\u4EE5\u4E0B\u7ED3\u679C\u4EC5\u6765\u81EA\u8F6F\u4EF6\u5939\u5177\uFF0C\u4E0D\u80FD\u8BC1\u660E\u771F\u5B9E\u8BBE\u5907\u5B89\u88C5\u6216\u542F\u52A8\u901A\u8FC7\u3002",
    tfGuide: "\u56FA\u4EF6\u6821\u9A8C\u5B8C\u6210\u4ECD\u9700\u666E\u901A\u542F\u52A8\u9A8C\u6536\u3002TF \u8BCD\u5E93\u3001\u5B57\u4F53\u53CA\u72EC\u7ACB LearningIndex \u5FC5\u987B\u53E6\u884C\u51C6\u5907\uFF1B\u672C\u9875\u4E0D\u68C0\u67E5 TF\uFF0C\u4E5F\u4E0D\u5BA3\u79F0\u6574\u673A\u5DF2\u53EF\u5199\u4F5C\u3002",
    finished: "\u56FA\u4EF6\u5185\u5BB9\u5DF2\u72EC\u7ACB\u6821\u9A8C\uFF0C\u7B49\u5F85\u666E\u901A\u542F\u52A8\u4E0E TF \u9A8C\u6536\u3002\u6CA1\u6709\u81EA\u52A8\u590D\u4F4D\u3002",
    phase_CONNECTING: "\u6B63\u5728\u8FDE\u63A5 ROM \u4E0B\u8F7D\u6A21\u5F0F\u2026",
    phase_CHECKING: "\u6B63\u5728\u53EA\u8BFB\u68C0\u67E5\u8BBE\u5907\u4E0E\u5B9E\u9645\u5206\u533A / OTA\u2026",
    phase_PLAN_READY: "\u53EA\u8BFB\u8BA1\u5212\u5DF2\u5C31\u7EEA\uFF0C\u5C1A\u672A\u5199\u5165\u3002",
    phase_RECHECKING: "\u5199\u5165\u524D\u91CD\u65B0\u6838\u5BF9\u540C\u4E00\u8BBE\u5907\u4E0E\u8BA1\u5212\u2026",
    phase_ERASING: "\u6B63\u5728\u6E05\u9664\u5185\u90E8 Flash \u5E76\u6838\u9A8C\u5168\u7247\u7A7A\u767D\u2026",
    phase_WRITING: "\u6B63\u5728\u5199\u5165\u8BA1\u5212\u6307\u5B9A\u7684\u533A\u57DF\u2026",
    phase_VERIFYING: "\u6B63\u5728\u72EC\u7ACB\u6821\u9A8C Flash \u5185\u5BB9\u4E0E\u4FDD\u62A4\u533A\u57DF\u2026",
    phase_VERIFIED_BOOT_PENDING: "VERIFIED_BOOT_PENDING \xB7 \u7B49\u5F85\u666E\u901A\u542F\u52A8\u9A8C\u6536",
    phase_STOP: "STOP \xB7 \u5DF2\u505C\u6B62\uFF0C\u8BF7\u4FDD\u7559\u72B6\u6001\uFF1B\u4E0D\u91CD\u8BD5\u3001\u4E0D\u6362\u69FD\u3001\u4E0D\u6539\u4E3A\u6E05\u7A7A\u91CD\u5237\u3002"
  },
  en: {
    previewHint: "v0.1.0-alpha.3 \xB7 Software qualification \xB7 Hardware writes locked",
    testHint: "v0.1.0-alpha.3 \xB7 Dedicated device test build \xB7 Compatible updates only",
    firstHint: "v0.1.0-alpha.3 \xB7 R6 first-install experiment \xB7 Internal Flash will be cleared",
    factory: "Recognized Waveshare factory demo: initialization",
    intro: "Connect by USB, check compatibility, then confirm installation.",
    installHeading: "Connect & Install",
    preparing: "Verifying local release files\u2026",
    preflight: "Save writing and finish transfers. Fully power off and remove the TF card. Disconnect other boards, hold BOOT while connecting USB, then release after about two seconds. Keep this window open.",
    boardConfirm: "I checked the physical board: Waveshare ESP32-S3-RLCD-4.2 (ST7305), not another ESP32-S3 board.",
    macLabel: "Device MAC from your private backup or previously verified record",
    macHint: "Compared only in this page\u2019s memory; never uploaded or included in receipts. A chip family does not identify a PCB.",
    connectRead: "Select port & inspect",
    ready: "All four firmware files and release manifests verified. Ready to connect.",
    unsupported: "Secure Web Serial is unavailable here. Open this page on localhost or HTTPS in a desktop browser with Web Serial support. No device was connected.",
    backup: "I privately retained and verified recoverable full Flash (settings and pairing), original firmware, TF document and learning-directory backups.",
    tfRemoved: "The TF card was removed after complete power-off and important writing was saved.",
    approve: "I reviewed the complete plan below and approve its write ranges.",
    erase: "I separately confirm: fresh installation erases all 16 MiB of internal Flash, removing old firmware, settings and pairing.",
    currentApp: "Optional: exact previous App file (local comparison only; never uploaded)",
    run: "Install the verified plan",
    export: "Save local receipt",
    close: "Close",
    stop: "Stop & keep receipt",
    update: "Compatible update",
    fresh: "Entire Flash proven blank: fresh installation",
    plan: "Complete read-only plan",
    updateSummary: "Write only the active App and erase only its covered sectors. Preserve the other App, partitions, OTA, NVS and pairing. Do not access TF. An interruption may prevent boot; retain recoverable backups.",
    freshSummary: "Clear all 16 MiB of internal Flash, including firmware, settings, pairing and internal files, then write the four exact Draftor images. TF is untouched. Retain verified full recovery backups and separately approve erasure. An interruption may prevent boot; no automatic retry or recovery.",
    locked: "This delivery build permits read-only hardware plans. Hardware writing is locked. Use the dedicated test build after authorization for compatible-update qualification.",
    freshLocked: "First installation is locked in this build. Only the separate R6 experiment permits it after complete checks, recovery preparation and separate erase approval.",
    sim: "Software simulation \xB7 Synthetic Flash \xB7 No USB / TF access",
    simHint: "These are software fixture results, not proof of physical installation or boot.",
    tfGuide: "Verified firmware still needs a normal boot check. TF dictionaries, body font and the separate LearningIndex must be prepared independently. This page does not inspect TF or certify writing readiness.",
    finished: "Flash content independently verified; normal boot and TF qualification remain pending. No automatic reset.",
    phase_CONNECTING: "Connecting to ROM download mode\u2026",
    phase_CHECKING: "Inspecting device, actual partitions and OTA read-only\u2026",
    phase_PLAN_READY: "Read-only plan ready. No write performed.",
    phase_RECHECKING: "Rechecking the same device and approved plan\u2026",
    phase_ERASING: "Clearing internal Flash and verifying it is entirely blank\u2026",
    phase_WRITING: "Writing the ranges specified in the plan\u2026",
    phase_VERIFYING: "Independently verifying Flash content and protected ranges\u2026",
    phase_VERIFIED_BOOT_PENDING: "VERIFIED_BOOT_PENDING \xB7 Normal boot qualification pending",
    phase_STOP: "STOP \xB7 Preserve the state. No retry, alternate slot, or erase-and-reinstall fallback."
  }
};
var errors = {
  BOARD: ["\u5FC5\u987B\u4EBA\u5DE5\u786E\u8BA4\u51C6\u786E\u7684\u5FAE\u96EA\u5B9E\u7269\u677F\u578B\u3002", "Confirm the exact physical Waveshare board."],
  IDENTITY: ["\u8BBE\u5907\u8EAB\u4EFD\u7F3A\u5931\u6216\u4E0E\u9884\u671F MAC \u4E0D\u7B26\u3002\u6838\u5BF9\u79C1\u4EBA\u8BB0\u5F55\u4E0E\u6240\u9009\u4E32\u53E3\u3002", "Device identity is missing or does not match the expected MAC. Check your private record and selected port."],
  ASSET: ["\u53D1\u884C\u6587\u4EF6\u7684\u5927\u5C0F\u6216 SHA-256 \u4E0D\u7B26\uFF1B\u7981\u6B62\u5199\u5165\u3002", "Release size or SHA-256 mismatch. Writing is forbidden."],
  IMAGE: ["ESP32-S3 \u955C\u50CF\u5934\u6216\u53D1\u884C\u6807\u8BC6\u4E0D\u7B26\u3002", "ESP32-S3 image header or release identity mismatch."],
  CAPACITY: ["App \u8D85\u51FA\u56FA\u5B9A\u69FD\u4F4D\u5BB9\u91CF\u3002", "App exceeds the fixed slot capacity."],
  DOWNLOAD: ["\u53D1\u884C\u6587\u4EF6\u8BFB\u53D6\u4E2D\u65AD\u3002\u6CA1\u6709\u81EA\u52A8\u91CD\u8BD5\uFF0C\u8BF7\u68C0\u67E5\u672C\u5730\u7AD9\u70B9\u6587\u4EF6\u3002", "Release file loading failed. No automatic retry. Check the local site files."],
  CHIP: ["\u4E0D\u662F\u53D7\u652F\u6301\u7684 ESP32-S3\u3002", "Not a supported ESP32-S3."],
  FLASH: ["\u672A\u786E\u8BA4\u662F 16 MiB Flash\u3002", "16 MiB Flash could not be confirmed."],
  SECURITY: ["\u5B89\u5168\u72B6\u6001\u672A\u77E5\u3001\u53D7\u9650\u6216\u5DF2\u542F\u7528 Secure Boot / Flash Encryption\u3002", "Security state is unknown, restricted, or Secure Boot / Flash Encryption is enabled."],
  PARTITIONS: ["\u5B9E\u9645\u5206\u533A\u8868\u4E0E\u56FA\u5B9A B \u5E03\u5C40\u5B57\u8282\u4E0D\u7B26\uFF1B\u7981\u6B62\u66F4\u65B0\u6216\u81EA\u52A8\u9996\u88C5\u3002", "Actual partition bytes differ from the fixed B layout. No update or automatic fresh installation."],
  OTA: ["OTA \u72B6\u6001 pending\u3001rollback\u3001\u635F\u574F\u6216\u4E0D\u552F\u4E00\uFF1B\u7981\u6B62\u731C\u6D4B\u6D3B\u52A8\u69FD\u3002", "OTA is pending, rollback, corrupt or ambiguous. Never guess the active slot."],
  UNKNOWN: ["\u5206\u533A\u7A7A\u767D\u4E0D\u8DB3\u4EE5\u8BC1\u660E\u6574\u7247\u4E3A\u7A7A\uFF1B\u8BE5\u8BBE\u5907\u72B6\u6001\u4E0D\u652F\u6301\u81EA\u52A8\u9996\u88C5\u3002", "Blank metadata does not prove blank Flash. This state cannot enter fresh installation."],
  READBACK: ["\u53EA\u8BFB\u6570\u636E\u4E0E ROM \u72EC\u7ACB\u6458\u8981\u4E0D\u4E00\u81F4\u3002", "Read data does not match the independent ROM digest."],
  VERIFY: ["\u5199\u540E\u5185\u5BB9\u6821\u9A8C\u4E0D\u4E00\u81F4\uFF0C\u8BBE\u5907\u53EF\u80FD\u53EA\u6709\u90E8\u5206 App\u3002\u8BF7\u4FDD\u7559\u73B0\u573A\u3002", "Post-write verification failed. The App may be partial. Preserve the device state."],
  PROTECTED_CHANGED: ["\u4FDD\u62A4\u533A\u57DF\u53D1\u751F\u53D8\u5316\uFF1B\u4E0D\u80FD\u5BA3\u79F0\u5B89\u88C5\u6210\u529F\u3002", "A protected region changed. Installation cannot be reported as successful."],
  STALE_PLAN: ["\u8BBE\u5907\u72B6\u6001\u5DF2\u53D8\u5316\uFF0C\u539F\u8BA1\u5212\u5931\u6548\u3002", "Device state changed; the approved plan is stale."],
  BACKUP: ["\u5FC5\u987B\u786E\u8BA4\u53EF\u6062\u590D\u5907\u4EFD\u3001TF \u5DF2\u53D6\u51FA\u4EE5\u53CA\u5B8C\u6574\u5199\u5165\u8BA1\u5212\u3002", "Confirm recoverable backups, TF removal and the complete write plan."],
  CURRENT_APP: ["\u51C6\u786E\u65E7 App \u6BD4\u5BF9\u5931\u8D25\uFF0C\u672A\u5F00\u59CB\u5199\u5165\u3002", "Exact previous App comparison failed. No write started."],
  USB_TRANSPORT: ["R1 \u53EA\u8986\u76D6 Espressif USB Serial/JTAG\uFF08303a:1001\uFF09\u3002\u5176\u4ED6\u4E32\u53E3\u8DEF\u5F84\u5C1A\u672A\u9A8C\u6536\u3002", "R1 covers only Espressif USB Serial/JTAG (303a:1001). Other serial paths are unqualified."],
  STUB: ["\u8BBE\u5907\u4F3C\u4E4E\u5DF2\u8FD0\u884C stub\uFF1B\u8BF7\u624B\u52A8\u91CD\u65B0\u8FDB\u5165 ROM\uFF0C\u4FDD\u7559\u539F\u73B0\u573A\u3002", "A stub appears to be running. Preserve the state and manually re-enter ROM mode."],
  CONNECTION: ["\u672A\u80FD\u8FDE\u63A5 ROM\u3002\u6838\u5BF9\u4E32\u53E3\u3001BOOT \u64CD\u4F5C\u53CA\u5360\u7528\u8BE5\u4E32\u53E3\u7684\u5E94\u7528\u3002", "Could not connect to ROM. Check the port, BOOT procedure and other serial applications."],
  DISCONNECTED: ["\u8FDE\u63A5\u4E2D\u65AD\uFF1B\u505C\u6B62\u540E\u7EED\u547D\u4EE4\uFF0C\u4E0D\u81EA\u52A8\u91CD\u8FDE\u3002", "Disconnected. Further commands stopped; no automatic reconnect."],
  IO: ["\u4E32\u53E3\u8BFB\u5199\u5931\u8D25\uFF1B\u8BBE\u5907\u72B6\u6001\u53EF\u80FD\u4E0D\u5B8C\u6574\uFF0C\u8BF7\u4FDD\u7559\u56DE\u6267\u3002", "Serial I/O failed. Device state may be incomplete; keep the receipt."],
  TIMEOUT: ["\u8BBE\u5907\u5E94\u7B54\u8D85\u65F6\uFF0C\u5DF2\u505C\u6B62\u3002", "Device response timed out. Stopped."],
  PROTOCOL: ["ROM \u5E94\u7B54\u7ED3\u6784\u6216\u957F\u5EA6\u4E0D\u7B26\uFF0C\u5DF2\u505C\u6B62\u3002", "ROM response structure or length is invalid. Stopped."],
  ROM_STATUS: ["ROM \u8FD4\u56DE\u9519\u8BEF\u72B6\u6001\uFF0C\u5DF2\u505C\u6B62\u3002", "ROM reported an error. Stopped."],
  CANCELLED: ["\u7528\u6237\u5DF2\u505C\u6B62\uFF1B\u82E5\u5199\u5165\u5DF2\u7ECF\u5F00\u59CB\uFF0CApp \u53EF\u80FD\u4E0D\u5B8C\u6574\u3002", "Stopped by the user. If writing had begun, the App may be incomplete."],
  ERASE_CONFIRM: ["\u5168\u7247\u64E6\u9664\u5FC5\u987B\u5355\u72EC\u786E\u8BA4\u3002", "Full Flash erasure requires separate confirmation."],
  ERASE_FORBIDDEN: ["\u517C\u5BB9\u66F4\u65B0\u4E0D\u63A5\u53D7\u5168\u7247\u64E6\u9664\u6743\u9650\u3002", "Compatible updates never accept full erase permission."],
  FRESH_DEVICE_PENDING: ["\u771F\u5B9E\u9996\u88C5\u5DF2\u9501\u5B9A\uFF0C\u7B49\u5F85\u72EC\u7ACB\u7A7A\u677F\u9A8C\u6536\u3002", "Real fresh installation is locked pending separate blank-board qualification."],
  DEVICE_WRITES_LOCKED: ["\u6B64\u6784\u5EFA\u7684\u771F\u5B9E\u5199\u5165\u5DF2\u9501\u5B9A\u3002", "Hardware writing is locked in this build."],
  FACTORY_FINGERPRINT: ["\u5382\u5BB6\u955C\u50CF\u6307\u7EB9\u4E0D\u7B26\uFF1B\u672A\u77E5\u65E7\u72B6\u6001\u7981\u6B62\u6E05\u9664\u3002", "Factory fingerprint mismatch. Unknown old state must not be erased."],
  ERASE_VERIFY: ["\u5168\u7247\u64E6\u540E\u6821\u9A8C\u5931\u8D25\uFF1B\u505C\u6B62\uFF0C\u4E0D\u5199\u4EFB\u4F55\u56FA\u4EF6\u3002", "Full erase verification failed. Stopped before firmware writes."],
  INITIALIZATION_VERIFY: ["\u521D\u59CB\u5316\u5B8C\u6574\u955C\u50CF\u6821\u9A8C\u5931\u8D25\uFF1B\u505C\u6B62\u5E76\u4FDD\u7559\u73B0\u573A\u3002", "Complete initialized Flash verification failed. Preserve the state."],
  BUILD_SCOPE: ["\u6784\u5EFA\u6743\u9650\u51B2\u7A81\uFF1B\u505C\u6B62\u3002", "Conflicting build permissions. Stopped."]
};
function startUI({ requestDriver, simulation = false, scenario, expectedMac = "", hardwareWritesEnabled = false, hardwareFreshWritesEnabled = false, publicRelease = false, bindSelectedIdentity = false, sessionFactory = (driver, release, options) => new InstallSession(driver, release, options) }) {
  Object.assign(dict.zh, copy.zh);
  Object.assign(dict.en, copy.en);
  if (publicRelease) {
    Object.assign(dict.zh, publicCopy.zh);
    Object.assign(dict.en, publicCopy.en);
  }
  dict.zh.dialogs.help = { title: "\u5B89\u88C5\u5E2E\u52A9", body: '<p>\u4F7F\u7528\u7535\u8111\u7AEF Web Serial \u6D4F\u89C8\u5668\u3001USB \u6570\u636E\u7EBF\u548C\u51C6\u786E\u7684\u5FAE\u96EA ST7305 \u8BBE\u5907\u3002R1 \u53EA\u8986\u76D6 USB Serial/JTAG\uFF1B\u6309\u4F4F BOOT \u63A5\u5165 USB\uFF0C\u4FDD\u6301 ROM \u4E0B\u8F7D\u6A21\u5F0F\u3002</p><p>\u5199\u5165\u524D\u987B\u5728\u672C\u673A\u6838\u9A8C\u53EF\u6062\u590D\u7684\u5B8C\u6574 Flash\uFF0F\u914D\u5BF9\u5907\u4EFD\uFF0C\u5E76\u5355\u72EC\u5907\u4EFD TF \u6587\u7A3F\u4E0E\u5B66\u4E60\u76EE\u5F55\u3002\u7F51\u9875\u4E0D\u521B\u5EFA\u3001\u6536\u96C6\u6216\u4E0A\u4F20\u79C1\u4EBA\u5907\u4EFD\u3002</p><p>\u517C\u5BB9\u66F4\u65B0\u53EA\u5199\u5DF2\u786E\u8BA4\u7684\u6D3B\u52A8 App\u3002\u672A\u77E5\u5206\u533A\u3001OTA pending\uFF0Frollback\u3001\u5B89\u5168\u72B6\u6001\u4E0D\u660E\u5747\u505C\u6B62\u3002\u771F\u5B9E\u9996\u88C5\u5C1A\u672A\u9A8C\u6536\uFF0C\u672C\u8F6E\u9501\u5B9A\u3002</p><p>TF \u987B\u53E6\u5907\uFF1AAlpha 2.1 \u8D44\u6E90\u5305\u4E2D\u7684 <code>HANIME/IME/B_SHORT_TEXT_20261018/</code> \u8BCD\u5E93\u53CA <code>HANIME/IME/R1_API080/matched28.hbf1</code> \u5B57\u4F53\uFF1BAlpha 3 \u72EC\u7ACB\u5305\u4E2D\u7684 <code>WAVESHARE_R7_L1_AREV1_RES_20261112/relations.bgr</code> \u548C <code>relations.hnv</code>\u3002\u4E0D\u8981\u683C\u5F0F\u5316 TF\uFF0C\u4E0D\u8986\u76D6\u4E0D\u5339\u914D\u7684\u539F\u6587\u4EF6\u3002</p><p><a href="docs/TF_GUIDE.html" target="_blank" rel="noopener">\u51C6\u786E\u4E0B\u8F7D\u3001\u8DEF\u5F84\u53CA SHA-256 \u6E05\u5355</a> \xB7 <a href="docs/INSTALL.zh-CN.md" target="_blank" rel="noopener">\u53D1\u884C\u5B89\u88C5\u6307\u5357</a></p><div class="notice">\u56FA\u4EF6\u6821\u9A8C\u6210\u529F \u2260 \u6574\u673A\u5DF2\u5177\u5907\u5168\u90E8\u5199\u4F5C\u8D44\u6E90\u3002\u666E\u901A\u542F\u52A8\u3001\u952E\u76D8\u914D\u5BF9\u3001\u4FDD\u5B58\u72B6\u6001\u4ECD\u9700\u5B9E\u673A\u9A8C\u6536\u3002</div>' };
  dict.en.dialogs.help = { title: "Installation help", body: '<p>Use desktop Web Serial, a USB data cable and the exact Waveshare ST7305 board. R1 covers USB Serial/JTAG only. Hold BOOT while connecting USB and remain in ROM download mode.</p><p>Verify recoverable full Flash/settings/pairing backups and separate TF document and learning backups privately before a write. This page never creates, collects or uploads private backups.</p><p>Compatible updates write only the confirmed active App. Unknown partitions, pending/rollback OTA or unknown security stop installation. Physical fresh installation is locked and unqualified.</p><p>Prepare TF separately: dictionaries under <code>HANIME/IME/B_SHORT_TEXT_20261018/</code> and <code>HANIME/IME/R1_API080/matched28.hbf1</code> from Alpha 2.1 Resources; both <code>WAVESHARE_R7_L1_AREV1_RES_20261112/relations.bgr</code> and <code>relations.hnv</code> from the separate Alpha 3 LearningIndex. Never format TF or overwrite mismatching originals.</p><p><a href="docs/TF_GUIDE.html" target="_blank" rel="noopener">Exact downloads, paths and SHA-256 list</a> \xB7 <a href="docs/INSTALL.md" target="_blank" rel="noopener">Release installation guide</a></p><div class="notice">Verified firmware does not certify writing readiness. Normal boot, keyboard pairing and saved writing still need physical qualification.</div>' };
  for (const lang of ["zh", "en"]) {
    dict[lang].dialogs.advanced = { title: lang === "zh" ? "\u9AD8\u7EA7\u9009\u9879" : "Advanced", body: `<p>${lang === "zh" ? "\u9501\u5B9A\u53D1\u884C" : "Pinned release"}: <strong>${PINS.version}</strong><br>WAVESHARE_RLCD42 \xB7 ST7305 \xB7 16 MiB</p><p>App: 4,021,152 bytes<br><code>${PINS.files["app.bin"].sha256}</code></p><p>${lang === "zh" ? "\u56FA\u5B9A B \u5206\u533A\uFF1B\u53EA\u63A5\u53D7\u552F\u4E00\u6D3B\u52A8\u69FD\u3002\u65E0\u5F3A\u5237\u3001\u6539\u69FD\u6216\u5168\u7247\u64E6\u9664\u5F00\u5173\u3002\u8BBE\u5907\u4FE1\u606F\u4EC5\u5728\u672C\u9875\u5185\u5B58\u4E2D\u6BD4\u5BF9\uFF1B\u56DE\u6267\u4E0D\u542B MAC \u6216\u79C1\u6709 Flash \u5185\u5BB9\u3002" : "Fixed B layout; one unambiguous active slot only. No force, slot override or full erase switch. Device identity stays in page memory; receipts exclude MAC and private Flash content."}</p><p><a href="simulation.html?scenario=app0">${lang === "zh" ? "\u8FD0\u884C\u8F6F\u4EF6\u6A21\u62DF\uFF08\u4E0D\u8FDE\u63A5\u8BBE\u5907\uFF09" : "Run software simulation (no device)"}</a> \xB7 <a href="simulation.html?scenario=app1">app1</a> \xB7 <a href="simulation.html?scenario=fresh">fresh</a></p><p><a href="docs/CAPABILITY_AUDIT.md">${lang === "zh" ? "\u5E95\u5C42\u80FD\u529B\u5BA1\u8BA1" : "Capability audit"}</a> \xB7 <a href="docs/SOURCE.html">${lang === "zh" ? "\u6E90\u7801\u4E0E\u8BB8\u53EF" : "Source and licenses"}</a></p>` };
  }
  let language = "zh", modalKey, release, session, busy = false, chooserOpen = false, lastPhase = "", errorCode = "", dialogEpoch = 0;
  if (publicRelease) for (const lang of ["zh", "en"]) {
    const c = dict[lang];
    c.dialogs.help.body = `<p>${c.intro}</p><p>${c.preflight}</p><p>${c.publicScope}</p><p>${c.tfGuide}</p><p><a href="docs/TF_GUIDE.html">TF \u8D44\u6E90 / TF resources</a> \xB7 <a href="docs/PUBLIC_STATUS.html">\u516C\u5F00\u8303\u56F4 / Public scope</a></p>`;
    c.dialogs.advanced.body = `<p>${c.publicScope}</p><p>App SHA-256: <code>${PINS.files["app.bin"].sha256}</code></p><p><a href="BUILD.json">Build</a> \xB7 <a href="SITE_SHA256SUMS">SHA-256</a> \xB7 <a href="docs/SOURCE.html">\u6E90\u7801\u4E0E\u8BB8\u53EF / Source and licenses</a></p>`;
  }
  const $ = (id) => document.getElementById(id), t = (key) => dict[language][key] || key;
  const dialog = $("info-dialog");
  const workflow = document.createElement("dialog");
  workflow.id = "install-dialog";
  workflow.setAttribute("aria-labelledby", "install-heading");
  workflow.innerHTML = `<div class="dialog-inner">
    <div class="dialog-title"><h3 id="install-heading" data-i18n="installHeading"></h3><button class="close" id="install-close" type="button" aria-label="\u5173\u95ED">\xD7</button></div>
    <div class="dialog-body">
      <p id="simulation-notice" class="notice" hidden></p>
      <p id="workflow-status" role="status" aria-live="polite"></p><p id="workflow-error" role="alert" hidden></p>
      <section id="connect-section">
        <p data-i18n="preflight"></p>
        <label class="confirm"><input type="checkbox" id="physical-board"><span data-i18n="boardConfirm"></span></label>
        <label class="field" for="expected-mac" data-i18n="macLabel"></label>
        <input id="expected-mac" type="text" placeholder="aa:bb:cc:dd:ee:ff" autocomplete="off" spellcheck="false" maxlength="17" aria-describedby="mac-hint">
        <p class="field-hint" id="mac-hint" data-i18n="macHint"></p>
        <button id="connect-read" class="install-primary" type="button" data-i18n="connectRead" disabled></button>
      </section>
      <section id="plan-section" hidden>
        <h4 id="plan-title"></h4><p id="identity-display"></p><p id="plan-summary"></p>
        <details id="plan-details" open><summary data-i18n="plan"></summary><pre id="plan-json"></pre></details>
        <p id="write-lock" class="notice" hidden></p>
        <div id="confirm-section">
          <label class="confirm"><input type="checkbox" id="backup-confirmed"><span data-i18n="backup"></span></label>
          <label class="confirm"><input type="checkbox" id="tf-removed"><span data-i18n="tfRemoved"></span></label>
          <label class="confirm"><input type="checkbox" id="plan-confirmed"><span data-i18n="approve"></span></label>
          <label class="confirm notice" id="erase-label" hidden><input type="checkbox" id="erase-confirmed"><span data-i18n="erase"></span></label>
          <label class="field" id="current-app-label"><span data-i18n="currentApp"></span><input type="file" id="current-app" accept=".bin"></label>
          <button id="execute-plan" class="install-primary" type="button" data-i18n="run" disabled></button>
        </div>
        <progress id="write-progress" max="1" value="0" aria-label="Installation progress" hidden></progress>
        <p data-i18n="tfGuide"></p>
      </section>
      <p id="finished-note" class="notice" hidden></p>
      <div class="workflow-actions"><button type="button" id="save-receipt" data-i18n="export" disabled></button><button type="button" id="stop-close" data-i18n="close"></button></div>
    </div></div>`;
  document.body.append(workflow);
  if (bindSelectedIdentity) {
    for (const el of [document.querySelector('label[for="expected-mac"]'), $("expected-mac"), $("mac-hint")]) el.hidden = true;
  }
  const supported = simulation || isSecureContext && !!navigator.serial?.requestPort;
  function updateLanguage() {
    document.documentElement.lang = language === "zh" ? "zh-CN" : "en";
    document.title = language === "zh" ? "Draftor \u8349\u7A3F\u673A \xB7 \u56FA\u4EF6\u5B89\u88C5" : "Draftor \xB7 Firmware Installer";
    document.querySelectorAll("[data-i18n]").forEach((el) => {
      el.textContent = t(el.dataset.i18n);
    });
    document.querySelector('[data-i18n="previewHint"]').textContent = simulation ? `${t("sim")} \xB7 ${scenario}` : t(hardwareFreshWritesEnabled ? "firstHint" : hardwareWritesEnabled ? "testHint" : "previewHint");
    $("board-name").textContent = language === "zh" ? "\u5FAE\u96EA ESP32-S3-RLCD-4.2" : "Waveshare ESP32-S3-RLCD-4.2";
    $("board-change").textContent = t(chooserOpen ? "hide" : "change");
    $("lang-toggle").setAttribute("aria-label", language === "zh" ? "Switch language to English" : "\u5207\u6362\u4E3A\u4E2D\u6587");
    for (const id of ["dialog-close", "install-close"]) $(id).setAttribute("aria-label", t("close"));
    $("simulation-notice").textContent = `${t("sim")} \xB7 ${scenario || ""}. ${t("simHint")}`;
    $("simulation-notice").hidden = !simulation;
    $("stop-close").textContent = t(busy ? "stop" : "close");
    $("workflow-status").textContent = t(lastPhase);
    if (errorCode) $("workflow-error").textContent = `${errorCode}: ${(errors[errorCode] || errors.IO)[language === "zh" ? 0 : 1]}`;
    if (session?.plan) renderPlan();
    if (modalKey && dialog.open) renderInfo();
    if (session?.state === "VERIFIED_BOOT_PENDING") $("finished-note").textContent = `${simulation ? t("simHint") + " " : ""}${t("finished")}`;
  }
  function renderInfo() {
    const c = dict[language].dialogs[modalKey];
    $("dialog-heading").textContent = c.title;
    $("dialog-body").innerHTML = c.body;
  }
  function state(phase, receipt = {}) {
    lastPhase = `phase_${phase}`;
    if (phase === "STOP" || phase === "VERIFIED_BOOT_PENDING") $("plan-details").open = false;
    if (phase === "STOP" && receipt.error) errorCode = receipt.error;
    $("workflow-error").hidden = !errorCode;
    if (receipt.progress !== void 0) {
      $("write-progress").hidden = false;
      $("write-progress").value = receipt.progress;
    }
    $("save-receipt").disabled = !session;
    updateLanguage();
  }
  function renderPlan() {
    const p = session.plan;
    $("plan-section").hidden = false;
    $("plan-title").textContent = `${t(p.initialState === "waveshare-factory-20260120" ? "factory" : p.mode === "fresh" ? "fresh" : "update")} \xB7 ${p.appSlot} \xB7 ${p.appOffset}`;
    $("plan-summary").textContent = t(p.mode === "fresh" ? "freshSummary" : "updateSummary");
    $("plan-json").textContent = JSON.stringify(p, null, 2);
    $("identity-display").textContent = bindSelectedIdentity ? `${t("identityBound")} ${session.selectedIdentity || ""}` : "";
    $("erase-label").hidden = p.mode !== "fresh";
    $("current-app-label").hidden = p.mode === "fresh";
    $("write-lock").hidden = simulation || p.hardwareWriteAllowed;
    $("write-lock").textContent = t(p.mode === "fresh" ? "freshLocked" : "locked");
    enableExecution();
  }
  function enableExecution() {
    const p = session?.plan;
    $("execute-plan").disabled = busy || session?.state !== "PLAN_READY" || !p || !simulation && !p.hardwareWriteAllowed || !["backup-confirmed", "tf-removed", "plan-confirmed"].every((id) => $(id).checked) || p.mode === "fresh" && !$("erase-confirmed").checked;
  }
  function fail(error) {
    errorCode = error instanceof Stop ? error.code : error?.name === "NotFoundError" ? "CONNECTION" : "IO";
    if (session && session.state !== "STOP") session.cancel(errorCode);
    lastPhase = "phase_STOP";
    $("workflow-error").hidden = false;
    updateLanguage();
  }
  function saveReceipt() {
    if (!session) return;
    const url = URL.createObjectURL(new Blob([JSON.stringify(session.receipt(), null, 2) + "\n"], { type: "application/json" }));
    const link = document.createElement("a");
    link.href = url;
    link.download = `Draftor-${simulation ? "SIMULATION" : "LOCAL"}-receipt.json`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1e3);
  }
  async function closeWorkflow() {
    if (busy) {
      session?.cancel();
      return;
    }
    dialogEpoch++;
    try {
      await session?.close();
    } catch (error) {
      fail(error);
      return;
    }
    $("expected-mac").value = "";
    $("current-app").value = "";
    workflow.close();
    $("install-btn").focus();
  }
  $("install-btn").addEventListener("click", async () => {
    if (workflow.open || busy) return;
    const epoch = ++dialogEpoch;
    release = void 0;
    session = void 0;
    errorCode = "";
    lastPhase = supported ? "preparing" : "unsupported";
    $("workflow-error").hidden = true;
    $("plan-section").hidden = true;
    $("finished-note").hidden = true;
    $("connect-section").hidden = !supported;
    $("connect-read").disabled = true;
    $("save-receipt").disabled = true;
    $("expected-mac").value = expectedMac;
    for (const id of ["physical-board", "backup-confirmed", "tf-removed", "plan-confirmed", "erase-confirmed"]) $(id).checked = false;
    $("current-app").value = "";
    $("write-progress").hidden = true;
    $("plan-details").open = true;
    updateLanguage();
    workflow.showModal();
    if (!supported) return;
    try {
      const loaded = await loadRelease(new URL("../firmware/", import.meta.url));
      if (!workflow.open || epoch !== dialogEpoch) return;
      release = loaded;
      lastPhase = "ready";
      $("connect-read").disabled = false;
      updateLanguage();
    } catch (error) {
      if (workflow.open && epoch === dialogEpoch) fail(error);
    }
  });
  $("connect-read").addEventListener("click", async () => {
    if (busy || !release) return;
    try {
      if (!$("physical-board").checked) throw new Stop("BOARD");
      if (!bindSelectedIdentity) normalizeMac($("expected-mac").value);
    } catch (error) {
      fail(error);
      return;
    }
    busy = true;
    $("connect-read").disabled = true;
    errorCode = "";
    try {
      const driver = await requestDriver(release);
      session = sessionFactory(driver, release, { hardwareWritesEnabled, hardwareFreshWritesEnabled, bindSelectedIdentity, onState: state });
      await session.prepare({ boardConfirmed: $("physical-board").checked, expectedMac: $("expected-mac").value });
      $("connect-section").hidden = true;
    } catch (error) {
      fail(error);
    } finally {
      busy = false;
      updateLanguage();
      enableExecution();
    }
  });
  $("execute-plan").addEventListener("click", async () => {
    if (busy || $("execute-plan").disabled) return;
    busy = true;
    errorCode = "";
    $("confirm-section").hidden = true;
    enableExecution();
    try {
      const file = $("current-app").files[0];
      if (file && (file.size < 24 || file.size > PINS.slotCapacity)) throw new Stop("CURRENT_APP");
      const currentApp = file ? new Uint8Array(await file.arrayBuffer()) : void 0;
      await session.execute({
        backupConfirmed: $("backup-confirmed").checked,
        tfRemoved: $("tf-removed").checked,
        planConfirmed: $("plan-confirmed").checked,
        eraseConfirmed: $("erase-confirmed").checked,
        currentApp
      });
      $("finished-note").hidden = false;
    } catch (error) {
      fail(error);
    } finally {
      busy = false;
      $("current-app").value = "";
      updateLanguage();
      enableExecution();
    }
  });
  for (const id of ["backup-confirmed", "tf-removed", "plan-confirmed", "erase-confirmed"]) $(id).addEventListener("change", enableExecution);
  $("save-receipt").addEventListener("click", saveReceipt);
  $("stop-close").addEventListener("click", closeWorkflow);
  $("install-close").addEventListener("click", closeWorkflow);
  workflow.addEventListener("cancel", (event) => {
    event.preventDefault();
    closeWorkflow();
  });
  workflow.addEventListener("close", () => {
    $("confirm-section").hidden = false;
  });
  $("lang-toggle").addEventListener("click", () => {
    language = language === "zh" ? "en" : "zh";
    updateLanguage();
  });
  $("board-change").addEventListener("click", () => {
    chooserOpen = !chooserOpen;
    $("board-chooser").hidden = !chooserOpen;
    $("board-change").setAttribute("aria-expanded", String(chooserOpen));
    updateLanguage();
  });
  document.querySelectorAll("[data-modal]").forEach((el) => el.addEventListener("click", () => {
    modalKey = el.dataset.modal;
    renderInfo();
    dialog.showModal();
  }));
  $("dialog-close").addEventListener("click", () => dialog.close());
  dialog.addEventListener("click", (event) => {
    if (event.target === dialog) dialog.close();
  });
  addEventListener("beforeunload", (event) => {
    if (busy) {
      event.preventDefault();
      event.returnValue = "";
    }
  });
  updateLanguage();
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
  constructor(port, { writeTimeout = 1e4, closeTimeout = 2e3 } = {}) {
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
      await this.#port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: "none", flowControl: "none", bufferSize: 65536 });
      this.#opened = true;
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
    this.#dead = true;
    this.#reader?.cancel().catch(() => {
    });
    this.#writer?.abort().catch(() => {
    });
  }
  async disconnect() {
    this.#dead = true;
    if (this.#reader) {
      await this.#bounded(this.#reader.cancel().catch(() => {
      }), this.#closeTimeout);
      this.#reader.releaseLock();
      this.#reader = void 0;
    }
    if (this.#opened) {
      await this.#bounded(this.#port.close(), this.#closeTimeout);
      this.#opened = false;
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
  return { chip: "ESP32-S3", securityKnown: true, secureBoot: false, flashEncryption: false };
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
  constructor(wire, { allowUpdate = false, allowFirstInstall = false } = {}) {
    requireSafe(!(allowUpdate && allowFirstInstall), "BUILD_SCOPE");
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
    return { value: u32(packet, 4), data: packet.slice(8, 8 + dataBytes) };
  }
  async #command(op, payload = new Uint8Array(), { dataBytes = 0, checksum = 0, timeout = 1e4, responses = 1 } = {}) {
    this.#alive();
    requireSafe(!this.#inFlight, "BUSY");
    this.#inFlight = true;
    try {
      requireSafe(Object.values(OP).includes(op) && payload.length <= 1040, "PROTOCOL");
      const header = new Uint8Array(8);
      header[1] = op;
      new DataView(header.buffer).setUint16(2, payload.length, true);
      new DataView(header.buffer).setUint32(4, checksum, true);
      await this.#wire.write(concat(header, payload));
      let reply;
      for (let i = 0; i < responses; i++) {
        reply = await this.#reply(op, dataBytes, timeout);
        if (op === OP.SYNC) requireSafe(reply.value !== 0, "STUB");
      }
      return reply;
    } catch (error) {
      this.#fatal = true;
      throw error instanceof Stop ? error : new Stop("IO");
    } finally {
      this.#inFlight = false;
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
  async writeImage(offset, image, { mode, slotOffset, progress = () => {
  } } = {}) {
    if (mode === "fresh") return this.#writeFirstImage(offset, image, progress);
    const data = image.slice();
    this.#range(offset, data.length);
    requireSafe(this.#writesAllowed, "DEVICE_WRITES_LOCKED");
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
  async eraseAll({ source, expectedMac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed } = {}) {
    this.#range(0, PINS.flashBytes);
    requireSafe(this.#firstAllowed && !this.#writesAllowed, "FRESH_DEVICE_PENDING");
    requireSafe(backupConfirmed === true && tfRemoved === true && planConfirmed === true, "BACKUP");
    requireSafe(eraseConfirmed === true, "ERASE_CONFIRM");
    requireSafe(this.#firstStage === "IDLE" && !this.#writeUsed, "STATE");
    this.#firstStage = "RESERVED";
    this.#writeUsed = true;
    try {
      const info = await this.identity();
      requireSafe(normalizeMac(info.mac) === normalizeMac(expectedMac), "IDENTITY");
      await verifyInitialFlash(this, source);
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
    this.#wire.cancel();
  }
  async disconnect() {
    this.#fatal = true;
    await this.#wire.disconnect();
  }
};

// src/public-main.js
var ReadOnlyDriver = class {
  #rom;
  kind = "hardware";
  constructor(transport) {
    this.#rom = new S3Rom(transport);
  }
  connect() {
    return this.#rom.connect();
  }
  identity() {
    return this.#rom.identity();
  }
  readFlash(offset, length) {
    return this.#rom.readFlash(offset, length);
  }
  flashMd5(offset, length) {
    return this.#rom.flashMd5(offset, length);
  }
  writeImage() {
    throw new Stop("DEVICE_WRITES_LOCKED");
  }
  eraseAll() {
    throw new Stop("DEVICE_WRITES_LOCKED");
  }
  cancel() {
    this.#rom.cancel();
  }
  disconnect() {
    return this.#rom.disconnect();
  }
};
startUI({
  publicRelease: true,
  bindSelectedIdentity: true,
  requestDriver: async () => new ReadOnlyDriver(new StrictSerialTransport(await navigator.serial.requestPort()))
});
export {
  ReadOnlyDriver
};
//# sourceMappingURL=app.js.map
