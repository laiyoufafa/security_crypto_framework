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
#include "securec.h"

#include "sym_key_generator.h"
#include "cipher.h"
#include "log.h"
#include "memory.h"
#include "detailed_iv_params.h"
#include "detailed_gcm_params.h"
#include "detailed_ccm_params.h"


using namespace std;
using namespace testing::ext;

namespace {
class Crypto3DesCipherTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void Crypto3DesCipherTest::SetUpTestCase() {}
void Crypto3DesCipherTest::TearDownTestCase() {}

void Crypto3DesCipherTest::SetUp() // add init here, this will be called before test.
{
}

void Crypto3DesCipherTest::TearDown() // add destroy here, this will be called when test case done.
{
}

static int32_t DesEncrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
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
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText, maxLen, output.data, output.len);
    }
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
    }
    ret = cipher->doFinal(cipher, NULL, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText + *cipherTextLen, maxLen - *cipherTextLen, output.data, output.len);
    }
    *cipherTextLen += output.len;
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
    }
    return 0;
}

static int32_t DesDecrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
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
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText, maxLen, output.data, output.len);
    }
    cipherTextLen = output.len;
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
        output.len = 0;
    }
    ret = cipher->doFinal(cipher, NULL, &output);
    if (ret != 0) {
        LOGE("doFinal failed!");
        return ret;
    }
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText + cipherTextLen, maxLen - cipherTextLen, output.data, output.len);
    }
    cipherTextLen += output.len;
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
        output.len = 0;
    }

    ret = memcmp(cipherText, plainText, cipherTextLen);
    ret =  ret || (sizeof(plainText) - 1 == cipherTextLen) ? 0 : 1;
    return ret;
}

static int32_t DesNoUpdateEncrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
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
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText, maxLen, output.data, output.len);
    }
    *cipherTextLen += output.len;
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
    }
    return 0;
}

static int32_t DesNoUpdateDecrypt(HcfCipher *cipher, HcfSymKey *key, HcfParamsSpec *params,
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
    if (output.len > 0 && output.data != NULL) {
        (void)memcpy_s(cipherText, maxLen, output.data, output.len);
    }
    cipherTextLen += output.len;
    if (output.data != NULL) {
        HcfFree(output.data);
        output.data = NULL;
        output.len = 0;
    }

    ret = memcmp(cipherText, plainText, cipherTextLen);
    ret =  ret || (sizeof(plainText) - 1 == cipherTextLen) ? 0 : 1;
    return ret;
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest001, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest002, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest003, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest004, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest005, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }
    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest006, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }
    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest007, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest008, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }
    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest009, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest010, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest011, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest012, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest013, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest014, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest015, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|ECB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, NULL, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, NULL, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}


HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest016, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_NE(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest017, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest018, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CBC|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest019, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest020, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest021, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|OFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }


clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest022, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|NoPadding", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest023, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|PKCS5", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(Crypto3DesCipherTest, Crypto3DesCipherTest024, TestSize.Level0)
{
    int ret = 0;
    uint8_t cipherText[128] = {0};
    int cipherTextLen = 128;

    HcfSymKeyGenerator *generator = NULL;
    HcfCipher *cipher = NULL;
    HcfSymKey *key = NULL;
    uint8_t iv[8] = {0};
    HcfIvParamsSpec ivSpec = {};
    ivSpec.iv.data = iv;
    ivSpec.iv.len = 8;

    ret = HcfSymKeyGeneratorCreate("3DES192", &generator);
    if (ret != 0) {
        LOGE("HcfSymKeyGeneratorCreate failed!");
        goto clearup;
    }

    ret = generator->generateSymKey(generator, &key);
    if (ret != 0) {
        LOGE("generateSymKey failed!");
        goto clearup;
    }

    ret = HcfCipherCreate("3DES192|CFB|PKCS7", &cipher);
    if (ret != 0) {
        LOGE("HcfCipherCreate failed!");
        goto clearup;
    }

    ret = DesNoUpdateEncrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, &cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateEncrypt failed! %d", ret);
        goto clearup;
    }

    ret = DesNoUpdateDecrypt(cipher, key, (HcfParamsSpec *)&ivSpec, cipherText, cipherTextLen);
    if (ret != 0) {
        LOGE("DesNoUpdateDecrypt failed! %d", ret);
        goto clearup;
    }

clearup:
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)key);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)cipher);
    OH_HCF_OBJ_DESTROY((HcfObjectBase *)generator);
    EXPECT_EQ(ret, 0);
}
}