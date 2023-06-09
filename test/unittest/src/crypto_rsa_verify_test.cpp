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
#include "asy_key_generator.h"
#include "blob.h"
#include "memory.h"
#include "signature.h"
#include "cstring"


using namespace std;
using namespace testing::ext;

namespace {
class CryptoRsaVerifyTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void CryptoRsaVerifyTest::SetUp() {}
void CryptoRsaVerifyTest::TearDown() {}
void CryptoRsaVerifyTest::SetUpTestCase() {}
void CryptoRsaVerifyTest::TearDownTestCase() {}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest100, TestSize.Level0)
{
    HcfVerify *verify = NULL;
    int32_t res = HcfVerifyCreate("RSA1024|PSS|SHA256", &verify);
    EXPECT_NE(res, HCF_SUCCESS);
    ASSERT_EQ(verify, nullptr);

    OH_HCF_OBJ_DESTROY(verify);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest110, TestSize.Level0)
{
    HcfVerify *verify = NULL;
    int32_t res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    ASSERT_NE(verify, nullptr);

    OH_HCF_OBJ_DESTROY(verify);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest200, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);
    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;
    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest210, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest220, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PSS|SHA256|MGF1_SHA512", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA512", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest230, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PKCS1|SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PKCS1|SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest240, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    uint8_t errorverify[] = "asdfasdfasdfasf";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfBlob invalidverifyData = {.data = errorverify, .len = strlen((char *)errorverify)};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PKCS1|SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PKCS1|SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &invalidverifyData);
    EXPECT_EQ(res, 0);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

// Incorrect case: different mode
HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest250, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PKCS1|SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA512", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 0);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

// Incorrect case: different mgf1md
HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest260, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA512", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 0);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}


// Incorrect case: different pkcs1 md, verifu fail
HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest270, TestSize.Level0)
{
    uint8_t plan[] = "this is rsa verify test aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan, .len = strlen((char *)plan)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PKCS1|SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input, &verifyData);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PKCS1|SHA512", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input, &verifyData);
    EXPECT_EQ(res, 0);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

// check update_func in PSS padding
HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest280, TestSize.Level0)
{
    uint8_t plan1[] = "this is rsa verify test plane1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    uint8_t plan2[] = "this is rsa verify test plane2 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input = {.data = plan1, .len = strlen((char *)plan1)};
    HcfBlob inputEx = {.data = plan2, .len = strlen((char *)plan2)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->update(sign, &input);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &inputEx, &verifyData);
    EXPECT_EQ(res, HCF_SUCCESS);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PSS|SHA256|MGF1_SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->update(verify, &input);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &inputEx, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}

// check update in PKCS1 padding
HWTEST_F(CryptoRsaVerifyTest, CryptoRsaVerifyTest290, TestSize.Level0)
{
    uint8_t plan1[] = "this is rsa verify test plane1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    uint8_t plan2[] = "this is rsa verify test plane2 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";
    uint8_t plan3[] = "this is rsa verify test plane3 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";

    HcfAsyKeyGenerator *generator = NULL;
    int32_t res = HcfAsyKeyGeneratorCreate("RSA2048|PRIMES_2", &generator);

    HcfKeyPair *keyPair = NULL;
    res = generator->generateKeyPair(generator, NULL, &keyPair);
    EXPECT_EQ(res, HCF_SUCCESS);

    HcfPubKey *pubkey = keyPair->pubKey;
    HcfPriKey *prikey = keyPair->priKey;

    HcfBlob input1 = {.data = plan1, .len = strlen((char *)plan1)};
    HcfBlob input2 = {.data = plan2, .len = strlen((char *)plan2)};
    HcfBlob input3 = {.data = plan3, .len = strlen((char *)plan3)};
    HcfBlob verifyData = {.data = NULL, .len = 0};
    HcfSign *sign = NULL;
    res = HcfSignCreate("RSA1024|PKCS1|SHA256", &sign);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->init(sign, NULL, prikey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->update(sign, &input1);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->update(sign, &input2);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = sign->sign(sign, &input3, &verifyData);
    EXPECT_EQ(res, HCF_SUCCESS);
    OH_HCF_OBJ_DESTROY(sign);

    HcfVerify *verify = NULL;
    res = HcfVerifyCreate("RSA1024|PKCS1|SHA256", &verify);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->init(verify, NULL, pubkey);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->update(verify, &input1);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->update(verify, &input2);
    EXPECT_EQ(res, HCF_SUCCESS);
    res = verify->verify(verify, &input3, &verifyData);
    EXPECT_EQ(res, 1);
    OH_HCF_OBJ_DESTROY(verify);

    HcfFree(verifyData.data);
    OH_HCF_OBJ_DESTROY(keyPair);
    OH_HCF_OBJ_DESTROY(generator);
}
}
