# PKCS11 Test

## On this page:
1. [Introduction](#1-introduction)
2. [PKCS11 Test Configurations](#2-pkcs11-test-configurations)
3. [Prerequisites For PKCS11 Test](#3-prerequisites-for-pkcs11-test)
4. [Source Code Organization](#4-source-code-organization)
5. [Implement PKCS11 Test Application](#5-implement-pkcs11-test-application)
6. [Run The PKCS11 Test](#6-run-the-pkcs11-test)<br>
</t>6.1 [Setup the provisioning mechanism and key function](#61-setup-the-provisioning-mechanism-and-key-function)<br>
</t>6.2 [Compile and run the PKCS11 test application](#62-compile-and-run-the-pkcs11-test-application)<br>

## 1. Introduction
[PKCS #11 ](https://en.wikipedia.org/wiki/PKCS_11) is a standardize API to allow application software to use, create, modify and delete cryptographic objects. corePKCS11 defines a subset of PKCS#11 APIs to build FreeRTOS demos. The following table lists the PKCS#11 API subset and the targeting FreeRTOS demos.


|PKCS#11 API	|Category	|Any	|Provisioning demo	|TLS	|FreeRTOS+TCP	|OTA	|
|---	|---	|---	|---	|---	|---	|---	|
|C_Initialize	|General Purpose Functions	|O	|	|	|	|	|
|C_Finalize	|General Purpose Functions	|O	|	|	|	|	|
|C_GetFunctionList	|General Purpose Functions	|O	|	|	|	|	|
|C_GetSlotList	|Slot and token management functions	|O	|	|	|	|	|
|C_GetMechanismInfo	|Slot and token management functions	|O	|	|	|	|	|
|C_InitToken	|Slot and token management functions	|	|O	|	|	|	|
|C_GetTokenInfo	|Slot and token management functions	|	|O	|	|	|	|
|C_OpenSession	|Session management functions	|O	|	|	|	|	|
|C_CloseSession	|Session management functions	|O	|	|	|	|	|
|C_Login	|Session management functions	|O	|	|	|	|	|
|C_CreateObject	|Object managements functions	|	|O	|	|	|	|
|C_DestroyObject	|Object managements functions	|	|O	|	|	|	|
|C_GetAttributeValue	|Object managements functions	|	|	|O	|	|O	|
|C_FindObjectsInit	|Object managements functions	|	|	|O	|	|O	|
|C_FindObjects	|Object managements functions	|	|	|O	|	|O	|
|C_FindObjectsFinal	|Object managements functions	|	|	|O	|	|O	|
|C_DigestInit	|Message digesting functions	|	|	|	|O	|O	|
|C_DigestUpdate	|Message digesting functions	|	|	|	|O	|O	|
|C_DigestFinal	|Message digesting functions	|	|	|	|O	|O	|
|C_SignInit	|Signing and MACing functions	|	|	|O	|	|	|
|C_Sign	|Signing and MACing functions	|	|	|O	|	|	|
|C_VerifyInit	|Functions for verifying signatures and MACs	|	|	|	|	|O	|
|C_Verify	|Functions for verifying signatures and MACs	|	|	|	|	|O	|
|C_GenerateKeyPair	|Key management functions	|	|O	|	|	|	|
|C_GenerateRandom	|Random number generation functions	|	|	|O	|O	|	|

The PKCS11 tests help to verify the implementation of PKCS11 subset. The test directly exercises the PKCS11 implementation on the device under testing. User runs the PKCS11 test by running a test application. The test application is usually implemented by calling the provided PKCS11 test routine from the main function. By passing this test, the PKCS11 subset implementation is validated to run the targeting FreeRTOS demos.

## 2. PKCS11 Test Configurations

The following table lists the required test configurations for PKCS11 tests. These test configurations need to be defined in **test_param_config.h**.

|Configuration	|Description	|
|--- | --- |
|PKCS11_TEST_RSA_KEY_SUPPORT	|The porting support RSA key functions.	|
|PKCS11_TEST_EC_KEY_SUPPORT	|The porting support EC key functions.	|
|PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT	|The porting support import private key. RSA and EC key import will be validated in the test if the corresponding key functions are enabled.	|
|PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT	|The porting support keypair generation. EC keypair generation will be validated in the test if the corresponding key functions are enabled.	|
|PKCS11_TEST_PREPROVISIONED_SUPPORT	|The porting has pre-provisioned credentials. These test labels, PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS, PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS and PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS, are the labels of the pre-provisioned credentials. 	|
|PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS	|The label of the private key used in the test.	|
|PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS	|The label of the public key used in the test.	|
|PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS	|The label of the certificate key used in the test.	|
|PKCS11_TEST_JITP_CODEVERIFY_ROOT_CERT_SUPPORTED	|The porting support storage for JITP. Set 1 to enable the JITP codeverify test.	|
|PKCS11_TEST_LABEL_CODE_VERIFICATION_KEY	|The label of the code verification key used in JITP codeverify test.	|
|PKCS11_TEST_LABEL_JITP_CERTIFICATE	|The label of the JITP certificate used in JITP codeverify test.	|
|PKCS11_TEST_LABEL_ROOT_CERTIFICATE	|The label of the code verification key used in JITP codeverify test.	|


FreeRTOS libraries and reference integrations needs at least one of the key function and one of the key provisioning mechanism supported by the PKCS11 APIs. The test must enable at least one of the following configurations:

* At least one of the key function configurations:
    * PKCS11_TEST_RSA_KEY_SUPPORT 
    *  PKCS11_TEST_EC_KEY_SUPPORT 
* At least one of the key provisioning mechanisms:
    *  PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT 
    *  PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT 
    *  PKCS11_TEST_PREPROVISIONED_SUPPORT 


Pre-provisioned device credential test can not be enabled with other provisioning mechanisms. It must be run in the following conditions:

* Enable **PKCS11_TEST_PREPROVISIONED_SUPPORT** and the other provisioning mechanisms must be disabled
* Only one of the key function, **PKCS11_TEST_RSA_KEY_SUPPORT** or **PKCS11_TEST_EC_KEY_SUPPORT**, enabled
* Setup the pre-provisioned key labels according to your key function, including **PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS**, **PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS** and **PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS**. These credentials must exist in the PKCS11 before running the test.

You may need to run the test several times with different configurations if your implementation support pre-provisioned credentials and other provisioning mechanisms.


>Note:
Objects with label **PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS**, **PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS** and **PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS** will be erased during the test if any one of **PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT** and **PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT** is enabled.


## 3. Prerequisites For PKCS11 Test
The PKCS11 tests assume the tested platform already has the following components integrated.
* **The PKCS11 APIs subset implementation**<br>
    The implementation should support the APIs list in this [section](#1-introduction).
* **corePKCS11**<br>
    The utilities in corePKCS11 are used in PKCS11 test. The software based mock implementation is up to developer's implementation choice.
* **MbedTLS**<br>
    MbedTLS is required to verify the result of the PKCS11 implementation.
* **Unity test framework**<br>
    PKCS11 test make use of the Unity test framework. Reference the [website](https://github.com/ThrowTheSwitch/Unity) for integration guide.

## 4. Source Code Organization
The tree only lists the required files to run the PKCS11 test.
```
./FreeRTOS-Libraries-Integration-Tests/
├── config_template
│   ├── test_execution_config_template.h
│   └── test_param_config_template.h
└── src
    ├── common
    │   └── platform_function.h
    ├── pkcs11
    │   ├── core_pkcs11_test.c
    │   ├── core_pkcs11_test.h
    │   ├── ecdsa_test_credentials.h
    │   ├── rsa_test_credentials.h
    │   └── dev_mode_key_provisioning
    │       ├── dev_mode_key_provisioning.c
    │       └── dev_mode_key_provisioning.h
    ├── qualification_test.c
    └── qualification_test.h
```

## 5. Implement PKCS11 Test Application
1. Add [FreeRTOS-Libraries-Integration-Tests](https://github.com/FreeRTOS/FreeRTOS-Libraries-Integration-Tests) as a submodule into your project. It doesn’t matter where the submodule is placed in the project, as long as it can be built.
2. Copy **config_template/test_execution_config_template.h** and **config_template/test_param_config_template.h** to a project location in the build path, and rename them to **test_execution_config.h** and **test_param_config.h**.
3. Include relevant files into the build system. If using CMake, **qualification_test.cmake** and **src/pkcs11_test.cmake** can be used to include relevant files.
4. Implement the [platform functions](https://github.com/FreeRTOS/FreeRTOS-Libraries-Integration-Tests/blob/main/src/common/platform_function.h) required by PKCS11 tests. 
5. Enable the PKCS11 test config, **PKCS11_TEST_ENABLED**, in **test_execution_config.h**.
6. Implement the main function and call the **RunQualificationTest**.

The following is an example test application.

```C
#include "platform_function.h"
#include "qualification_test.h"

FRTestThreadHandle_t FRTest_ThreadCreate( FRTestThreadFunction_t threadFunc, void * pParam )
{
    /* Thread create function for multithreaded test. */
}

int FRTest_ThreadTimedJoin( FRTestThreadHandle_t threadHandle, uint32_t timeoutMs )
{
    /* Thread timed wait function for multithreaded test. */
}

void FRTest_TimeDelay( uint32_t delayMs )
{
    /* Delay function to wait for PKCS11 result. */
}

void * FRTest_MemoryAlloc( size_t size )
{
    /* Malloc function to allocate memory for test. */
}

void FRTest_MemoryFree( void * ptr )
{
    /* Free function to free memory allocated by FRTest_MemoryAlloc function. */
}

void yourMainFunction( void )
{
    RunQualificationTest();
}

```

## 6. Run The PKCS11 Test
### 6.1 Setup the provisioning mechanism and key function
Setup the provisioning mechanism and key function in **test_param_config.h** according to the device capability.

### 6.2 Compile and run the PKCS11 test application
Compile and run the test application in your development environment.
The following is a sample test result log:
```
TEST(Full_PKCS11_StartFinish, PKCS11_StartFinish_FirstTest) PASS
TEST(Full_PKCS11_StartFinish, PKCS11_GetFunctionList) PASS
TEST(Full_PKCS11_StartFinish, PKCS11_InitializeFinalize) PASS
TEST(Full_PKCS11_StartFinish, PKCS11_GetSlotList) PASS
TEST(Full_PKCS11_StartFinish, PKCS11_OpenSessionCloseSession) PASS
TEST(Full_PKCS11_Capabilities, PKCS11_Capabilities) PASS
TEST(Full_PKCS11_NoObject, PKCS11_Digest) PASS
TEST(Full_PKCS11_NoObject, PKCS11_Digest_ErrorConditions) PASS
TEST(Full_PKCS11_NoObject, PKCS11_GenerateRandom) PASS
TEST(Full_PKCS11_NoObject, PKCS11_GenerateRandomMultiThread) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_CreateObject) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_FindObject) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_GetAttributeValue) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_Sign) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_FindObjectMultiThread) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_GetAttributeValueMultiThread) PASS
TEST(Full_PKCS11_RSA, PKCS11_RSA_DestroyObject) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_GenerateKeyPair) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_CreateObject) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_FindObject) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_GetAttributeValue) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_Sign) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_Verify) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_FindObjectMultiThread) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_GetAttributeValueMultiThread) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_SignVerifyMultiThread) PASS
TEST(Full_PKCS11_EC, PKCS11_EC_DestroyObject) PASS

-----------------------
27 Tests 0 Failures 0 Ignored
OK

```


