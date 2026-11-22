(() => {
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
        function hex(x) {
          var i;
          for (i = 0; i < x.length; i += 1) {
            x[i] = rhex(x[i]);
          }
          return x.join("");
        }
        if (hex(md51("hello")) !== "5d41402abc4b2a76b9719d911017c592") {
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
        function hexToBinaryString(hex2) {
          var bytes = [], length = hex2.length, x;
          for (x = 0; x < length - 1; x += 2) {
            bytes.push(parseInt(hex2.substr(x, 2), 16));
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
          ret = hex(this._hash);
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
          var hash = md51(content), ret = hex(hash);
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
          ret = hex(this._hash);
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
          var hash = md51_array(new Uint8Array(arr)), ret = hex(hash);
          return raw ? hexToBinaryString(ret) : ret;
        };
        return SparkMD52;
      });
    }
  });

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

  // src/recovery-package.js
  var RECOVERY_SCHEMA = "draftor-private-recovery/1";
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
    const table = bytes.subarray(32768, 36864), tableSha = await sha256(table);
    if (tableSha === FACTORY.tableSha256) {
      for (const pin of FACTORY.fingerprint) requireSafe(md5(bytes.subarray(pin.offset, pin.offset + pin.bytes)) === pin.md5, "FACTORY_FINGERPRINT");
      return { firmware: "waveshare-factory-20260120", rule: "FACTORY_FIXED_THREE_RANGES_V1", partition: { offset: 32768, bytes: 4096, sha256: tableSha, md5: md5(table), rows: FACTORY.partitions }, ota: { applicable: false }, activeApp: "factory" };
    }
    const rows = checkPartitions(table);
    requireSafe(await sha256(table.subarray(0, 3072)) === PINS.files["partitions.bin"].sha256, "PARTITIONS");
    const ota = bytes.subarray(57344, 65536);
    requireSafe(activeSlot(ota) === 0, "UNQUALIFIED_SLOT");
    for (const name of ["bootloader.bin", "app.bin"]) {
      const p = PINS.files[name];
      requireSafe(await sha256(bytes.subarray(p.offset, p.offset + p.bytes)) === p.sha256, "UNQUALIFIED_FIRMWARE");
    }
    return {
      firmware: "draftor-alpha3-app0",
      rule: "ALPHA3_BOOT_APP0_B_TABLE_V1",
      partition: { offset: 32768, bytes: 4096, sha256: tableSha, md5: md5(table), rows },
      ota: { applicable: true, offset: 57344, bytes: 8192, sha256: await sha256(ota), md5: md5(ota), entries: [0, 4096].map((at) => ({ sequence: u32(ota, at), state: u32(ota, at + 24), crc: u32(ota, at + 28) })) },
      activeApp: "app0"
    };
  }
  var readmes = {
    "README.zh-CN.txt": "\u79C1\u4EBA\u5B8C\u6574 Flash \u6062\u590D\u5305\u3002\u4E0D\u8981\u4E0A\u4F20\u3001\u516C\u5F00\u6216\u53D1\u9001\u7ED9\u4ED6\u4EBA\u3002\u4EC5\u9002\u7528\u4E8E\u521B\u5EFA\u65F6\u7684\u539F\u5FAE\u96EA ESP32-S3-RLCD-4.2 / ST7305\u3002\u6062\u590D\u8986\u76D6\u5168\u90E8 16 MiB \u5185\u90E8 Flash\uFF0C\u8BBE\u7F6E\u3001\u914D\u5BF9\u53CA\u5185\u90E8\u6587\u7A3F\u56DE\u5230\u5907\u4EFD\u65F6\uFF1BTF \u6587\u4EF6\u4E0D\u5728\u672C\u5305\u4E2D\uFF0C\u5FC5\u987B\u72EC\u7ACB\u4FDD\u7559\u3002\u6062\u590D\u5382\u5BB6\u5907\u4EFD\u4F1A\u56DE\u5230\u5382\u5BB6\u6F14\u793A\uFF0C\u4E0D\u4F1A\u5B89\u88C5 Draftor\u3002\u4F7F\u7528\u968F\u9644\u79BB\u7EBF\u5DE5\u5177\uFF0C\u9009\u62E9\u672C ZIP \u5E76\u8FDE\u63A5\u539F\u8BBE\u5907\uFF1B\u6821\u9A8C\u901A\u8FC7\u540E\u624D\u786E\u8BA4\u6062\u590D\u3002\u5B8C\u6210\u540E\u5FC5\u987B\u65AD\u7535\u3001\u63D2\u56DE TF\u3001\u666E\u901A\u542F\u52A8\u9A8C\u6536\u3002\u6458\u8981\u548C MAC \u53EA\u9632\u8BEF\u9009\u53CA\u610F\u5916\u635F\u574F\uFF0C\u4E0D\u662F\u6570\u5B57\u7B7E\u540D\u6216\u8EAB\u4EFD\u8BA4\u8BC1\u3002\n",
    "README.en.txt": "PRIVATE full Flash recovery package. Do not upload, publish or share. Only for the original Waveshare ESP32-S3-RLCD-4.2 / ST7305. Recovery overwrites all 16 MiB of internal Flash and rolls settings, pairing and internal documents back to backup time. TF files are NOT included; preserve TF separately. Restoring a factory backup returns the factory demo, not Draftor. Open the offline recovery tool, select this ZIP, connect the original board and confirm only after checks pass. After verification, power off, insert TF and validate normal boot. Hashes and MAC are accidental-corruption / wrong-device safeguards, not signatures or authentication.\n"
  };
  async function createRecoveryPackage(bytes, info, { boardConfirmed, romMd5Before, romMd5After, method, build, sourceCommit, createdAt = (/* @__PURE__ */ new Date()).toISOString(), id = crypto.randomUUID() } = {}) {
    requireSafe(boardConfirmed === true, "BOARD_CONFIRM");
    const image = bytes.slice(), device = checkedIdentity(info), digest = md5(image);
    requireSafe(romMd5Before === digest && romMd5After === digest, "BACKUP_STALE");
    requireSafe(["ROM_READ_SLOW_MD5_V1", "ROM_MD5_MATCH_EXISTING_V1"].includes(method), "PACKAGE_PROOF");
    const original = await classifyRecoveryImage(image);
    const manifest = {
      schema: RECOVERY_SCHEMA,
      id,
      createdAt,
      tool: { name: "Draftor Recovery", build, sourceCommit },
      board: { model: "ESP32-S3-RLCD-4.2", controller: "ST7305", confirmation: "USER_PHYSICAL_CONFIRMATION" },
      device,
      image: { file: "flash.bin", offset: 0, bytes: PINS.flashBytes, sha256: await sha256(image), md5: digest },
      creationProof: { method, romMd5Before, romMd5After, romMd5Match: true, identitySource: "ROM_READ_REG_EFUSE", securitySource: "ROM_GET_SECURITY_INFO", capacitySource: "JEDEC_RDID" },
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
    requireSafe(m.schema === RECOVERY_SCHEMA, "PACKAGE_SCHEMA");
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
    keys(m.creationProof, "method,romMd5Before,romMd5After,romMd5Match,identitySource,securitySource,capacitySource");
    requireSafe(m.creationProof.identitySource === "ROM_READ_REG_EFUSE" && m.creationProof.securitySource === "ROM_GET_SECURITY_INFO" && m.creationProof.capacitySource === "JEDEC_RDID", "PACKAGE_PROOF");
    requireSafe(["ROM_READ_SLOW_MD5_V1", "ROM_MD5_MATCH_EXISTING_V1"].includes(m.creationProof.method) && m.creationProof.romMd5Match === true && m.creationProof.romMd5Before === m.image.md5 && m.creationProof.romMd5After === m.image.md5, "PACKAGE_PROOF");
    requireSafe(JSON.stringify(await classifyRecoveryImage(image)) === JSON.stringify(m.original), "PACKAGE_METADATA");
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
    diagnostics?.check("partitionRead", true);
    const tableMatch = md5(table) === await driver.flashMd5(32768, 4096);
    diagnostics?.check("partitionMd5Match", tableMatch);
    requireSafe(tableMatch, "READBACK");
    if (await sha256(table) === FACTORY.tableSha256) {
      for (const p of FACTORY.fingerprint) requireSafe(await driver.flashMd5(p.offset, p.bytes) === p.md5, "FACTORY_FINGERPRINT");
      diagnostics?.check("otaRead", "NOT_APPLICABLE_FACTORY");
      diagnostics?.check("otaMd5Match", "NOT_APPLICABLE_FACTORY");
      table.fill(0);
      return { firmware: "waveshare-factory-20260120", activeApp: "factory" };
    }
    checkPartitions(table);
    requireSafe(await sha256(table.subarray(0, 3072)) === PINS.files["partitions.bin"].sha256, "PARTITIONS");
    table.fill(0);
    const ota = await driver.readFlash(57344, 8192);
    diagnostics?.check("otaRead", true);
    const otaMatch = md5(ota) === await driver.flashMd5(57344, 8192);
    diagnostics?.check("otaMd5Match", otaMatch);
    requireSafe(otaMatch, "READBACK");
    requireSafe(activeSlot(ota) === 0, "UNQUALIFIED_SLOT");
    ota.fill(0);
    for (const name of ["bootloader.bin", "app.bin"]) {
      const p = PINS.files[name];
      requireSafe(await driver.flashMd5(p.offset, p.bytes) === p.md5, "UNQUALIFIED_FIRMWARE");
    }
    return { firmware: "draftor-alpha3-app0", activeApp: "app0" };
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

  // src/recovery-copy.js
  var recoveryCopy = { zh: {
    title: "\u5907\u4EFD\u4E0E\u6062\u590D",
    intro: "\u4E3A\u539F\u5FAE\u96EA\u4FDD\u5B58\u79C1\u4EBA\u6062\u590D\u5305\uFF0C\u6216\u628A\u539F\u8BBE\u5907\u6062\u590D\u5230\u5907\u4EFD\u65F6\u3002\u6587\u4EF6\u53EA\u5728\u672C\u673A\u5904\u7406\uFF0CTF \u6587\u7A3F\u53E6\u884C\u4FDD\u7559\u3002",
    locked: "\u53EA\u8BFB\u8BCA\u65AD\u7248\uFF1A\u53EF\u4EE5\u68C0\u67E5\u3001\u4FDD\u5B58\u548C\u56DE\u9009\u5907\u4EFD\uFF1B\u5B89\u88C5\u4E0E\u6574\u7247\u6062\u590D\u5199\u5165\u5747\u9501\u5B9A\u3002",
    candidate: "\u6062\u590D\u5019\u9009 \xB7 \u4EC5\u4F9B\u672C\u8F6E\u5BA1\u6838\uFF1A\u5177\u5907\u771F\u5B9E\u6574\u7247\u6062\u590D\u80FD\u529B\uFF0C\u5C1A\u672A\u516C\u5F00\u53D1\u5E03\u3002",
    mock: "SOFTWARE TEST \xB7 \u5408\u6210\u8BBE\u5907\uFF0C\u4E0D\u8FDE\u63A5 USB\u3002\u6B64\u7ED3\u679C\u4E0D\u80FD\u66FF\u4EE3\u5B9E\u673A\u9A8C\u6536\u3002",
    backupMode: "\u4FDD\u5B58\u79C1\u4EBA\u6062\u590D\u5305",
    restoreMode: "\u6062\u590D\u539F\u8BBE\u5907",
    verifyMode: "\u518D\u6B21\u53EA\u8BFB\u6838\u9A8C",
    selectTitle: "1. \u9009\u62E9\u5DF2\u4FDD\u5B58\u7684\u6062\u590D\u5305",
    selectHelp: "\u9009\u62E9\u672C\u5DE5\u5177\u521B\u5EFA\u7684\u5355\u4E2A ZIP\uFF0C\u4E0D\u8981\u89E3\u538B\u3002\u53EA\u63A5\u53D7\u5DF2\u9A8C\u6536\u7684 Alpha 3 app0 \u6216\u51C6\u786E\u5FAE\u96EA\u5382\u5BB6\u72B6\u6001\u3002\u4E0D\u4F1A\u56E0\u4E3A\u5F53\u524D\u8BBE\u5907\u5206\u533A\u635F\u574F\u800C\u6539\u4E3A\u5B89\u88C5\u56FA\u4EF6\u3002",
    packageFile: "\u79C1\u4EBA\u6062\u590D\u5305\uFF08.zip\uFF0C\u4EC5\u672C\u5730\uFF09",
    connectTitle: "\u8FDE\u63A5\u539F\u5FAE\u96EA",
    prepare: "\u4FDD\u5B58\u6587\u7A3F\u5E76\u7ED3\u675F\u4F20\u8F93\uFF0C\u5B8C\u5168\u65AD\u7535\uFF08\u542B\u7535\u6C60\uFF09\u540E\u53D6\u51FA TF\u3002\u53EA\u8FDE\u63A5\u8FD9\u5757\u5FAE\u96EA\uFF0C\u6309\u4F4F BOOT \u63A5 USB\uFF0C\u7EA6\u4E24\u79D2\u540E\u677E\u5F00\uFF0C\u4FDD\u6301 ROM\uFF1B\u5173\u95ED\u4E32\u53E3\u76D1\u89C6\u5668\u3002\u8FDE\u63A5\u65F6\u9009\u62E9 USB JTAG/serial debug unit\u3002",
    board: "\u6211\u5DF2\u6838\u5BF9\u5B9E\u7269\u662F\u5FAE\u96EA ESP32-S3-RLCD-4.2 / ST7305\uFF0C\u4E14\u4EC5\u8FD9\u5757\u677F\u8FDE\u63A5\u3002",
    tf: "\u6587\u7A3F\u5DF2\u4FDD\u5B58\u3001\u4F20\u8F93\u5DF2\u7ED3\u675F\uFF0CTF \u5DF2\u5728\u5F7B\u5E95\u65AD\u7535\u540E\u53D6\u51FA\u3002",
    connect: "\u9009\u62E9\u4E32\u53E3\u5E76\u53EA\u8BFB\u68C0\u67E5",
    saveTitle: "\u4FDD\u5B58\u4E00\u4EFD\u5B8C\u6574\u6062\u590D\u5305",
    backupHelp: "\u5148\u6BD4\u5BF9\u5DF2\u6709\u6574\u7247\u5907\u4EFD\uFF1B\u82E5\u6CA1\u6709\u7B26\u5408\u5F53\u524D\u8BBE\u5907\u72B6\u6001\u7684\u5907\u4EFD\uFF0C\u518D\u8BFB\u53D6\u5B8C\u6574 16 MiB\u3002ROM \u6162\u8BFB\u9700\u8981\u8F83\u957F\u65F6\u95F4\uFF0C\u8BF7\u4FDD\u6301\u6B64\u9875\u548C\u8FDE\u63A5\u3002",
    existingTitle: "\u6211\u5DF2\u6709\u5B8C\u6574 Flash .bin",
    existingHelp: "\u9009\u62E9\u539F\u6765\u4FDD\u5B58\u7684 16 MiB \u6574\u7247\u6587\u4EF6\u3002\u5DE5\u5177\u4F1A\u4E0E\u5F53\u524D\u539F\u8BBE\u5907\u7684 ROM \u6458\u8981\u6BD4\u5BF9\uFF1B\u4E0D\u63A5\u53D7 App \u5207\u7247\uFF0C\u4E0D\u9700\u8981\u8F93\u5165 MAC\u3002\u82E5\u4E0D\u5339\u914D\uFF0C\u8BF7\u4FDD\u7559\u65E7\u6587\u4EF6\uFF0C\u518D\u8BFB\u53D6\u5F53\u524D\u5907\u4EFD\u3002",
    rawFile: "\u5DF2\u6709\u5B8C\u6574 Flash\uFF08.bin\uFF0C16 MiB\uFF09",
    capture: "\u8BFB\u53D6\u5F53\u524D\u5B8C\u6574 Flash\uFF08\u53EA\u8BFB\uFF09",
    savePackage: "\u4E0B\u8F7D\u79C1\u4EBA\u6062\u590D\u5305",
    saveWarning: "\u70B9\u51FB\u4E0B\u8F7D\u4E0D\u4EE3\u8868\u6587\u4EF6\u5DF2\u4FDD\u5B58\u3002\u786E\u8BA4\u6D4F\u89C8\u5668\u4E0B\u8F7D\u5B8C\u6210\u540E\uFF0C\u91CD\u65B0\u9009\u62E9\u521A\u4FDD\u5B58\u7684 ZIP\uFF1B\u8BF7\u5C06\u5305\u548C\u79BB\u7EBF\u5DE5\u5177\u4E00\u8D77\u7559\u5728\u79C1\u4EBA\u7684\u672C\u673A\u76EE\u5F55\u3002",
    reimport: "\u56DE\u9009\u5DF2\u4FDD\u5B58\u7684\u6062\u590D\u5305",
    saved: "\u4FDD\u5B58\u6587\u4EF6\u56DE\u9009\u901A\u8FC7\uFF0C\u4E0E\u5F53\u524D\u539F\u8BBE\u5907\u5B8C\u6574 Flash \u4E00\u81F4\u3002",
    restoreTitle: "\u786E\u8BA4\u6574\u7247\u6062\u590D",
    impact: "\u5C06\u8986\u76D6\u5168\u90E8 16 MiB \u5185\u90E8 Flash\uFF0C\u5305\u62EC\u56FA\u4EF6\u3001\u8BBE\u7F6E\u3001\u952E\u76D8\u914D\u5BF9\u548C\u5185\u90E8\u6587\u7A3F\u3002\u5907\u4EFD\u540E\u4EA7\u751F\u7684\u5185\u90E8\u53D8\u5316\u4F1A\u4E22\u5931\u3002TF \u4E0D\u5728\u5305\u5185\u4E14\u5FC5\u987B\u4FDD\u6301\u53D6\u51FA\u3002\u4E2D\u65AD\u53EF\u80FD\u65E0\u6CD5\u542F\u52A8\uFF1B\u5931\u8D25\u5373\u505C\u6B62\uFF0C\u4E0D\u81EA\u52A8\u91CD\u8BD5\u6216\u91CD\u542F\u3002",
    consent: "\u6211\u7406\u89E3\u8986\u76D6\u8303\u56F4\uFF0C\u786E\u8BA4\u8FD9\u662F\u539F\u8BBE\u5907\u53CA\u5E94\u6062\u590D\u7684\u79C1\u4EBA\u5305\uFF0C\u540C\u610F\u6267\u884C\u8FD9\u4E00\u6B21\u5B8C\u6574\u6062\u590D\u3002",
    restore: "\u6062\u590D\u539F\u8BBE\u5907\u7684\u5B8C\u6574 Flash",
    verifyHelp: "\u8FD9\u662F\u65B0\u8FDE\u63A5\u7684\u53EA\u8BFB\u6838\u9A8C\uFF1A\u8BFB\u53D6\u5B9E\u9645\u8BBE\u5907\u6458\u8981\u548C\u5206\u533A\uFF0C\u4E0D\u6CBF\u7528\u4E0A\u6B21\u6210\u529F\u7ED3\u679C\uFF0C\u4E5F\u4E0D\u6388\u4E88\u5199\u5165\u6743\u9650\u3002",
    verify: "\u6838\u9A8C\u8BBE\u5907\u5B9E\u9645\u6062\u590D\u5185\u5BB9",
    bootTitle: "\u5185\u5BB9\u5DF2\u6821\u9A8C\uFF0C\u7B49\u5F85\u666E\u901A\u542F\u52A8",
    bootHelp: "\u5148\u70B9\u201C\u5173\u95ED\u8FDE\u63A5\u5E76\u6E05\u9664\u79C1\u4EBA\u4F1A\u8BDD\u201D\u3002\u5F7B\u5E95\u65AD\u7535\uFF08\u542B\u7535\u6C60\uFF09\uFF0C\u63D2\u56DE\u539F TF\uFF0C\u677E\u5F00 BOOT \u666E\u901A\u542F\u52A8\u3002\u539F\u952E\u76D8\u7528\u539F\u901A\u9053\u76F4\u63A5\u56DE\u8FDE\uFF0C\u68C0\u67E5\u65E7\u7A3F\u3001\u65B0\u7A3F\u4FDD\u5B58\u548C\u91CD\u542F\u8BFB\u53D6\uFF1B\u4E0D\u8981\u91CD\u7F6E\u539F\u914D\u5BF9\u3002",
    factoryHelp: "\u82E5\u5305\u6765\u81EA\u5382\u5BB6\u6F14\u793A\uFF0C\u6062\u590D\u540E\u5E94\u56DE\u5230\u5382\u5BB6\u6F14\u793A\uFF1B\u8FD9\u4E0D\u7B49\u4E8E\u5DF2\u5B89\u88C5 Draftor\u3002",
    offlineTitle: "\u4FDD\u7559\u79BB\u7EBF\u5DE5\u5177",
    offlineHelp: "\u4FDD\u5B58\u79BB\u7EBF HTML\u3002\u4EE5\u540E\u5B8C\u5168\u5173\u95ED\u7F51\u9875\u540E\uFF0C\u5728\u7535\u8111 Chrome \u4E2D\u91CD\u65B0\u6253\u5F00\u8BE5\u6587\u4EF6\u5373\u53EF\u4F7F\u7528\uFF1B\u65E0\u9700\u7F51\u7EDC\u3001Python \u6216\u7EC8\u7AEF\u3002",
    offlineLocked: "\u5F53\u524D\u53EF\u4E0B\u8F7D\u7684\u662F\u9501\u5199\u8BCA\u65AD\u79BB\u7EBF\u7248\uFF1B\u5B83\u4E0D\u80FD\u8FDB\u884C\u771F\u5B9E\u6062\u590D\u3002\u5177\u5907\u5199\u5165\u80FD\u529B\u7684\u5019\u9009\u4EC5\u968F\u9694\u79BB\u5BA1\u6838\u4EA4\u4ED8\uFF0C\u4E0D\u5728\u6B63\u5F0F\u7F51\u7AD9\u516C\u5F00\u3002",
    offlineLocal: "\u5F53\u524D\u5C31\u662F\u4FDD\u5B58\u5230\u672C\u673A\u7684\u79BB\u7EBF\u5DE5\u5177\uFF0C\u8BF7\u4FDD\u7559\u6B64 HTML \u548C\u79C1\u4EBA\u6062\u590D\u5305\u3002",
    offlineDownload: "\u4E0B\u8F7D\u79BB\u7EBF\u5DE5\u5177\uFF08HTML\uFF09",
    privacy: "\u79C1\u4EBA\u5305\u5305\u542B MAC\u3001\u8BBE\u7F6E\u3001\u914D\u5BF9\u548C\u53EF\u80FD\u7684\u5185\u90E8\u6587\u7A3F\uFF0C\u4E0D\u8981\u4E0A\u4F20\u6216\u516C\u5F00\u3002\u6458\u8981\u53CA MAC \u7528\u4E8E\u9632\u8BEF\u9009\u548C\u610F\u5916\u635F\u574F\uFF0C\u4E0D\u662F\u6570\u5B57\u7B7E\u540D\u3002TF \u4E0D\u4F1A\u88AB\u672C\u5DE5\u5177\u8BFB\u53D6\u6216\u4FEE\u6539\u3002",
    close: "\u5173\u95ED\u8FDE\u63A5\u5E76\u6E05\u9664\u79C1\u4EBA\u4F1A\u8BDD",
    receipt: "\u4FDD\u5B58\u8131\u654F\u8BCA\u65AD\u56DE\u6267",
    diagnostics: "\u672C\u6B21\u8BCA\u65AD\u8BB0\u5F55",
    diagnosticHelp: "\u8BA1\u6570\u6765\u81EA\u7A0B\u5E8F\u5B9E\u9645\u63A7\u5236\u70B9\uFF0C\u4E0D\u662F\u72EC\u7ACB\u6293\u7EBF\u8BC1\u636E\uFF1B\u672A\u8FD0\u884C\u9879\u4FDD\u7559 NOT_RUN\u3002\u56DE\u6267\u4E0D\u542B MAC\u3001\u79C1\u4EBA\u6458\u8981\u3001\u6587\u4EF6\u540D\u6216\u5185\u5BB9\u3002\u5173\u95ED\u540E\u518D\u4FDD\u5B58\uFF0C\u53EF\u5305\u542B\u5173\u95ED\u7ED3\u679C\u3002",
    guide: "\u5B8C\u6574\u6062\u590D\u8BF4\u660E",
    unsupported: "\u5F53\u524D\u73AF\u5883\u4E0D\u80FD\u8FDE\u63A5\u8BBE\u5907\u3002\u8BF7\u4F7F\u7528\u7535\u8111 Chrome \u7684 HTTPS\u3001localhost \u6216\u5DF2\u9A8C\u8BC1\u7684\u79BB\u7EBF\u6587\u4EF6\u5165\u53E3\uFF1B\u624B\u673A\u4EC5\u53EF\u9605\u8BFB\u548C\u4E0B\u8F7D\u3002",
    supported: "\u7535\u8111 Web Serial \u73AF\u5883\u53EF\u7528\u3002\u672A\u8FDE\u63A5\u524D\u4E0D\u4F1A\u8BBF\u95EE\u8BBE\u5907\u3002",
    selected: "\u6709\u6548\u6062\u590D\u5305",
    closed: "\u4F1A\u8BDD\u5DF2\u5173\u95ED\uFF0C\u79C1\u4EBA\u6587\u4EF6\u4E0E\u8EAB\u4EFD\u5F15\u7528\u5DF2\u6E05\u9664\u3002\u53EF\u4FDD\u5B58\u8131\u654F\u56DE\u6267\u3002",
    choose: "\u8BF7\u9009\u62E9\u6062\u590D\u5305\uFF0C\u518D\u8FDE\u63A5\u539F\u8BBE\u5907\u3002",
    readyBackup: "\u53EF\u8FDE\u63A5\u5E76\u53EA\u8BFB\u68C0\u67E5\uFF0C\u968F\u540E\u4FDD\u5B58\u5F53\u524D\u6062\u590D\u5305\u3002",
    errorFile: "\u6587\u4EF6\u672A\u901A\u8FC7\u68C0\u67E5\u3002\u539F\u6587\u4EF6\u672A\u4FEE\u6539\uFF1B\u8BF7\u9009\u62E9\u6B63\u786E\u7684\u5305\u3002",
    errorStop: "\u5DF2\u505C\u6B62\u3002\u8BF7\u4FDD\u7559\u73B0\u573A\u548C\u8BCA\u65AD\u56DE\u6267\uFF1B\u4E0D\u8981\u81EA\u52A8\u91CD\u8FDE\u3001\u91CD\u5237\u6216\u590D\u4F4D\u3002",
    errorStale: "\u8FD9\u4EFD\u65E7\u5907\u4EFD\u4E0E\u8BBE\u5907\u5F53\u524D\u5185\u5BB9\u4E0D\u540C\u3002\u8BF7\u4FDD\u7559\u65E7\u6587\u4EF6\uFF0C\u53EF\u4F7F\u7528\u4E0A\u65B9\u53EA\u8BFB\u6309\u94AE\u4FDD\u5B58\u5F53\u524D\u5B8C\u6574 Flash\u3002",
    errorClose: "\u5173\u95ED\u672A\u786E\u8BA4\u6210\u529F\u3002\u79C1\u4EBA\u5F15\u7528\u5DF2\u6E05\u9664\uFF0C\u4F46\u4E32\u53E3\u53EF\u80FD\u4ECD\u88AB\u5360\u7528\uFF1B\u8BF7\u5173\u95ED\u6B64\u6807\u7B7E\u5E76\u4FDD\u7559\u56DE\u6267\u3002",
    partial: "\u5199\u547D\u4EE4\u5DF2\u5C1D\u8BD5\uFF0C\u5185\u90E8 Flash \u53EF\u80FD\u4E0D\u5B8C\u6574\u3002\u4E0D\u80FD\u5BA3\u79F0\u6062\u590D\u6210\u529F\u3002",
    IDLE: "\u5C1A\u672A\u8FDE\u63A5",
    PACKAGE_VALIDATED: "\u6062\u590D\u5305\u5DF2\u6821\u9A8C\uFF0C\u5C1A\u672A\u8FDE\u63A5",
    CONNECTING: "\u6B63\u5728\u8FDE\u63A5 ROM\u2026",
    DEVICE_VERIFIED: "\u8BBE\u5907\u53EA\u8BFB\u68C0\u67E5\u901A\u8FC7",
    READY_FOR_CONFIRMATION: "\u539F\u8BBE\u5907\u5339\u914D\uFF0C\u7B49\u5F85\u6574\u7247\u6062\u590D\u786E\u8BA4",
    READING_BACKUP: "\u6B63\u5728\u8BFB\u53D6\u5B8C\u6574\u5907\u4EFD\u2026",
    SAVED_FILE_REIMPORT_REQUIRED: "\u79C1\u4EBA\u5305\u5DF2\u751F\u6210\uFF1B\u8BF7\u4E0B\u8F7D\uFF0C\u518D\u56DE\u9009\u4FDD\u5B58\u7684\u6587\u4EF6",
    BACKUP_SAVED_VERIFIED: "\u6062\u590D\u5305\u4FDD\u5B58\u4E0E\u56DE\u9009\u5DF2\u901A\u8FC7",
    RECHECKING: "\u6B63\u5728\u91CD\u65B0\u68C0\u67E5\u539F\u8BBE\u5907\u4E0E\u5B89\u5168\u72B6\u6001\u2026",
    RESTORING: "\u6B63\u5728\u6574\u7247\u6062\u590D\uFF0C\u8BF7\u52FF\u65AD\u5F00\u2026",
    VERIFYING: "\u6B63\u5728\u8BFB\u53D6 ROM \u6458\u8981\u5E76\u6838\u5BF9\u6062\u590D\u5185\u5BB9\u2026",
    VERIFIED_BOOT_PENDING: "\u6062\u590D\u5185\u5BB9\u5DF2\u6838\u9A8C\uFF1B\u4ECD\u9700\u666E\u901A\u542F\u52A8\u9A8C\u6536",
    STOP: "\u5DF2\u505C\u6B62"
  }, en: {
    title: "Backup & recovery",
    intro: "Save a private recovery package for your Waveshare board, or restore the original board to backup time. Files stay on this computer. Keep TF documents separately.",
    locked: "Read-only diagnostic build: inspect, save and reselect backups. Installation and full recovery writes are locked.",
    candidate: "Recovery review candidate \xB7 Real full-Flash restore capability. Not publicly released.",
    mock: "SOFTWARE TEST \xB7 Synthetic device, no USB. This is not hardware qualification.",
    backupMode: "Save a private package",
    restoreMode: "Restore original board",
    verifyMode: "New read-only verification",
    selectTitle: "1. Select your saved recovery package",
    selectHelp: "Select the single ZIP created by this tool; do not extract it. Only qualified Alpha 3 app0 or the exact Waveshare factory image is accepted. Damaged current partitions do not trigger a firmware installation.",
    packageFile: "Private recovery package (.zip, local only)",
    connectTitle: "Connect the original Waveshare",
    prepare: "Save writing and finish transfers. Fully power off including the battery, then remove TF. Connect only this board: hold BOOT while plugging in USB, release after about two seconds, and stay in ROM. Close serial monitors. Select USB JTAG/serial debug unit.",
    board: "I checked the physical Waveshare ESP32-S3-RLCD-4.2 / ST7305; only this board is connected.",
    tf: "Writing is saved, transfers have ended, and TF was removed after complete power-off.",
    connect: "Select port & inspect read-only",
    saveTitle: "Save a full recovery package",
    backupHelp: "Check an existing full backup first. If none matches the current device, read all 16 MiB. ROM slow reading takes time; keep this page and cable connected.",
    existingTitle: "I already have a full Flash .bin",
    existingHelp: "Select your saved full 16 MiB image. It is compared with the original device\u2019s current ROM digest. App slices are not accepted. No manual MAC entry. If it differs, keep the old file and capture a current backup.",
    rawFile: "Existing full Flash (.bin, 16 MiB)",
    capture: "Read current full Flash (read-only)",
    savePackage: "Download private recovery package",
    saveWarning: "Clicking Download does not prove saving. Wait for completion, then reselect the saved ZIP. Keep the package and offline tool together in a private local folder.",
    reimport: "Reselect saved recovery package",
    saved: "Saved package reselected and verified against the original device\u2019s current full Flash.",
    restoreTitle: "Confirm full recovery",
    impact: "This overwrites all 16 MiB of internal Flash: firmware, settings, keyboard pairing and internal documents. Internal changes after backup time are lost. TF is not included and must remain removed. Interruption may prevent boot. Errors stop the session; no automatic retry or reboot.",
    consent: "I understand the overwrite scope, confirm the original board and intended private package, and approve this one full recovery.",
    restore: "Restore the original board\u2019s full Flash",
    verifyHelp: "A new read-only connection checks actual ROM digests and metadata. It does not reuse a cached success or grant writing authority.",
    verify: "Verify actual restored device contents",
    bootTitle: "Contents verified; normal boot pending",
    bootHelp: "First close the connection and clear the private session. Fully power off including the battery, insert the original TF, release BOOT and start normally. Reconnect the keyboard on its original channel. Check old writing, a new saved document and reading after restart. Do not reset the original pairing.",
    factoryHelp: "A factory package restores the factory demo. It does not install Draftor.",
    offlineTitle: "Keep the offline tool",
    offlineHelp: "Save the offline HTML. Completely close the page and reopen this file in desktop Chrome whenever needed. No network, Python or terminal required.",
    offlineLocked: "The downloadable diagnostic offline build is write-locked and cannot restore. The actual recovery candidate is delivered separately for review, not published on the official site.",
    offlineLocal: "This is the saved local offline tool. Keep this HTML together with your private recovery package.",
    offlineDownload: "Download offline tool (HTML)",
    privacy: "Private packages contain MAC, settings, pairing and possibly internal writing. Never upload or publish them. Hashes and MAC prevent mistakes and accidental corruption; they are not signatures. This tool never reads or modifies TF.",
    close: "Close connection & clear private session",
    receipt: "Save sanitized diagnostic receipt",
    diagnostics: "Session diagnostics",
    diagnosticHelp: "Counters record actual program control points, not an independent wire capture. Unperformed checks remain NOT_RUN. Receipts exclude MAC, private hashes, filenames and contents. Save after closing to include its outcome.",
    guide: "Full recovery guide",
    unsupported: "Device access is unavailable here. Use desktop Chrome on HTTPS, localhost or the qualified offline-file entry. Mobile is for reading and downloads only.",
    supported: "Desktop Web Serial is available. No device is accessed before you connect.",
    selected: "Valid recovery package",
    closed: "Session closed; private file and identity references cleared. You can save the sanitized receipt.",
    choose: "Select a recovery package, then connect the original board.",
    readyBackup: "Connect for read-only checks, then save a current recovery package.",
    errorFile: "File validation failed. The original file is unchanged. Select the correct package.",
    errorStop: "Stopped. Preserve the device state and diagnostic receipt. Do not automatically reconnect, reflash or reset.",
    errorStale: "This old backup differs from the current board. Keep it and use the read-only capture button to save the current full Flash.",
    errorClose: "Port closure was not confirmed. Private references are cleared, but the port may remain occupied. Close this tab and keep the receipt.",
    partial: "A Flash write command was attempted. Internal Flash may be partial. Recovery has not passed.",
    IDLE: "Not connected",
    PACKAGE_VALIDATED: "Package validated, not connected",
    CONNECTING: "Connecting to ROM\u2026",
    DEVICE_VERIFIED: "Read-only device checks passed",
    READY_FOR_CONFIRMATION: "Original board matches; full recovery confirmation required",
    READING_BACKUP: "Reading complete backup\u2026",
    SAVED_FILE_REIMPORT_REQUIRED: "Package created; download and reselect the saved file",
    BACKUP_SAVED_VERIFIED: "Saved recovery package verified",
    RECHECKING: "Rechecking the original board and security\u2026",
    RESTORING: "Restoring full Flash; do not disconnect\u2026",
    VERIFYING: "Reading ROM digests and checking restored contents\u2026",
    VERIFIED_BOOT_PENDING: "Restored contents verified; normal boot pending",
    STOP: "Stopped"
  } };

  // src/recovery-ui.js
  function startRecoveryUI({ build, sourceCommit, recoveryEnabled = false, simulation = false, requestDriver, copyOverrides = {}, supportedEnvironment, offlineHref, embeddedGuide = false }) {
    const $ = (id) => document.getElementById(id);
    let language = "zh", mode = recoveryEnabled ? "restore" : "backup", session, diagnostics, packageBytes, downloadBytes, summary, busy = false, activeTask, notice = "", error = "", closed = false;
    const copy = Object.fromEntries(["zh", "en"].map((l) => [l, { ...recoveryCopy[l], ...copyOverrides[l] }]));
    const objectUrls = /* @__PURE__ */ new Set();
    const t = (key) => copy[language][key] || key;
    let seenOpened = false;
    const originType = location.protocol === "file:" ? "FILE" : location.protocol === "https:" ? "HTTPS" : "LOOPBACK";
    const supported = supportedEnvironment ?? (simulation || isSecureContext && !!navigator.serial && !/Android|iPhone|iPad|Mobile/i.test(navigator.userAgent));
    const newDiagnostics = () => new Diagnostics({ build, sourceCommit, originType, origin: originType === "FILE" ? "LOCAL_FILE" : location.origin, mode, simulation });
    diagnostics = newDiagnostics();
    diagnostics.check("writeLock", !recoveryEnabled || mode !== "restore");
    function receipt() {
      const data = diagnostics.snapshot(), attempted = data.commands.FLASH_BEGIN.attempted + data.commands.FLASH_DATA.attempted;
      return { schema: "draftor-recovery-receipt/1", diagnostics: data, flashOutcome: attempted ? data.stage === "VERIFIED_BOOT_PENDING" ? "VERIFIED_BOOT_PENDING" : "POSSIBLY_PARTIAL" : "NO_FLASH_WRITE_ATTEMPT", normalBoot: "NOT_VERIFIED_BY_TOOL", tf: "NOT_ACCESSED" };
    }
    function render() {
      document.documentElement.lang = language === "zh" ? "zh-CN" : "en";
      for (const el of document.querySelectorAll("[data-t]")) el.textContent = t(el.dataset.t);
      if (simulation) document.querySelector('[data-t="prepare"]').textContent = language === "zh" ? "\u8FD9\u91CC\u53EA\u8FDE\u63A5\u5408\u6210\u8BBE\u5907\u3002\u4E0B\u65B9\u4E24\u9879\u4EC5\u786E\u8BA4\u865A\u6784\u6D4B\u8BD5\u6761\u4EF6\uFF0C\u4E0D\u8981\u64CD\u4F5C\u771F\u5B9E\u5FAE\u96EA\u6216 TF\u3002" : "Only a synthetic device is connected. The two checks below confirm fictional test conditions. Do not operate your real board or TF.";
      $("build-notice").textContent = t(simulation ? "mock" : recoveryEnabled ? "candidate" : "locked");
      $("build-id").textContent = `${build} \xB7 ${sourceCommit.slice(0, 12)}`;
      $("environment").textContent = t(supported ? "supported" : "unsupported");
      $("mode-restore").hidden = !recoveryEnabled;
      for (const key of ["backup", "restore", "verify"]) {
        $(`mode-${key}`).setAttribute("aria-pressed", String(mode === key));
        $(`mode-${key}`).disabled = busy;
      }
      $("package-section").hidden = mode === "backup";
      $("package-file").disabled = busy;
      $("connect-section").hidden = !!session || closed;
      $("connect").disabled = busy || !supported || !$("board-confirmed").checked || !$("tf-removed").checked || mode !== "backup" && !packageBytes;
      $("backup-section").hidden = mode !== "backup" || !session;
      $("raw-file").disabled = busy || session?.state !== "DEVICE_VERIFIED";
      $("capture").disabled = busy || session?.state !== "DEVICE_VERIFIED";
      $("save-package").disabled = busy || !downloadBytes;
      $("saved-file").disabled = busy || !session || !["DEVICE_VERIFIED", "SAVED_FILE_REIMPORT_REQUIRED"].includes(session.state);
      $("restore-section").hidden = mode !== "restore" || !session || closed;
      $("restore").disabled = busy || !recoveryEnabled || session?.state !== "READY_FOR_CONFIRMATION" || !$("restore-consent").checked;
      $("verify-section").hidden = mode !== "verify" || !session || closed;
      $("verify").disabled = busy || session?.state !== "DEVICE_VERIFIED";
      const stage = diagnostics.snapshot().stage;
      $("status").textContent = t(notice || stage);
      $("error").hidden = !error;
      $("error").textContent = error;
      $("package-summary").textContent = summary ? `${t("selected")} \xB7 ${summary.createdAt} \xB7 ${summary.firmware} \xB7 16 MiB` : "";
      $("saved-result").textContent = session?.state === "BACKUP_SAVED_VERIFIED" ? t("saved") : "";
      $("boot-section").hidden = stage !== "VERIFIED_BOOT_PENDING";
      $("offline-help").textContent = t(originType === "FILE" ? "offlineLocal" : recoveryEnabled ? "offlineHelp" : "offlineLocked");
      $("offline-download").hidden = originType === "FILE";
      document.querySelector('[data-t="guide"]').href = embeddedGuide ? "#offline-guide" : `docs/RECOVERY_GUIDE.${language === "zh" ? "zh-CN" : "en"}.md`;
      if (offlineHref) $("offline-download").href = offlineHref;
      $("diagnostics").textContent = JSON.stringify(receipt(), null, 2);
    }
    function clearPrivateUI() {
      packageBytes?.fill(0);
      downloadBytes?.fill(0);
      packageBytes = downloadBytes = void 0;
      summary = void 0;
      for (const id of ["package-file", "raw-file", "saved-file"]) $(id).value = "";
      for (const id of ["board-confirmed", "tf-removed", "restore-consent"]) $(id).checked = false;
      for (const url of objectUrls) URL.revokeObjectURL(url);
      objectUrls.clear();
      $("package-summary").textContent = "";
      $("saved-result").textContent = "";
    }
    async function closeSession() {
      let failure;
      try {
        await session?.close();
      } catch (e) {
        failure = e;
      } finally {
        session = void 0;
        clearPrivateUI();
        closed = true;
        diagnostics.check("cleanup", { sessionPrivateReferencesCleared: true, domIdentityCleared: true, fileInputsCleared: true, objectUrlsRevoked: true, physicalMemoryErasureGuaranteed: false });
      }
      if (failure) {
        error = t("errorClose");
        throw failure;
      }
    }
    function download(bytes, name, type) {
      const url = URL.createObjectURL(new Blob([bytes], { type }));
      objectUrls.add(url);
      const a = document.createElement("a");
      a.href = url;
      a.download = name;
      document.body.append(a);
      a.click();
      a.remove();
      setTimeout(() => {
        URL.revokeObjectURL(url);
        objectUrls.delete(url);
      }, 3e4);
    }
    async function run(fn, { fileOnly = false } = {}) {
      if (busy) return;
      busy = true;
      notice = "";
      error = "";
      render();
      const ticker = setInterval(render, 300);
      activeTask = (async () => {
        try {
          await fn();
        } catch (e) {
          const code = /^[A-Z_0-9]{1,48}$/.test(e?.code || "") ? e.code : e?.name === "NotFoundError" ? "PORT_SELECTION_CANCELLED" : "IO";
          diagnostics.error({ code });
          const chooserCancelled = code === "PORT_SELECTION_CANCELLED";
          const recoverable = fileOnly && session?.state !== "STOP" || chooserCancelled;
          error = `${t(code === "BACKUP_STALE" && session?.state === "DEVICE_VERIFIED" ? "errorStale" : recoverable ? "errorFile" : "errorStop")} (${code})`;
          if (chooserCancelled) {
            notice = mode === "backup" ? "readyBackup" : "choose";
            error = language === "zh" ? "\u5DF2\u53D6\u6D88\u4E32\u53E3\u9009\u62E9\uFF0C\u5C1A\u672A\u8FDE\u63A5\uFF1B\u53EF\u4EE5\u91CD\u65B0\u9009\u62E9\u3002" : "Port selection cancelled; no connection. You may choose again.";
          }
          if (!recoverable) {
            session?.cancel(code);
            try {
              await closeSession();
            } catch {
            }
            diagnostics.stage("STOP");
          }
          if (receipt().flashOutcome === "POSSIBLY_PARTIAL") error += ` ${t("partial")}`;
        } finally {
          clearInterval(ticker);
          busy = false;
          render();
        }
      })();
      return activeTask;
    }
    async function resetMode(next) {
      if (busy) return;
      await closeSession();
      mode = next;
      diagnostics = newDiagnostics();
      diagnostics.check("writeLock", !recoveryEnabled || mode !== "restore");
      closed = false;
      notice = mode === "backup" ? "readyBackup" : "choose";
      error = "";
      render();
    }
    for (const next of ["backup", "restore", "verify"]) $(`mode-${next}`).addEventListener("click", () => {
      if (next === "restore" && !recoveryEnabled) return;
      resetMode(next).catch(() => render());
    });
    $("language").addEventListener("click", () => {
      language = language === "zh" ? "en" : "zh";
      render();
    });
    for (const id of ["board-confirmed", "tf-removed", "restore-consent"]) $(id).addEventListener("change", render);
    $("package-file").addEventListener("change", () => {
      const file = $("package-file").files[0];
      if (!file) return;
      run(async () => {
        await closeSession();
        diagnostics = newDiagnostics();
        diagnostics.check("writeLock", !recoveryEnabled || mode !== "restore");
        closed = false;
        requireSafe(file.size > 16777216 && file.size <= MAX_PACKAGE_BYTES, "PACKAGE_SIZE");
        const bytes = new Uint8Array(await file.arrayBuffer());
        let handle;
        try {
          handle = await validateRecoveryPackage(bytes);
          summary = recoverySummary(handle);
          packageBytes = bytes;
          diagnostics.check("packageValidated", true);
          diagnostics.stage("PACKAGE_VALIDATED");
        } catch (e) {
          bytes.fill(0);
          throw e;
        } finally {
          destroyRecoveryPackage(handle);
          $("package-file").value = "";
        }
      }, { fileOnly: true });
    });
    $("connect").addEventListener("click", () => {
      if (busy || $("connect").disabled) return;
      const pending = requestDriver(mode, diagnostics);
      run(async () => {
        const driver = await pending;
        session = new RecoverySession(driver, { mode, diagnostics, build, sourceCommit });
        if (mode !== "backup") await session.selectPackage(packageBytes);
        try {
          await session.prepare({ boardConfirmed: $("board-confirmed").checked, tfRemoved: $("tf-removed").checked });
        } finally {
          if (diagnostics.snapshot().events.openSuccess > 0) {
            if (seenOpened) diagnostics.event("manualReconnects");
            seenOpened = true;
          }
        }
        packageBytes?.fill(0);
        packageBytes = void 0;
      });
    });
    $("raw-file").addEventListener("change", () => {
      const file = $("raw-file").files[0];
      if (!file) return;
      run(async () => {
        let b;
        try {
          requireSafe(file.size === 16777216, "PACKAGE_SIZE");
          b = new Uint8Array(await file.arrayBuffer());
          downloadBytes = await session.useExistingRaw(b);
        } finally {
          b?.fill(0);
          $("raw-file").value = "";
        }
      }, { fileOnly: true });
    });
    const progress = (value) => {
      $("progress").hidden = false;
      $("progress").value = value;
    };
    $("capture").addEventListener("click", () => run(async () => {
      downloadBytes = await session.capture({ progress });
    }));
    $("save-package").addEventListener("click", () => {
      if (!downloadBytes || busy) return;
      const name = `Draftor-${simulation ? "SYNTHETIC-TEST" : "private"}-recovery-${(/* @__PURE__ */ new Date()).toISOString().replace(/[:.]/g, "-")}-${crypto.randomUUID().slice(0, 8)}.zip`;
      if (typeof window.showSaveFilePicker !== "function") {
        download(downloadBytes, name, "application/zip");
        return;
      }
      const chosen = window.showSaveFilePicker({ suggestedName: name, types: [{ description: "Draftor private recovery ZIP", accept: { "application/zip": [".zip"] } }] });
      run(async () => {
        let writable;
        try {
          const handle = await chosen;
          writable = await handle.createWritable();
          await writable.write(downloadBytes);
          await writable.close();
          writable = void 0;
        } catch (e) {
          if (e?.name === "AbortError") {
            notice = "SAVED_FILE_REIMPORT_REQUIRED";
            return;
          }
          throw e;
        } finally {
          if (writable) await writable.abort().catch(() => {
          });
        }
      }, { fileOnly: true });
    });
    $("saved-file").addEventListener("change", () => {
      const file = $("saved-file").files[0];
      if (!file) return;
      run(async () => {
        let b;
        try {
          requireSafe(file.size > 16777216 && file.size <= MAX_PACKAGE_BYTES, "PACKAGE_SIZE");
          b = new Uint8Array(await file.arrayBuffer());
          await session.verifySavedPackage(b);
        } finally {
          b?.fill(0);
          $("saved-file").value = "";
        }
      }, { fileOnly: true });
    });
    $("restore").addEventListener("click", () => {
      if ($("restore").disabled) return;
      run(() => session.restore({ restoreConfirmed: $("restore-consent").checked, progress }));
    });
    $("verify").addEventListener("click", () => {
      if ($("verify").disabled) return;
      run(() => session.verifyAgain());
    });
    $("close").addEventListener("click", async () => {
      if (busy) {
        session?.cancel();
        await activeTask;
      }
      try {
        await closeSession();
        notice = "closed";
      } catch {
        notice = "STOP";
      }
      render();
    });
    $("save-receipt").addEventListener("click", () => {
      const b = new TextEncoder().encode(JSON.stringify(receipt(), null, 2) + "\n");
      download(b, "Draftor-R9-sanitized-receipt.json", "application/json");
    });
    addEventListener("beforeunload", (e) => {
      if (busy || session) {
        e.preventDefault();
        e.returnValue = "";
      }
    });
    addEventListener("pagehide", () => {
      session?.cancel();
      session?.close().catch(() => {
      });
      clearPrivateUI();
    });
    notice = mode === "backup" ? "readyBackup" : "choose";
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

  // src/first-install.js
  var FIRST_FILES = Object.freeze(["bootloader.bin", "partitions.bin", "boot_app0.bin", "app.bin"]);
  async function verifyInitialFlash(driver, source) {
    requireSafe(["blank", "waveshare-factory-20260120"].includes(source), "UNKNOWN");
    if (source === "blank") {
      requireSafe(await driver.flashMd5(0, PINS.flashBytes) === PINS.blankFlashMd5, "UNKNOWN");
    } else {
      for (const row of FACTORY.fingerprint)
        requireSafe(await driver.flashMd5(row.offset, row.bytes) === row.md5, "FACTORY_FINGERPRINT");
    }
  }

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
    async grantExclusiveMode({ mode, initialState, expectedMac, metadataFingerprint, backupMd5 }) {
      this.#alive();
      requireSafe(this.#candidate && this.#connected && this.#candidateMode === void 0 && !this.#writesAllowed && !this.#firstAllowed && !this.#writeUsed, "BUILD_SCOPE");
      this.#candidateMode = "RESERVING";
      try {
        requireSafe(["update", "fresh"].includes(mode), "BUILD_SCOPE");
        requireSafe(normalizeMac((await this.identity()).mac) === normalizeMac(expectedMac), "IDENTITY");
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
          requireSafe(initialState === "waveshare-factory-20260120", "UNQUALIFIED_BLANK");
          await verifyInitialFlash(this, initialState);
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
    async eraseAll({ source, expectedMac, backupConfirmed, tfRemoved, planConfirmed, eraseConfirmed } = {}) {
      this.#range(0, PINS.flashBytes);
      requireSafe(this.#firstAllowed && !this.#writesAllowed, "FRESH_DEVICE_PENDING");
      requireSafe(!this.#candidate || this.#candidateMode === "fresh" && source === "waveshare-factory-20260120", "UNKNOWN");
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
      this.#clearRecovery();
      this.#wire.cancel();
    }
    async disconnect() {
      this.#fatal = true;
      this.#clearRecovery();
      await this.#wire.disconnect();
    }
  };

  // src/read-only-driver.js
  var ReadOnlyDriver = class {
    #rom;
    #diagnostics;
    kind = "hardware";
    constructor(transport, { diagnostics } = {}) {
      this.#diagnostics = diagnostics;
      this.#rom = new S3Rom(transport, { diagnostics });
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
    verifyRecoveryImage(handle) {
      return this.#rom.verifyRecoveryImage(handle);
    }
    #locked() {
      this.#diagnostics?.event("blockedWriteRequests");
      throw new Stop("DEVICE_WRITES_LOCKED");
    }
    writeImage() {
      this.#locked();
    }
    eraseAll() {
      this.#locked();
    }
    grantRecovery() {
      this.#locked();
    }
    restoreRecovery() {
      this.#locked();
    }
    cancel() {
      this.#rom.cancel();
    }
    disconnect() {
      return this.#rom.disconnect();
    }
  };

  // src/release-platform.js
  function qualifiedDesktopChrome(environment) {
    const { userAgent = "", vendor = "", platform = "", brands, secureContext = false, serialAvailable = false, brave = false } = environment;
    const mac = /Macintosh/.test(userAgent) && (/Mac/.test(platform) || platform === "macOS");
    const chrome = /Chrome\/\d+/.test(userAgent) && vendor === "Google Inc." && !brave && !/(Edg|OPR|Opera|Android|Mobile|CriOS|FxiOS)/.test(userAgent);
    const brandOk = !brands || brands.some((x) => x.brand === "Google Chrome");
    return Boolean(mac && chrome && brandOk && secureContext && serialAvailable);
  }
  function browserIsQualified() {
    return qualifiedDesktopChrome({
      userAgent: navigator.userAgent,
      vendor: navigator.vendor,
      platform: navigator.userAgentData?.platform || navigator.platform,
      brands: navigator.userAgentData?.brands,
      secureContext: isSecureContext,
      serialAvailable: !!navigator.serial?.requestPort,
      brave: !!navigator.brave
    });
  }

  // src/r10-copy.js
  var publicRecoveryCopy = { zh: { locked: "\u5728\u7EBF\u5907\u4EFD\u4E0E\u53EA\u8BFB\u6838\u9A8C\uFF1A\u6B64\u9875\u4E0D\u6267\u884C\u6574\u7247\u6062\u590D\u3002\u8BF7\u4E0B\u8F7D\u79BB\u7EBF\u5DE5\u5177\uFF0C\u5728\u539F\u8BBE\u5907\u4E0A\u6062\u590D\u5DF2\u4FDD\u5B58\u7684\u79C1\u4EBA\u5305\u3002", candidate: "\u79BB\u7EBF\u6062\u590D\u5DE5\u5177 \xB7 \u9650 macOS \u684C\u9762 Chrome \u4E0E\u539F\u5FAE\u96EA ST7305\u3002\u6BCF\u6B21\u6062\u590D\u987B\u91CD\u65B0\u6838\u9A8C\u539F\u8BBE\u5907\u3001\u5B8C\u6574\u5305\u53CA\u8986\u76D6\u8303\u56F4\u3002", offlineLocked: "\u4E0B\u8F7D\u5177\u6709\u5B8C\u6574\u6062\u590D\u80FD\u529B\u7684\u79BB\u7EBF HTML\uFF0C\u4E0E\u79C1\u4EBA\u6062\u590D ZIP \u4E00\u8D77\u4FDD\u5B58\u3002\u5173\u95ED\u7F51\u9875\u540E\u5728 macOS Chrome \u7528\u201C\u6253\u5F00\u6587\u4EF6\u201D\u91CD\u5F00 HTML\uFF1B\u65E0\u9700\u7F51\u7EDC\u3001Python \u6216\u6D4F\u89C8\u5668\u6269\u5C55\u3002", supported: "macOS \u684C\u9762 Chrome \u73AF\u5883\u53EF\u7528\u3002\u5B9E\u6D4B\u57FA\u7EBF\u548C\u9650\u5B9A\u8303\u56F4\u89C1\u8BF4\u660E\uFF1B\u672A\u8FDE\u63A5\u524D\u4E0D\u4F1A\u8BBF\u95EE\u8BBE\u5907\u3002", unsupported: "\u8BBE\u5907\u64CD\u4F5C\u4EC5\u9762\u5411 macOS \u684C\u9762 Google Chrome \u7684\u5B89\u5168 Web Serial \u73AF\u5883\uFF1B\u4ECD\u53EF\u9605\u8BFB\u548C\u4E0B\u8F7D\u3002\u5176\u4ED6\u5E73\u53F0\u672A\u7EB3\u5165\u5DF2\u9A8C\u6536\u8303\u56F4\u3002" }, en: { locked: "Online backup and read-only verification. This page does not restore Flash. Download the offline tool to restore a saved private package to its original board.", candidate: "Offline recovery tool \xB7 macOS desktop Chrome and original Waveshare ST7305 only. Every restore rechecks the board, complete package and overwrite scope.", offlineLocked: "Download the writable offline HTML and keep it with the private ZIP. Close the page and reopen the HTML using Open File in macOS Chrome. No network, Python or extension required.", supported: "macOS desktop Chrome is available. See the guide for the tested baseline and limited scope. No device access before connection.", unsupported: "Device operations require macOS desktop Google Chrome with secure Web Serial. Reading and downloads remain available. Other platforms are unqualified." } };

  // src/r10-online-recovery.js
  var offline = false;
  startRecoveryUI({
    build: "R10-public-52f5f8203326",
    sourceCommit: "52f5f8203326911d5f4de39b6360d013ed404b49",
    recoveryEnabled: offline,
    copyOverrides: publicRecoveryCopy,
    supportedEnvironment: browserIsQualified(),
    embeddedGuide: offline,
    offlineHref: "offline/Draftor_Recovery_R10_Offline.html",
    requestDriver: async (mode, diagnostics) => {
      requireSafe(browserIsQualified(), "PLATFORM");
      const wire = new StrictSerialTransport(await requestSerialPort(navigator.serial, diagnostics), { diagnostics });
      return new ReadOnlyDriver(wire, { diagnostics });
    }
  });
})();
//# sourceMappingURL=recovery.js.map
