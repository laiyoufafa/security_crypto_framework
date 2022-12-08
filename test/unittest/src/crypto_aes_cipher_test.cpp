/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include "securec.h"

#include "aes_openssl.h"
#include "blob.h"
#include "cipher.h"
#include "detailed_iv_params.h"
#include "detailed_gcm_params.h"
#include "detailed_ccm_params.h"
#include "log.h"
#include "memory.h"
#include "sym_common_defines.h"
#include "sym_key_generator.h"

using namespace std;
using namespace testing::ext;

namespace {
const int32_t FILE_BLOCK_SIZE = 1024;
const int32_t RAND_MAX_NUM = 100;
const bool IS_DEBUG = false;
constexpr int32_t CIPHER_TEXT_LEN = 128;
constexpr int32_t KEY_MATERIAL_LEN = 16;
constexpr int32_t AES_IV_LEN = 16;   // iv for CBC|CTR|OFB|CFB mode
constexpr int32_t GCM_IV_LEN = 12;   // GCM
constexpr int32_t GCM_AAD_LEN = 8;
constexpr int32_t GCM_TAG_LEN = 16;
constexpr int32_t CCM_IV_LEN = 7;    // CCM
constexpr int32_t CCM_AAD_LEN = 8;
constexpr int32_t CCM_TAG_LEN = 12;
constexpr int32_t PLAINTEXT_LEN = 13;
constexpr int32_t AES_KEY_SIZE = 128;

class CryptoAesCipherTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void CryptoAesCipherTest::SetUpTestCase() {}

void CryptoAesCipherTest::TearDownTestCase() {}

void CryptoAesCipherTest::SetUp() // add init here, this will be called before test.
{
}

void CryptoAesCipherTest::TearDown() // add destroy here, this will be called when test case done.
{
}

static void PrintfHex(const char *tag, uint8_t *in, int inLen)
{
    if (!IS_DEBUG) {
        return;
    }
    printf("%s:\n", tag);
    for (int i = 0; i < inLen; i++) {
        printf("%02hhX", in[i]);
    }
    printf("\n");
}

static int32_t GenerateSymKey(const char *algoName, HcfSymKey **key)
{
    HcfSymKeyGenerator *generator = nullptr;

    int32_t ret = HcfSymKeyGeneratorCreate(algoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        return ret;
    }

    ret = generator->generateSymKey(generator, key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }
    HcfObjDestroy((HcfObjectBase *)generator);
    return ret;
}

static int32_t ConvertSymKey(const char *algoName, HcfSymKey **key)
{
    HcfSymKeyGenerator *generator = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = {.data = (uint8_t *)keyMaterial, .len = 16};

    int32_t ret = HcfSymKeyGeneratorCreate(algoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        return ret;
    }

    ret = generator->convertSymKey(generator, &keyTmpBlob, key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }
    PrintfHex("keybinary", keyTmpBlob.data, keyTmpBlob.len);
    HcfObjDestroy((HcfObjectBase *)generator);
    return ret;
}

/* just rand data fill file for test */
static int32_t GeneratorFile(const char *fileName, int32_t genFileSize)
{
    if (genFileSize == 0) {
        return 0;
    }
    uint8_t buffer[FILE_BLOCK_SIZE] = {0};
    std::ifstream file(fileName);

    if (file.good()) {
        file.close();
        return 0;
    }
    ofstream outfile(fileName, ios::out|ios::binary|ios::app);
    if (outfile.is_open()) {
        while (genFileSize) {
            for (uint32_t i = 0; i < FILE_BLOCK_SIZE; i++) {
                buffer[i] = (rand() % RAND_MAX_NUM) + 1;
            }
            genFileSize -= FILE_BLOCK_SIZE;
            outfile.write(reinterpret_cast<const char *>(buffer), FILE_BLOCK_SIZE);
        }
        outfile.close();
    }
    return 0;
}

static int32_t CompareFileContent()
{
    int32_t ret = -1;
    ifstream infile1, infile2;
    infile1.open("/data/test_aes.txt", ios::in|ios::binary);
    infile1.seekg (0, infile1.end);
    uint32_t length1 = infile1.tellg();
    infile1.seekg (0, infile1.beg);

    infile2.open("/data/test_aes_new.txt", ios::in|ios::binary);
    infile2.seekg (0, infile2.end);
    uint32_t length2 = infile2.tellg();
    infile2.seekg (0, infile2.beg);
    if (length1 != length2) {
        return ret;
    }
    uint8_t buffer1[FILE_BLOCK_SIZE] = {0};
    uint8_t buffer2[FILE_BLOCK_SIZE] = {0};
    for (uint32_t i = 0; i < length1 / FILE_BLOCK_SIZE; i++) {
        infile1.read(reinterpret_cast<char *>(buffer1), FILE_BLOCK_SIZE);
        infile2.read(reinterpret_cast<char *>(buffer2), FILE_BLOCK_SIZE);
        ret = memcmp(buffer1, buffer2, FILE_BLOCK_SIZE);
        if (ret != 0) {
            goto CLEAR_UP;
        }
    }
CLEAR_UP:
    infile1.close();
    infile2.close();
    return ret;
}

static int32_t AesMultiBlockEncrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params)
{
    HcfBlob output = {};
    ifstream infile;
    ofstream outfile;
    infile.open("/data/test_aes.txt", ios::in|ios::binary);
    infile.seekg (0, infile.end);
    uint32_t length = infile.tellg();
    infile.seekg (0, infile.beg);
    uint8_t buffer[1024] = {0};
    outfile.open("/data/test_aes_enc.txt", ios::out|ios::binary);
    HcfBlob input = {.data = (uint8_t *)buffer, .len = FILE_BLOCK_SIZE};
    uint32_t count = length / FILE_BLOCK_SIZE;

    int32_t ret = cipher->init(cipher, ENCRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        goto CLEAR_UP;
    }
    for (uint32_t i = 0; i < count; i++) {
        infile.read(reinterpret_cast<char *>(buffer), FILE_BLOCK_SIZE);
        ret = cipher->update(cipher, &input, &output);
        if (ret != 0) {
            LOGE("update failed!");
            goto CLEAR_UP;
        }
        if (output.data != nullptr && output.len > 0) {
            outfile.write(reinterpret_cast<const char *>(output.data), output.len);
        }
        if (output.data != nullptr) {
            HcfFree(output.data);
            output.data = nullptr;
        }
    }
    ret = cipher->doFinal(cipher, nullptr, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        goto CLEAR_UP;
    }
    if (output.data != nullptr && output.len > 0) {
        outfile.write((const char *)output.data, output.len);
    }

    if (output.data != nullptr) {
        HcfFree(output.data);
        output.data = nullptr;
    }
CLEAR_UP:
    outfile.close();
    infile.close();

    return ret;
}

static int32_t AesMultiBlockDecrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params)
{
    HcfBlob output = {};
    ifstream infile;
    ofstream outfile;
    infile.open("/data/test_aes_enc.txt", ios::in|ios::binary);
    infile.seekg (0, infile.end);
    uint32_t length = infile.tellg();
    infile.seekg (0, infile.beg);
    uint8_t buffer[1024] = {0};
    outfile.open("/data/test_aes_new.txt", ios::out|ios::binary);
    HcfBlob input = {.data = (uint8_t *)buffer, .len = FILE_BLOCK_SIZE};

    uint32_t count = length / FILE_BLOCK_SIZE;
    int32_t ret = cipher->init(cipher, DECRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        goto CLEAR_UP;
    }
    for (uint32_t i = 0; i < count; i++) {
        infile.read(reinterpret_cast<char *>(buffer), FILE_BLOCK_SIZE);
        ret = cipher->update(cipher, &input, &output);
        if (ret != 0) {
            LOGE("update failed!");
            goto CLEAR_UP;
        }
        if (output.data != nullptr && output.len > 0) {
            outfile.write(reinterpret_cast<const char *>(output.data), output.len);
        }
        if (output.data != nullptr) {
            HcfFree(output.data);
            output.data = nullptr;
        }
    }
    ret = cipher->doFinal(cipher, nullptr, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        goto CLEAR_UP;
    }
    if (output.data != nullptr && output.len > 0) {
        outfile.write((const char *)output.data, output.len);
    }

    if (output.data != nullptr) {
        HcfFree(output.data);
        output.data = nullptr;
    }
CLEAR_UP:
    outfile.close();
    infile.close();

    return ret;
}

// use ECB, test abnormal input
static int32_t AesEncryptWithInput(HcfCipher *cipher, HcfSymKey *key, HcfBlob *input,
    uint8_t *cipherText, int *cipherTextLen)
{
    HcfBlob output = { .data = nullptr, .len = 0 };
    int32_t maxLen = *cipherTextLen;
    int32_t ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    ret = cipher->update(cipher, input, &output);
    if (ret != 0) {
        LOGE("update failed!");
        return ret;
    }
    *cipherTextLen = output.len;
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        HcfBlobDataFree(&output);
    }

    ret = cipher->doFinal(cipher, nullptr, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText + *cipherTextLen, maxLen - *cipherTextLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        *cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("ciphertext", cipherText, *cipherTextLen);
    return 0;
}

static int32_t AesEncrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
    uint8_t *cipherText, int *cipherTextLen)
{
    uint8_t plainText[] = "this is test!";
    HcfBlob input = {.data = (uint8_t *)plainText, .len = 13};
    HcfBlob output = {};
    int32_t maxLen = *cipherTextLen;
    int32_t ret = cipher->init(cipher, ENCRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    ret = cipher->update(cipher, &input, &output);
    if (ret != 0) {
        LOGE("update failed!");
        return ret;
    }
    *cipherTextLen = output.len;
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        HcfBlobDataFree(&output);
    }

    ret = cipher->doFinal(cipher, nullptr, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText + *cipherTextLen, maxLen - *cipherTextLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        *cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("ciphertext", cipherText, *cipherTextLen);
    return 0;
}

static int32_t AesDecrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
    uint8_t *cipherText, int cipherTextLen)
{
    uint8_t plainText[] = "this is test!";
    HcfBlob input = {.data = (uint8_t *)cipherText, .len = cipherTextLen};
    HcfBlob output = {};
    int32_t maxLen = cipherTextLen;
    int32_t ret = cipher->init(cipher, DECRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    ret = cipher->update(cipher, &input, &output);
    if (ret != 0) {
        LOGE("update failed!");
        return ret;
    }
    cipherTextLen = output.len;
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        HcfBlobDataFree(&output);
    }

    ret = cipher->doFinal(cipher, nullptr, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText + cipherTextLen, maxLen - cipherTextLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("plainText", cipherText, cipherTextLen);
    if (cipherTextLen != sizeof(plainText) - 1) {
        return -1;
    }
    return memcmp(cipherText, plainText, cipherTextLen);
}

static int32_t AesNoUpdateEncWithInput(HcfCipher *cipher, HcfSymKey *key, HcfBlob *input,
    uint8_t *cipherText, int *cipherTextLen)
{
    HcfBlob output = { .data = nullptr, .len = 0 };
    int32_t maxLen = *cipherTextLen;
    int32_t ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    *cipherTextLen = 0;
    ret = cipher->doFinal(cipher, input, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        *cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("ciphertext", cipherText, *cipherTextLen);
    return 0;
}

// test encrypt and decrypt with null plain text
static int32_t AesDecryptEmptyMsg(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
    uint8_t *cipherText, int cipherTextLen)
{
    HcfBlob input = { .data = cipherText, .len = cipherTextLen };
    HcfBlob output = { .data = nullptr, .len = 0 };
    int32_t ret = cipher->init(cipher, DECRYPT_MODE, &(key->key), params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    ret = cipher->doFinal(cipher, &input, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.len == 0 && output.data == nullptr) {
        ret = 0;
    } else {
        ret = -1;
    }
    HcfBlobDataFree(&output);
    return ret;
}

static int32_t AesNoUpdateEncrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
    uint8_t *cipherText, int *cipherTextLen)
{
    uint8_t plainText[] = "this is test!";
    HcfBlob input = {.data = (uint8_t *)plainText, .len = 13};
    HcfBlob output = {};
    int32_t maxLen = *cipherTextLen;
    int32_t ret = cipher->init(cipher, ENCRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    *cipherTextLen = 0;
    ret = cipher->doFinal(cipher, &input, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        *cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("ciphertext", cipherText, *cipherTextLen);
    return 0;
}

static int32_t AesNoUpdateDecrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
    uint8_t *cipherText, int cipherTextLen)
{
    uint8_t plainText[] = "this is test!";
    HcfBlob input = {.data = (uint8_t *)cipherText, .len = cipherTextLen};
    HcfBlob output = {};
    int32_t maxLen = cipherTextLen;
    int32_t ret = cipher->init(cipher, DECRYPT_MODE, (HcfKey *)key, params);
    if (ret != 0) {
        LOGE("init failed! %d", ret);
        return ret;
    }

    cipherTextLen = 0;
    ret = cipher->doFinal(cipher, &input, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.data != nullptr) {
        if (memcpy_s(cipherText, maxLen, output.data, output.len) != EOK) {
            HcfBlobDataFree(&output);
            return -1;
        }
        cipherTextLen += output.len;
        HcfBlobDataFree(&output);
    }

    PrintfHex("plainText", cipherText, cipherTextLen);
    if (cipherTextLen != sizeof(plainText) - 1) {
        return -1;
    }
    return memcmp(cipherText, plainText, cipherTextLen);
}

/**
 * @tc.name: CryptoAesCipherTest.CryptoAesCipherTest001
 * @tc.desc: Verify whether the crypto framework is normal.
 * @tc.type: FUNC
 * @tc.require: I5QWEO
 */
HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest001, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

/**
 * @tc.name: CryptoAesCipherTest.CryptoAesCipherTest002
 * @tc.desc: Verify AES128 cipher algorithm.
 * @tc.type: FUNC
 * @tc.require: I5QWEG
 */
HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest002, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest003, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest004, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest005, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest006, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest007, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest008, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest009, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest010, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest011, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest012, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest013, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest014, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest015, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest016, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB1|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest017, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB1|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest018, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB1|PKCS7", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest019, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|NoPadding", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest020, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest021, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|PKCS7", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest022, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|NoPadding", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest023, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|PKCS5", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest024, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|PKCS7", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest025, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest026, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest027, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest028, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest029, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest030, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest031, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t codeCipherText[] = {
        0xF5, 0x12, 0xA0, 0x33, 0xCD, 0xCF, 0x0D, 0x32,
        0x3E, 0xFF, 0x80, 0x53, 0x89, 0xB6, 0xE4, 0xFE
    };

    ret = ConvertSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = memcmp(cipherText, codeCipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("cipherText compare failed!");
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest032, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest033, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest034, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest035, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest036, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest037, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest038, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest039, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest040, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest041, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest042, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest043, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest044, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest045, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest046, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest047, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB1|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest048, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB1|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest049, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB1|PKCS7", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest050, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|NoPadding", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest051, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest052, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB8|PKCS7", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest053, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|NoPadding", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest054, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|PKCS5", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest055, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfIvParamsSpec ivSpec = {};
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

    ret = HcfCipherCreate("AES128|CFB128|PKCS7", &cipher);  // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    HcfObjDestroy((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest056, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest057, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest058, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[16] = {0};
    uint8_t iv[12] = {0}; // openssl only support nonce 12 bytes, tag 16bytes
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 16, cipherText + cipherTextLen - 16, 16);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 16;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest059, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest060, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest061, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[8] = {0};
    uint8_t tag[12] = {0};
    uint8_t iv[7] = {0};
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, 12, cipherText + cipherTextLen - 12, 12);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= 12;

    ret = AesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&spec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest062, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t codeCipherText[] = {
        0xF5, 0x12, 0xA0, 0x33, 0xCD, 0xCF, 0x0D, 0x32,
        0x3E, 0xFF, 0x80, 0x53, 0x89, 0xB6, 0xE4, 0xFE
    };

    ret = ConvertSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("ConvertSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = memcmp(cipherText, codeCipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("cipherText compare failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest063, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GeneratorFile("/data/test_aes.txt", 10 * FILE_BLOCK_SIZE);
    if (ret != 0) {
        LOGE("GeneratorFile failed!");
        goto CLEAR_UP;
    }
    ret = ConvertSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("ConvertSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesMultiBlockEncrypt(cipher, key, nullptr);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesMultiBlockDecrypt(cipher, key, nullptr);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    ret = CompareFileContent();
    if (ret != 0) {
        LOGE("CompareFileContent failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest064, TestSize.Level0)
{
    int ret = 0;
    HcfIvParamsSpec ivSpec = {};
    uint8_t iv[16] = {0};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GeneratorFile("/data/test_aes.txt", 10 * FILE_BLOCK_SIZE);
    if (ret != 0) {
        LOGE("GeneratorFile failed!");
        goto CLEAR_UP;
    }

    ret = ConvertSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("ConvertSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CBC|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesMultiBlockEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesMultiBlockDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    ret = CompareFileContent();
    if (ret != 0) {
        LOGE("CompareFileContent failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest065, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = ConvertSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("ConvertSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CTR|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = GeneratorFile("/data/test_aes.txt", 10 * FILE_BLOCK_SIZE);
    if (ret != 0) {
        LOGE("GeneratorFile failed!");
        goto CLEAR_UP;
    }

    ret = AesMultiBlockEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesMultiBlockDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    ret = CompareFileContent();
    if (ret != 0) {
        LOGE("CompareFileContent failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest066, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|OFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = GeneratorFile("/data/test_aes.txt", 10 * FILE_BLOCK_SIZE);
    if (ret != 0) {
        LOGE("GeneratorFile failed!");
        goto CLEAR_UP;
    }

    ret = AesMultiBlockEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesMultiBlockDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    ret = CompareFileContent();
    if (ret != 0) {
        LOGE("CompareFileContent failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest067, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[16] = {0};

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 16;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = GeneratorFile("/data/test_aes.txt", 10 * FILE_BLOCK_SIZE);
    if (ret != 0) {
        LOGE("GeneratorFile failed!");
        goto CLEAR_UP;
    }

    ret = AesMultiBlockEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesMultiBlockDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec);
    if (ret != 0) {
        LOGE("AesNoUpdateDecrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    ret = CompareFileContent();
    if (ret != 0) {
        LOGE("CompareFileContent failed!");
        goto CLEAR_UP;
    }

CLEAR_UP:
    HcfObjDestroy((HcfObjectBase *)key);
    HcfObjDestroy((HcfObjectBase *)cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest068, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest069, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
    key->clearMem(key);
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest070, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
    key->clearMem(key);
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest071, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest072, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest073, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CTR|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest074, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CTR|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest075, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest076, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest077, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest078, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest079, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CFB1|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest080, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CFB1|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest081, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CFB8|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest082, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CFB8|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest083, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CFB128|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest084, TestSize.Level0)
{
    int ret = 0;
    uint8_t iv[AES_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfIvParamsSpec ivSpec = {};
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    ivSpec.iv.data = iv;
    ivSpec.iv.len = AES_IV_LEN;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CFB128|PKCS5", &cipher);    // CFB1/CFB8/CFB128 bit
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(ivSpec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, &(ivSpec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest085, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[GCM_AAD_LEN] = { 0 };
    uint8_t tag[GCM_TAG_LEN] = { 0 };
    uint8_t iv[GCM_IV_LEN] = { 0 }; // openssl only support nonce 12 bytes, tag 16 bytes
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|GCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, GCM_TAG_LEN, cipherText + cipherTextLen - GCM_TAG_LEN, GCM_TAG_LEN);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= GCM_TAG_LEN;

    ret = AesDecrypt(cipher, key, &(spec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed, ret:%d!", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest086, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[GCM_AAD_LEN] = { 0 };
    uint8_t tag[GCM_TAG_LEN] = { 0 };
    uint8_t iv[GCM_IV_LEN] = { 0 }; // openssl only support nonce 12 bytes, tag 16 bytes
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|GCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, GCM_TAG_LEN, cipherText + cipherTextLen - GCM_TAG_LEN, GCM_TAG_LEN);
    PrintfHex("gcm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= GCM_TAG_LEN;

    ret = AesDecrypt(cipher, key, &(spec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed, ret:%d!", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest087, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[CCM_AAD_LEN] = { 0 };
    uint8_t tag[CCM_TAG_LEN] = { 0 };
    uint8_t iv[CCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES192", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES192|CCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, CCM_TAG_LEN, cipherText + cipherTextLen - CCM_TAG_LEN, CCM_TAG_LEN);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= CCM_TAG_LEN;

    ret = AesDecrypt(cipher, key, &(spec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest088, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[CCM_AAD_LEN] = { 0 };
    uint8_t tag[CCM_TAG_LEN] = { 0 };
    uint8_t iv[CCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|CCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
        goto CLEAR_UP;
    }

    (void)memcpy_s(spec.tag.data, CCM_TAG_LEN, cipherText + cipherTextLen - CCM_TAG_LEN, CCM_TAG_LEN);
    PrintfHex("ccm tag", spec.tag.data, spec.tag.len);
    cipherTextLen -= CCM_TAG_LEN;

    ret = AesDecrypt(cipher, key, &(spec.base), cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest089, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    const char *cipherName = "AES128|CFB|NoPadding";
    const char *retAlgo = nullptr;
    ret = HcfCipherCreate(cipherName, &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    retAlgo = cipher->getAlgorithm(cipher);
    if (retAlgo == nullptr) {
        LOGE("cipher getAlgorithm failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
        goto CLEAR_UP;
    }

    ret = strcmp(retAlgo, cipherName);
    if (ret != 0) {
        LOGE("cipher getAlgorithm failed!");
    }
CLEAR_UP:
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest090, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    const char *cipherName = "AES128|CFB|NoPadding";
    const char *retAlgo = nullptr;
    ret = HcfCipherCreate(cipherName, &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    retAlgo = cipher->getAlgorithm(nullptr);
    if (retAlgo == nullptr) {
        LOGE("cipher getAlgorithm failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest091, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKeyGenerator *generator = nullptr;
    const char *cipherName = "AES128|CFB|NoPadding";
    const char *retAlgo = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate(cipherName, &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    retAlgo = cipher->getAlgorithm(reinterpret_cast<HcfCipher *>(generator));
    if (retAlgo == nullptr) {
        LOGE("cipher getAlgorithm failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(generator);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest092, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    const char *inputAlgoName = "AES128";
    const char *generatorAlgoName = nullptr;

    ret = HcfSymKeyGeneratorCreate(inputAlgoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // generator getAlgoName
    generatorAlgoName = generator->getAlgoName(generator);
    if (generatorAlgoName == nullptr) {
        LOGE("generator getAlgoName returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
        goto CLEAR_UP;
    }

    ret = strcmp(generatorAlgoName, inputAlgoName);
    if (ret != 0) {
        LOGE("generator getAlgoName failed!");
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest093, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    const char *generatorAlgoName = nullptr;
    const char *inputAlgoName = "AES128";

    ret = HcfSymKeyGeneratorCreate(inputAlgoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    // generator getAlgoName
    generatorAlgoName = generator->getAlgoName(nullptr);
    if (generatorAlgoName == nullptr) {
        LOGE("generator getAlgoName failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest094, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    const char *generatorAlgoName = nullptr;
    const char *inputAlgoName = "AES128";

    ret = HcfSymKeyGeneratorCreate(inputAlgoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // generator getAlgoName
    generatorAlgoName = generator->getAlgoName(reinterpret_cast<HcfSymKeyGenerator *>(key));
    if (generatorAlgoName == nullptr) {
        LOGE("generator getAlgoName failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest095, TestSize.Level0)
{
    int ret = 0;
    HcfSymKey *key = nullptr;
    const char *inputAlgoName = "AES128";
    const char *keyAlgoName = nullptr;

    ret = GenerateSymKey(inputAlgoName, &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getAlgorithm
    keyAlgoName = key->key.getAlgorithm(&(key->key));
    if (keyAlgoName == nullptr) {
        LOGE("key getAlgorithm returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
        goto CLEAR_UP;
    }

    ret = strcmp(keyAlgoName, inputAlgoName);
    if (ret != 0) {
        LOGE("key getAlgorithm failed!");
    }
CLEAR_UP:
    HcfObjDestroy(key);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest096, TestSize.Level0)
{
    int ret = 0;
    HcfSymKey *key = nullptr;
    const char *inputAlgoName = "AES128";
    const char *keyAlgoName = nullptr;

    ret = GenerateSymKey(inputAlgoName, &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getAlgorithm
    keyAlgoName = key->key.getAlgorithm(nullptr);
    if (keyAlgoName == nullptr) {
        LOGE("key getAlgorithm returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest097, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    const char *inputAlgoName = "AES128";
    const char *keyAlgoName = nullptr;

    ret = HcfSymKeyGeneratorCreate(inputAlgoName, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getAlgorithm
    keyAlgoName = key->key.getAlgorithm(reinterpret_cast<HcfKey *>(generator));
    if (keyAlgoName == nullptr) {
        LOGE("key getAlgorithm returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest098, TestSize.Level0)
{
    int ret = 0;
    HcfSymKey *key = nullptr;
    const char *keyFormat = "PKCS#8";
    const char *retFormat = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // key GetFormat
    retFormat = key->key.getFormat(&(key->key));
    if (retFormat == nullptr) {
        LOGE("key GetFormat returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
        goto CLEAR_UP;
    }

    ret = strcmp(retFormat, keyFormat);
    if (ret != 0) {
        LOGE("key GetFormat failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest099, TestSize.Level0)
{
    int ret = 0;
    HcfSymKey *key = nullptr;
    const char *retFormat = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getFormat
    retFormat = key->key.getFormat(nullptr);
    if (retFormat == nullptr) {
        LOGE("key GetFormat returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest100, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    const char *retFormat = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getFormat
    retFormat = key->key.getFormat(reinterpret_cast<HcfKey *>(generator));
    if (retFormat == nullptr) {
        LOGE("key GetFormat returns nullptr.");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest101, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob encodedBlob = { 0 };
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getEncoded
    ret = key->key.getEncoded(&(key->key), &encodedBlob);
    if (ret != 0) {
        LOGE("key GetEncoded failed.");
        goto CLEAR_UP;
    }

    if (encodedBlob.len != keyTmpBlob.len) {
        LOGE("key GetEncoded failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
        goto CLEAR_UP;
    }
    ret = memcmp(encodedBlob.data, keyTmpBlob.data, keyTmpBlob.len);

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    if (encodedBlob.data != nullptr) {
        HcfFree(encodedBlob.data);
        encodedBlob.data = nullptr;
    }
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest102, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob encodedBlob = { 0 };
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getEncoded
    ret = key->key.getEncoded(nullptr, &encodedBlob);
    if (ret != 0) {
        LOGE("key GetEncoded failed.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    if (encodedBlob.data != nullptr) {
        HcfFree(encodedBlob.data);
        encodedBlob.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest103, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob encodedBlob = { 0 };
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    // key getEncoded
    ret = key->key.getEncoded(reinterpret_cast<HcfKey *>(generator), &encodedBlob);
    if (ret != 0) {
        LOGE("key GetEncoded failed.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    if (encodedBlob.data != nullptr) {
        HcfFree(encodedBlob.data);
        encodedBlob.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest104, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob encodedBlob = { 0 };
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };
    SymKeyImpl *impl = nullptr;
    size_t tmpLen = 0;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }
    impl = reinterpret_cast<SymKeyImpl *>(key);
    tmpLen = impl->keyMaterial.len;
    impl->keyMaterial.len = 0;

    // key getEncoded
    ret = key->key.getEncoded(&(key->key), &encodedBlob);
    impl->keyMaterial.len = tmpLen;
    if (ret != 0) {
        LOGE("key GetEncoded failed.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    if (encodedBlob.data != nullptr) {
        HcfFree(encodedBlob.data);
        encodedBlob.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest105, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob encodedBlob = { 0 };
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    key->clearMem(nullptr);

    ret = key->key.getEncoded(&(key->key), &encodedBlob);
    if (ret != 0) {
        LOGE("key GetEncoded failed.");
        goto CLEAR_UP;
    }
    if ((encodedBlob.data != nullptr) && (encodedBlob.data[0] != '\0')) {
        LOGE("clearMem failed!");
        ret = HCF_ERR_CRYPTO_OPERATION;
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    if (encodedBlob.data != nullptr) {
        HcfFree(encodedBlob.data);
        encodedBlob.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest106, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;

    ret = HcfSymKeyGeneratorCreate("RSA128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed! Should not select RSA for symKey generator.");
    }

    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest107, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;

    ret = HcfSymKeyGeneratorCreate("RSA512", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed! Should not select RSA for symKey generator.");
    }

    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest108, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;

    ret = HcfSymKeyGeneratorCreate("", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed! Should not select empty string for symKey generator.");
    }

    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest109, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;

    ret = HcfSymKeyGeneratorCreate(nullptr, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed! Should not select nullptr for symKey generator.");
    }

    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest110, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;

    ret = HcfSymKeyGeneratorSpiCreate(nullptr, nullptr);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorSpiCreate failed!");
    }

    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest111, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(nullptr, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest112, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(reinterpret_cast<HcfSymKeyGenerator *>(cipher), &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest113, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->convertSymKey(nullptr, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest114, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->convertSymKey(reinterpret_cast<HcfSymKeyGenerator *>(cipher), &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest115, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = 0 };

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }

    ret = generator->convertSymKey(generator, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest116, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate("RSA128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed! Should not select RSA for GCM generator.");
    }

    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest117, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // allow input without encryption mode. It will pick the last PKCS5, and use default aes128ecb.
    ret = HcfCipherCreate("AES128|NoPadding|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest118, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;

    // not allow '|' without content, because findAbility will fail for "" input
    ret = HcfCipherCreate("AES128|GCM|", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed! Should select padding mode for AES generator.");
    }

    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest119, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // allow input without encryption mode. It will use default aes128ecb.
    ret = HcfCipherCreate("AES128|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest120, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // allow input without encryption mode. It will use default aes128ecb.
    ret = HcfCipherCreate("AES128", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest121, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate("", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
    }

    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest122, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate(nullptr, &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
    }

    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest123, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // allow input with more than one padding mode. It will pick the last PKCS5.
    ret = HcfCipherCreate("AES128|ECB|NoPadding|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }
CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest124, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES256|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    // It is not allowed that AES128 in key is smaller AES256 in cipher.
    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }
    
    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest125, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES256", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest126, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    // CBC, CTR, OFB, CFB enc/dec success,
    // GCM, CCM enc/dec failed with params set to nullptr.
    ret = HcfCipherCreate("AES128|GCM|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecrypt(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest127, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("GenerateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(nullptr, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed! Should input cipher when init.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest128, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfCipher *cipher = nullptr;

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(cipher, ENCRYPT_MODE, nullptr, nullptr);
    if (ret != 0) {
        LOGE("init failed! Should input key when init.");
    }

CLEAR_UP:
    HcfObjDestroy(cipher);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest129, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGenerator *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfCipher *cipher = nullptr;

    ret = HcfSymKeyGeneratorCreate("AES128", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(reinterpret_cast<HcfCipher *>(generator), ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed! Should input key when init.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest130, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[GCM_AAD_LEN] = { 0 };
    uint8_t tag[GCM_TAG_LEN] = { 0 };
    uint8_t iv[GCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = nullptr;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest131, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[GCM_AAD_LEN] = { 0 };
    uint8_t tag[GCM_TAG_LEN] = { 0 };
    uint8_t iv[GCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = nullptr;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest132, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[GCM_AAD_LEN] = { 0 };
    uint8_t tag[GCM_TAG_LEN] = { 0 };
    uint8_t iv[GCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    HcfGcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = nullptr;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|GCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed, ret:%d!", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest133, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest134, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[CCM_AAD_LEN] = { 0 };
    uint8_t tag[CCM_TAG_LEN] = { 0 };
    uint8_t iv[CCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = nullptr;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest135, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[CCM_AAD_LEN] = { 0 };
    uint8_t tag[CCM_TAG_LEN] = { 0 };
    uint8_t iv[CCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = tag;
    spec.tag.len = sizeof(tag);
    spec.iv.data = nullptr;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest136, TestSize.Level0)
{
    int ret = 0;
    uint8_t aad[CCM_AAD_LEN] = { 0 };
    uint8_t tag[CCM_TAG_LEN] = { 0 };
    uint8_t iv[CCM_IV_LEN] = { 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfCcmParamsSpec spec = {};
    spec.aad.data = aad;
    spec.aad.len = sizeof(aad);
    spec.tag.data = nullptr;
    spec.tag.len = sizeof(tag);
    spec.iv.data = iv;
    spec.iv.len = sizeof(iv);

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|CCM|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncrypt(cipher, key, &(spec.base), cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest137, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t plainText[] = "this is test!";
    HcfBlob input = { .data = plainText, .len = 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }
    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncryptWithInput(cipher, key, &input, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecryptEmptyMsg(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest138, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob input = { .data = nullptr, .len = 0 };
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }
    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesEncryptWithInput(cipher, key, &input, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecryptEmptyMsg(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest139, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob input = { .data = nullptr, .len = PLAINTEXT_LEN };
    HcfBlob output = { .data = nullptr, .len = 0 };

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed!");
        goto CLEAR_UP;
    }
    
    ret = cipher->update(nullptr, &input, &output);
    if (ret != 0) {
        LOGE("update failed! Blob data should not be nullptr.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    if (output.data != nullptr) {
        HcfFree(output.data);
        output.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest140, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    HcfBlob input = { .data = nullptr, .len = PLAINTEXT_LEN };
    HcfBlob output = { .data = nullptr, .len = 0 };

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed!");
        goto CLEAR_UP;
    }
    
    ret = cipher->update(reinterpret_cast<HcfCipher *>(key), &input, &output);
    if (ret != 0) {
        LOGE("update failed! Blob data should not be nullptr.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    if (output.data != nullptr) {
        HcfFree(output.data);
        output.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest141, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t cipherText[CIPHER_TEXT_LEN] = { 0 };
    int cipherTextLen = CIPHER_TEXT_LEN;

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = AesNoUpdateEncWithInput(cipher, key, nullptr, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("AesEncrypt failed! %d", ret);
        goto CLEAR_UP;
    }

    ret = AesDecryptEmptyMsg(cipher, key, nullptr, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("AesDecrypt failed! %d", ret);
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest142, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t plainText[] = "this is test!";
    HcfBlob input = { .data = plainText, .len = PLAINTEXT_LEN };

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed!");
        goto CLEAR_UP;
    }
    
    ret = cipher->doFinal(cipher, &input, nullptr);
    if (ret != 0) {
        LOGE("update failed! Blob data should not be nullptr.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest143, TestSize.Level0)
{
    int ret = 0;
    HcfCipher *cipher = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t plainText[] = "this is test!";
    HcfBlob input = { .data = plainText, .len = PLAINTEXT_LEN };
    HcfBlob output = { .data = nullptr, .len = 0 };

    ret = GenerateSymKey("AES128", &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto CLEAR_UP;
    }

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }

    ret = cipher->init(cipher, ENCRYPT_MODE, &(key->key), nullptr);
    if (ret != 0) {
        LOGE("init failed!");
        goto CLEAR_UP;
    }
    
    ret = cipher->doFinal(reinterpret_cast<HcfCipher *>(key), &input, &output);
    if (ret != 0) {
        LOGE("update failed! Blob data should not be nullptr.");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(cipher);
    if (output.data != nullptr) {
        HcfFree(output.data);
        output.data = nullptr;
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest144, TestSize.Level0)
{
    int ret = HcfCipherAesGeneratorSpiCreate(nullptr, nullptr);
    if (ret != 0) {
        LOGE("HcfCipherAesGeneratorSpiCreate failed!");
    }
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest145, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGeneratorSpi *generator = nullptr;
    HcfSymKey *key = nullptr;
    SymKeyAttr attr = { .algo = HCF_ALG_AES, .keySize = AES_KEY_SIZE };

    ret = HcfSymKeyGeneratorSpiCreate(&attr, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorSpiCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->engineGenerateSymmKey(nullptr, &key);
    if (ret != 0) {
        LOGE("engineGenerateSymmKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest146, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGeneratorSpi *generator = nullptr;
    HcfSymKey *key = nullptr;
    HcfCipher *cipher = nullptr;
    SymKeyAttr attr = { .algo = HCF_ALG_AES, .keySize = AES_KEY_SIZE };

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = HcfSymKeyGeneratorSpiCreate(&attr, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorSpiCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->engineGenerateSymmKey(reinterpret_cast<HcfSymKeyGeneratorSpi *>(cipher), &key);
    if (ret != 0) {
        LOGE("engineGenerateSymmKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest147, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGeneratorSpi *generator = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };
    SymKeyAttr attr = { .algo = HCF_ALG_AES, .keySize = AES_KEY_SIZE };

    ret = HcfSymKeyGeneratorSpiCreate(&attr, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorSpiCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->engineConvertSymmKey(nullptr, &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("engineConvertSymmKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(CryptoAesCipherTest, CryptoAesCipherTest148, TestSize.Level0)
{
    int ret = 0;
    HcfSymKeyGeneratorSpi *generator = nullptr;
    HcfSymKey *key = nullptr;
    uint8_t keyMaterial[] = {
        0xba, 0x3b, 0xc2, 0x71, 0x21, 0x1e, 0x30, 0x56,
        0xad, 0x47, 0xfc, 0x5a, 0x46, 0x39, 0xee, 0x7c
    };
    HcfBlob keyTmpBlob = { .data = keyMaterial, .len = KEY_MATERIAL_LEN };
    HcfCipher *cipher = nullptr;
    SymKeyAttr attr = { .algo = HCF_ALG_AES, .keySize = AES_KEY_SIZE };

    ret = HcfCipherCreate("AES128|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto CLEAR_UP;
    }
    ret = HcfSymKeyGeneratorSpiCreate(&attr, &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorSpiCreate failed!%d", ret);
        goto CLEAR_UP;
    }
    ret = generator->engineConvertSymmKey(reinterpret_cast<HcfSymKeyGeneratorSpi *>(cipher), &keyTmpBlob, &key);
    if (ret != 0) {
        LOGE("engineConvertSymmKey failed!");
    }

CLEAR_UP:
    HcfObjDestroy(key);
    HcfObjDestroy(generator);
    HcfObjDestroy(cipher);
    EXPECT_NE(ret, 0);
}
}