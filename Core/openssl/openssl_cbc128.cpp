/*
 * Copyright 2008-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "openssl_aes.h"
#include <cstring>

namespace openssl {

void AES_cbc128_encrypt(const uint8_t *in, uint8_t *out, size_t len, const AES_KEY *key, uint8_t ivec[16]) {
    size_t n;
    const uint8_t *iv = ivec;

    while (len >= 16) {
        for (n = 0; n < 16; n += sizeof(size_t))
            *(size_t *) (out + n) = *(size_t *) (in + n) ^ *(size_t *) (iv + n);
        AES_encrypt(out, out, key);
        iv = out;
        len -= 16;
        in += 16;
        out += 16;
    }
    while (len) {
        for (n = 0; n < 16 && n < len; ++n)
            out[n] = in[n] ^ iv[n];
        for (; n < 16; ++n)
            out[n] = iv[n];
        AES_encrypt(out, out, key);
        iv = out;
        if (len <= 16)
            break;
        len -= 16;
        in += 16;
        out += 16;
    }
    if (ivec != iv)
        memcpy(ivec, iv, 16);
}

void AES_cbc128_decrypt(const uint8_t *in, uint8_t *out, size_t len, const AES_KEY *key, uint8_t ivec[16]) {
    size_t n;
    union {
        size_t t[16 / sizeof(size_t)];
        uint8_t c[16];
    } tmp;

    if (in != out) {
        const uint8_t *iv = ivec;

        while (len >= 16) {
            auto *out_t = (size_t *) out;
            auto *iv_t = (size_t *) iv;

            AES_decrypt(in, out, key);

            for (n = 0; n < 16 / sizeof(size_t); n++)
                out_t[n] ^= iv_t[n];
            iv = in;
            len -= 16;
            in += 16;
            out += 16;
        }
        if (ivec != iv)
            memcpy(ivec, iv, 16);
    } else {
        while (len >= 16) {
            size_t c;
            auto *out_t = (size_t *) out;
            auto *ivec_t = (size_t *) ivec;
            const auto *in_t = (const size_t *) in;

            AES_decrypt(in, tmp.c, key);
            for (n = 0; n < 16 / sizeof(size_t); n++) {
                c = in_t[n];
                out_t[n] = tmp.t[n] ^ ivec_t[n];
                ivec_t[n] = c;
            }
            len -= 16;
            in += 16;
            out += 16;
        }
    }
    while (len) {
        uint8_t c;
        AES_decrypt(in, tmp.c, key);
        for (n = 0; n < 16 && n < len; ++n) {
            c = in[n];
            out[n] = tmp.c[n] ^ ivec[n];
            ivec[n] = c;
        }
        if (len <= 16) {
            for (; n < 16; ++n)
                ivec[n] = in[n];
            break;
        }
        len -= 16;
        in += 16;
        out += 16;
    }
}
} // namespace openssl