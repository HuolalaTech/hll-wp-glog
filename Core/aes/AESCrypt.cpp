/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2018 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AESCrypt.h"
#include "../openssl/openssl_aes.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "utilities.h"

using namespace openssl;

namespace glog {

AESCrypt::AESCrypt(const void *key, size_t keyLength, const void *iv, size_t ivLength) {
    if (key && keyLength > 0) {
        memcpy(m_key, key, (keyLength > AES_KEY_LEN) ? AES_KEY_LEN : keyLength);

        resetIV(iv, ivLength);

        m_aesKey = new AES_KEY;
        memset(m_aesKey, 0, sizeof(AES_KEY));

        int ret = AES_set_encrypt_key(m_key, AES_KEY_BITSET_LEN, m_aesKey);
        assert(ret == 0);
    }
}

AESCrypt::AESCrypt(const AESCrypt &other, const AESCryptStatus &status) : m_isClone(true), m_number(status.m_number) {
    //memcpy(m_key, other.m_key, sizeof(m_key));
    memcpy(m_vector, status.m_vector, sizeof(m_vector));
    m_aesKey = other.m_aesKey;
}

AESCrypt::~AESCrypt() {
    if (!m_isClone) {
        delete m_aesKey;
    }
}

void AESCrypt::resetIV(const void *iv, size_t ivLength) {
    m_number = 0;
    if (iv && ivLength > 0) {
        memcpy(m_vector, iv, (ivLength > AES_KEY_LEN) ? AES_KEY_LEN : ivLength);
    } else {
        memcpy(m_vector, m_key, AES_KEY_LEN);
    }
}

void AESCrypt::resetStatus(const AESCryptStatus &status) {
    m_number = status.m_number;
    memcpy(m_vector, status.m_vector, AES_KEY_LEN);
}

void AESCrypt::getKey(void *output) const {
    if (output) {
        memcpy(output, m_key, AES_KEY_LEN);
    }
}

void AESCrypt::encrypt(const void *input, void *output, size_t length) {
    if (!input || !output || length == 0) {
        return;
    }
    AES_cfb128_encrypt((const uint8_t *) input, (uint8_t *) output, length, m_aesKey, m_vector, &m_number);
}

void AESCrypt::decrypt(const void *input, void *output, size_t length) {
    if (!input || !output || length == 0) {
        return;
    }
    AES_cfb128_decrypt((const uint8_t *) input, (uint8_t *) output, length, m_aesKey, m_vector, &m_number);
}

void AESCrypt::encryptOnce(const void *input, void *output, size_t length, const AES_KEY *key, uint8_t *outIV) {
    AESCrypt::fillRandomIV(outIV);
    uint8_t iv[AES_KEY_LEN] = {};
    memcpy(iv, outIV, AES_KEY_LEN);
    uint32_t num = 0;
    AES_cfb128_encrypt((const uint8_t *) input, (uint8_t *) output, length, key, iv, &num);
}

void AESCrypt::decryptOnce(const void *input, void *output, size_t length, const openssl::AES_KEY *key, uint8_t *iv) {
    uint32_t num = 0;
    AES_cfb128_decrypt((const uint8_t *) input, (uint8_t *) output, length, key, iv, &num);
}

void AESCrypt::initRandomSeed() {
    srand((unsigned) cycleClockNow());
}

void AESCrypt::fillRandomIV(void *vector) {
    if (!vector) {
        return;
    }
    //    srand((unsigned) time(nullptr));
    int *ptr = (int *) vector;
    for (uint32_t i = 0; i < AES_KEY_LEN / sizeof(int); i++) {
        ptr[i] = rand();
    }
}

void AESCrypt::getCurStatus(AESCryptStatus &status) {
    status.m_number = static_cast<uint8_t>(m_number);
    memcpy(status.m_vector, m_vector, sizeof(m_vector));
}

AESCrypt AESCrypt::cloneWithStatus(const AESCryptStatus &status) const {
    return AESCrypt(*this, status);
}

} // namespace glog