'use strict';

// import { Inflate, constants, Deflate } from './pako.js';
const pako = require('./pako.js')
const log = require('electron-log');
const crypto = require('crypto');
const EC = require('elliptic').ec;

Object.assign(console, log.functions);

module.exports.createGlogReader = (bytes, key) => new GlogReader(bytes, key);

const GlogRecoveryVersion = 0x3;
const GlogCipherVersion = 0x4;

const COMPRESS_NONE_V3 = 0;
const COMPRESS_ZLIB_V3 = 1;

const ENCRYPT_NONE_V3 = 0;
const ENCRYPT_AES_V3 = 1;

const COMPRESS_NONE_V4 = 1;
const COMPRESS_ZLIB_V4 = 2;

const ENCRYPT_NONE_V4 = 1;
const ENCRYPT_AES_V4 = 2;

const MAGIC_NUMBER = new Uint8Array([0x1B, 0xAD, 0xC0, 0xDE]);

const SINGLE_LOG_CONTENT_MAX_LENGTH = 16 * 1024;

// log length width = 2 bytes
const LOG_LENGTH_BYTES = 2;
// sync marker
const SYNC_MARKER = new Uint8Array([0xB7, 0xDB, 0xE7, 0xDB, 0x80, 0xAD, 0xD9, 0x57]);

class FileReader {
    constructor(content, position) {
        this.content = content;
        this.position = position;
        this.size = content.length;
        this.inflate = pako.createInflate({ level: 9, raw: true, windowBits: -15, chunkSize: 16 * 1024 });
        this.partialMatchTable = computePartialMatchTable(SYNC_MARKER);
    }

    read() { throw "Should implement"; }

    readRemainHeader() { throw "Should implement" }

    spaceRemain() {
        if (this.size <= this.position) {
            return 0;
        }
        return this.size - this.position;
    }

    readU16Le() {
        if (this.spaceRemain() < 2) {
            throw "eof error";
        }
        let ret = this.content.readUInt16LE(this.position);
        this.position += 2;
        return ret;
    }

    decompress(data) {
        // const inflate = new pako.Inflate({ level: 9, raw: true, windowBits: -15 });
        let ret = null;

        this.inflate.onData = (chunk) => {
            // console.warn('inflate onData', chunk);
            ret = chunk;
        };

        // this.inflate.onEnd = (chunk) => {
        //   console.log('inflate onEnd', chunk);
        // };
        let success = this.inflate.push(data, pako.constants.Z_SYNC_FLUSH);
        if (this.inflate.err) {
            console.error("decompress error", this.inflate.err);
            return null;
        }
        return success ? ret : null;
    }

    compress(data) {
        const deflate = new pako.Deflate({ level: 9, raw: true, windowBits: -15, memLevel: 9, strategy: 0 });
        let ret = null;

        deflate.onData = (chunk) => {
            console.log('deflate onData', chunk);
            ret = chunk;
        };

        // deflate.onEnd = (chunk) => {
        //   console.log('deflate onEnd', chunk);
        // };
        let success = deflate.push(data, pako.constants.Z_SYNC_FLUSH); // Z_SYNC_FLUSH


        // console.log("result", deflate.result);
        return success ? ret : null;
    }

    tryRecover(errCode) {
        console.warn("file broken at position:%d, errCode:%d", this.position, errCode);
        let sync = this.searchSyncMarker(this.position);
        if (sync == -1) {
            console.error("fail to recover");
            return [errCode, null];
        } else {
            console.log("find sync:%d", sync + this.position);
            this.position += sync + SYNC_MARKER.length;
            return [0, null];
        }
    }

    searchSyncMarker(start) {
        return searchNeedle(this.content.slice(start), SYNC_MARKER, this.partialMatchTable);
    }
}

class FileReaderV3 extends FileReader {

    constructor(content, position) {
        super(content, position);
        this.compressMode = COMPRESS_NONE_V3;
        this.encryptMode = ENCRYPT_NONE_V3;
    }

    readRemainHeader() {
        if (this.spaceRemain() < 1) {
            throw "Too small header 1"
        }
        switch (this.content[this.position] >> 4) {
            case COMPRESS_NONE_V3:
                this.compressMode = COMPRESS_NONE_V3;
                break;
            case COMPRESS_ZLIB_V3:
                this.compressMode = COMPRESS_ZLIB_V3;
                break;
            default:
                throw "Illegal compress mode";
        }
        switch (this.content[this.position] & 0x0F) {
            case ENCRYPT_NONE_V3:
                this.encryptMode = ENCRYPT_NONE_V3;
                break;
            case ENCRYPT_AES_V3:
                this.encryptMode = ENCRYPT_AES_V3;
                break;
            default:
                throw "Illegal encrypt mode";
        }
        console.log("encryptMode:%s, compressMode:%s", this.encryptMode, this.compressMode);

        this.position++;

        const protoNameLen = this.readU16Le();

        if (this.spaceRemain() < protoNameLen + 8) {
            throw "Too small header";
        }

        let protoName = new TextDecoder().decode(this.content.slice(4 + 1 + 1 + 2, 4 + 1 + 1 + 2 + protoNameLen));
        console.log("protoName:", protoName);

        this.position += protoNameLen;

        for (let i = 0; i < SYNC_MARKER.length; ++i, this.position++) {
            if (this.content[this.position] != SYNC_MARKER[i]) {
                throw "Sync marker mismatch";
            }
        }

        console.log("Read header v3 done, position:", this.position);
    }

    read() {
        if (this.spaceRemain() < this.logStoreSize(1)) {
            return [-1, null];
        }
        let logLength = this.readU16Le();

        if (this.spaceRemain() < logLength + 8) {
            throw "No enough space for length:" + logLength + " at position:" + position;
        }

        if (logLength == 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
            return this.tryRecover(-2);
        }

        let data = this.content.slice(this.position, this.position + logLength);
        this.position += logLength;

        if (this.compressMode == COMPRESS_ZLIB_V3) {
            if ((data = this.decompress(data)) == null) {
                return this.tryRecover(-3);
            }
            logLength = data.length;
        }

        let syncMarkerMatch = true;
        for (let i = 0; i < SYNC_MARKER.length; ++i, ++this.position) {
            if (this.content[this.position] != SYNC_MARKER[i]) {
                syncMarkerMatch = false;
                break;
            }
        }

        if (!syncMarkerMatch) {
            return this.tryRecover(-4);
        }
        return [logLength, data];
    }

    logStoreSize(len) {
        return 2 + len + 8;
    }
}

class FileReaderV4 extends FileReader {

    constructor(content, position, svrPriKey) {
        super(content, position);
        this.svrPriKey = svrPriKey;
        this.ec = new EC('secp256k1');
    }

    readRemainHeader() {
        const protoNameLen = this.readU16Le();
        if (this.spaceRemain() < protoNameLen + 8) {
            throw "Too small header";
        }

        let protoName = new TextDecoder().decode(this.content.slice(4 + 1 + 2, 4 + 1 + 2 + protoNameLen));
        console.log("protoName:", protoName);

        this.position += protoNameLen;

        for (let i = 0; i < SYNC_MARKER.length; ++i, this.position++) {
            if (this.content[this.position] != SYNC_MARKER[i]) {
                throw "Sync marker mismatch";
            }
        }
        console.log("Read header v4 done, position:", this.position);
    }

    read() {
        if (this.spaceRemain() < 2 + 1 + 8) {
            return [-1, null];
        }
        let compressMode = 0;
        let encryptMode = 0;

        switch (this.content[this.position] >> 4) {
            case COMPRESS_NONE_V4:
                compressMode = COMPRESS_NONE_V4;
                break;
            case COMPRESS_ZLIB_V4:
                compressMode = COMPRESS_ZLIB_V4;
                break;
            default:
                console.warn("Illegal compress mode:", this.content[this.position] >> 4);
                this.position++;
                return this.tryRecover(-2);
        }
        switch (this.content[this.position] & 0x0F) {
            case ENCRYPT_NONE_V4:
                encryptMode = ENCRYPT_NONE_V4;
                break;
            case ENCRYPT_AES_V4:
                encryptMode = ENCRYPT_AES_V4;
                break;
            default:
                console.warn("Illegal encrypt mode:", this.content[this.position] & 0x0F);
                this.position++;
                return this.tryRecover(-3);
        }
        // console.log("encryptMode:%s, compressMode:%s", encryptMode, compressMode);

        this.position++;

        if (encryptMode == ENCRYPT_AES_V4 && this.svrPriKey == null) {
            throw "should set server private key";
        }

        let logLength = 0;
        let data = null;

        if (encryptMode == ENCRYPT_AES_V4) {
            if (this.spaceRemain() < 16 + 64) {
                throw "No enough space for iv and client pub key";
            }

            let iv = this.content.slice(this.position, this.position += 16);
            let clientPubKey = this.content.slice(this.position, this.position += 64);
            logLength = this.readU16Le();

            if (logLength == 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
                return this.tryRecover(-4);
            }

            if (this.spaceRemain() < logLength + 8) {
                throw "No enough space for length:" + logLength + " at position:" + position;
            }

            let encrypted = this.content.slice(this.position, this.position += logLength);
            data = this.decrypt(clientPubKey, iv, encrypted);

            if (compressMode == COMPRESS_ZLIB_V4) {
                if ((data = this.decompress(data)) == null) {
                    return this.tryRecover(-5);
                }
            }
            logLength = data.length;
        } else {
            logLength = this.readU16Le();

            if (logLength == 0 || logLength > SINGLE_LOG_CONTENT_MAX_LENGTH) {
                return this.tryRecover(-6);
            }

            if (this.spaceRemain() < logLength + 8) {
                throw "No enough space for length:" + logLength + " at position:" + position;
            }

            data = this.content.slice(this.position, this.position += logLength);
            if (compressMode == COMPRESS_ZLIB_V4) {
                if ((data = this.decompress(data)) == null) {
                    return this.tryRecover(-7);
                }
            }
            logLength = data.length;
        }

        let syncMarkerMatch = true;
        for (let i = 0; i < SYNC_MARKER.length; ++i, ++this.position) {
            if (this.content[this.position] != SYNC_MARKER[i]) {
                syncMarkerMatch = false;
                break;
            }
        }

        if (!syncMarkerMatch) {
            return this.tryRecover(-8);
        }
        return [logLength, data];
    }

    logStoreSize(len, cipher) {
        return 1 + 2 + len + 8 + (cipher ? 16 + 64 : 0);
    }

    decrypt(clientPubKey, iv, encrypted) {
        let ecPubKey = this.ec.keyFromPublic({ x: clientPubKey.slice(0, 32), y: clientPubKey.slice(32, 64) }).getPublic();
        let ecPriKey = this.ec.keyFromPrivate(this.svrPriKey, 'hex').getPrivate();
        let sharedKey = ecPubKey.mul(ecPriKey).getX().toBuffer().slice(0, 16);

        const decipher = crypto.createDecipheriv('aes-128-cfb', sharedKey, iv);
        let decrypted = Buffer.concat([decipher.update(encrypted), decipher.final()]);
        return decrypted;
    }

    // Convert a hex string to a byte array
    hexToBytes(hex) {
        for (var bytes = [], c = 0; c < hex.length; c += 2)
            bytes.push(parseInt(hex.substr(c, 2), 16));
        return bytes;
    }

    // Convert a byte array to a hex string
    bytesToHex(bytes) {
        for (var hex = [], i = 0; i < bytes.length; i++) {
            var current = bytes[i] < 0 ? bytes[i] + 256 : bytes[i];
            hex.push((current >>> 4).toString(16));
            hex.push((current & 0xF).toString(16));
        }
        return hex.join("");
    }
}

class GlogReader {
    constructor(content, key) {
        console.log('GlogReader open file, size:', content.length);
        this.fileReader = null;
        this.readHeader(content, key);
    }

    readHeader(content, key) {
        if (content.length < MAGIC_NUMBER.length + 1) {
            throw "Too small header";
        }
        let cur = 0;
        while (cur < MAGIC_NUMBER.length) {
            if (content[cur] != MAGIC_NUMBER[cur]) {
                throw "Magic number mismatch";
            }
            cur++;
        }

        switch (content[cur++]) {
            case GlogRecoveryVersion:
                this.fileReader = new FileReaderV3(content, cur);
                break;
            case GlogCipherVersion:
                this.fileReader = new FileReaderV4(content, cur, key);
                break;
            default:
                throw "Version code mismatch";
        }
        this.fileReader.readRemainHeader();
    }

    spaceRemain() {
        if (this.size <= this.position) {
            return 0;
        }
        return this.size - this.position;
    }

    read() {
        return this.fileReader.read();
    }
}

/**
 * kmp search needle's position in haystack.
 * 
 * @param {string} haystack
 * @param {string} needle
 * @return {number}
 */
var searchNeedle = function (haystack, needle, table) {
    if (needle.length == 0) {
        return 0;
    }
    // let table = computePartialMatchTable(needle);
    let i = 0, j = 0;
    while (i < haystack.length && j < needle.length) {
        if (j == -1 || haystack[i] == needle[j]) {
            i++;
            j++;
        } else {
            j = table[j];
        }
    }
    if (j == needle.length) {
        return i - j;
    }
    return -1;
};

function computePartialMatchTable(needle) {
    let table = [];
    table[0] = -1;
    let i = 0, j = -1;
    const size = needle.length;
    while (i < size - 1) {
        if (j == -1 || needle[i] == needle[j]) {
            i++;
            j++;
            if (needle[i] != needle[j]) {
                table[i] = j;
            } else {
                table[i] = table[j];
            }
        } else {
            j = table[j];
        }
    }
    return table;
}