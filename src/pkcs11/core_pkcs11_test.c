/*
 * FreeRTOS FreeRTOS LTS Qualification Tests preview
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file core_pkcs11_test.c
 * @brief Integration tests for the corePKCS11 implementation.
 */
/* Standard includes. */
#include <stdlib.h>
#include <string.h>

/* corePKCS11 includes. */
#include "core_pki_utils.h"
#include "core_pkcs11.h"

/* Device provisioning. */
#include "aws_dev_mode_key_provisioning.h"

/* Test includes. */
#include "unity_fixture.h"
#include "unity.h"

/* mbedTLS includes. */
#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/pk_internal.h"
#include "mbedtls/oid.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy_poll.h"
#include "mbedtls/x509_crt.h"

/* corePKCS11 test includes. */
#include "core_pkcs11_test.h"

/* Test configuration includes. */
#include "test_execution_config.h"
#include "test_param_config.h"

/*-----------------------------------------------------------*/

/**
 * @brief Number of simultaneous tasks for multithreaded tests.
 *
 * Each task consumes both stack and heap space, which may cause memory allocation
 * failures if too many tasks are created.
 */
#define PKCS11_TEST_MULTI_THREAD_TASK_COUNT    ( 2 )

/**
 * @brief The number of iterations of the test that will run in multithread tests.
 *
 * A single iteration of Signing and Verifying may take up to a minute on some
 * boards. Ensure that PKCS11_TEST_WAIT_THREAD_TIMEOUT_MS is long enough to accommodate
 * all iterations of the loop.
 */
#define PKCS11_TEST_MULTI_THREAD_LOOP_COUNT    ( 10 )

/**
 * @brief All tasks of the SignVerifyRoundTrip_MultitaskLoop test must finish within
 * this timeout, or the test will fail.
 */
#define PKCS11_TEST_WAIT_THREAD_TIMEOUT_MS     ( 1000000UL )

/**
 * @brief The test make use of the unity TEST_PRINTF function to print log. Log function
 * is disabled if not supported.
 */
#ifndef TEST_PRINTF
    #define TEST_PRINTF( ... )
#endif

/**
 * @brief At least one of RSA KEY and EC KEY mechanism must be supported.
 */
#if ( PKCS11_TEST_RSA_KEY_SUPPORT == 0 ) && ( PKCS11_TEST_EC_KEY_SUPPORT == 0 )
    #error "RSA or Elliptic curve keys (or both) must be supported."
#endif

/**
 * @brief At least one of the key provisioning functions must be supported.
 */
#if ( PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT == 0 ) && ( PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT == 0 ) && ( PKCS11_TEST_PREPROVISIONED_SUPPORT == 0 )
    #error "The device must have some mechanism configured to provision the PKCS #11 stack."
#endif

/**
 * @brief Random buffer size for random number generate test.
 */
#define PKCS11_TEST_RAND_BUFFER_SIZE      ( 10 )

/*-----------------------------------------------------------*/

typedef enum CredentialsProvisioned
{
    eNone,                /* Device is not provisioned.  All credentials have been destroyed. */
    eRsaTest,             /* Provisioned using the RSA test credentials located in this file. */
    eEllipticCurveTest,   /* Provisioned using EC test credentials located in this file. */
    eClientCredential,    /* Provisioned using the credentials in aws_clientcredential_keys. */
    eGeneratedEc,         /* Provisioned using elliptic curve generated on device.  Private key unknown.  No corresponding certificate. */
    eGeneratedRsa,
    eDeliberatelyInvalid, /* Provisioned using credentials that are meant to trigger an error condition. */
    eStateUnknown         /* State of the credentials is unknown. */
} CredentialsProvisioned_t;

/* Data structure to store results of multi-thread tests. */
typedef struct MultithreadTaskParams
{
    uint32_t xTaskNumber;
    CK_RV xTestResult;
    void * pvTaskData;
} MultithreadTaskParams_t;

/*-----------------------------------------------------------*/

/**
 * @brief Struct of test parameters filled in by user.
 */
Pkcs11TestParam_t testParam = { 0 };

/* PKCS #11 Globals.
 * These are used to reduce setup and tear down calls, and to
 * prevent memory leaks in the case of TEST_PROTECT() actions being triggered. */
CK_SESSION_HANDLE xGlobalSession = 0;
CK_FUNCTION_LIST_PTR pxGlobalFunctionList = NULL_PTR;
CK_SLOT_ID xGlobalSlotId = 0;
CK_MECHANISM_TYPE xGlobalMechanismType = 0;

/* Model Based Tests (MBT) test group variables. */
CK_OBJECT_HANDLE xGlobalPublicKeyHandle = 0;
CK_OBJECT_HANDLE xGlobalPrivateKeyHandle = 0;
CK_BBOOL xGlobalCkTrue = CK_TRUE;
CK_BBOOL xGlobalCkFalse = CK_FALSE;
CredentialsProvisioned_t xCurrentCredentials = eStateUnknown;

/* PKCS #11 Global Data Containers. */
CK_BYTE rsaHashedMessage[ pkcs11SHA256_DIGEST_LENGTH ] = { 0 };
CK_BYTE ecdsaSignature[ pkcs11RSA_2048_SIGNATURE_LENGTH ] = { 0x00 };
CK_BYTE ecdsaHashedMessage[ pkcs11SHA256_DIGEST_LENGTH ] = { 0xab };

/* Digest test input data. */
static CK_BYTE x896BitInput[] = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

/* Digest test hash data. */
static CK_BYTE xSha256HashOf896BitInput[] =
{
    0xcf, 0x5b, 0x16, 0xa7, 0x78, 0xaf, 0x83, 0x80, 0x03, 0x6c, 0xe5, 0x9e, 0x7b, 0x04, 0x92, 0x37,
    0x0b, 0x24, 0x9b, 0x11, 0xe8, 0xf0, 0x7a, 0x51, 0xaf, 0xac, 0x45, 0x03, 0x7a, 0xfe, 0xe9, 0xd1
};

/* Task parameters for each test thread. */
static MultithreadTaskParams_t xGlobalTaskParams[ PKCS11_TEST_MULTI_THREAD_TASK_COUNT ];

/* The StartFinish test group is for General Purpose,
 * Session, Slot, and Token management functions.
 * These tests do not require provisioning. */
TEST_GROUP( Full_PKCS11_StartFinish );

/* Tests for determing the capabilities of the PKCS #11 module. */
TEST_GROUP( Full_PKCS11_Capabilities );

/* The NoKey test group is for test of cryptographic functionality
 * that do not require keys.  Covers digesting and randomness.
 * These tests do not require provisioning. */
TEST_GROUP( Full_PKCS11_NoObject );

/* The RSA test group is for tests that require RSA keys. */
TEST_GROUP( Full_PKCS11_RSA );

/* The EC test group is for tests that require elliptic curve keys. */
TEST_GROUP( Full_PKCS11_EC );

/*-----------------------------------------------------------*/

/* Test helper function to get the slot ID for testing. This function should be called
 * in the test cases only t cases only. The cryptoki must already be initialized
 * and pxGlobalFunctionList is provided. C_GetSlotList is verified in AFQP_GetSlotList. */
static CK_SLOT_ID prvGetTestSlotId( void )
{
    CK_RV xResult;
    CK_SLOT_ID * pxSlotId = NULL;
    CK_SLOT_ID xSlotId;
    CK_ULONG xSlotCount = 0;

    TEST_ASSERT( pxGlobalFunctionList != NULL );

    /* Get the slot count. */
    xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, NULL, &xSlotCount );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get slot count." );
    TEST_ASSERT_GREATER_THAN_MESSAGE( 0, xSlotCount, "Slot count incorrectly updated." );

    /* Allocate memory to receive the list of slots. */
    pxSlotId = testParam.pMemoryAlloc( sizeof( CK_SLOT_ID ) * ( xSlotCount ) );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pxSlotId, "Failed malloc memory for slot list." );

    /* Call C_GetSlotList again to receive all slots with tokens present. */
    xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, pxSlotId, &xSlotCount );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get slot count." );
    xSlotId = pxSlotId[ PKCS11_TEST_SLOT_NUMBER ];

    testParam.pMemoryFree( pxSlotId );
    return xSlotId;
}

/*-----------------------------------------------------------*/

/* Test helper function to get the function list. This function should be called in
 * test cases only. C_GetFunctionList is verified in AFQP_GetFunctionList test case. */
static CK_FUNCTION_LIST_PTR prvGetFunctionList( void )
{
    CK_RV xResult;
    CK_FUNCTION_LIST_PTR pxFunctionList = NULL;

    xResult = C_GetFunctionList( &pxFunctionList );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "C_GetFunctionList should returns CKR_OK with valid parameter." );

    /* Ensure that pxFunctionList was changed by C_GetFunctionList. */
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pxFunctionList, "C_GetFunctionList should returns valid function list pointer." );

    return pxFunctionList;
}

/*-----------------------------------------------------------*/

/* Test helper function to reset the cryptoki to uninitialized state. */
static CK_RV prvBeforeRunningTests( void )
{
    CK_RV xResult;

    /* Initialize the function list */
    xResult = C_GetFunctionList( &pxGlobalFunctionList );

    if( xResult == CKR_OK )
    {
        /* Close the last session if it was not closed already. */
        pxGlobalFunctionList->C_Finalize( NULL );
    }

    return xResult;
}

/*-----------------------------------------------------------*/

static void prvMultiThreadHelper( void * pvTaskFxnPtr )
{
    uint32_t xTaskNumber;
    int retThreadTimedWait;
    ThreadHandle_t threadHandles[ PKCS11_TEST_MULTI_THREAD_TASK_COUNT ];

    /* Create all the tasks. */
    for( xTaskNumber = 0; xTaskNumber < PKCS11_TEST_MULTI_THREAD_TASK_COUNT; xTaskNumber++ )
    {
        threadHandles[ xTaskNumber ] = testParam.pThreadCreate( pvTaskFxnPtr, &( xGlobalTaskParams[ xTaskNumber ] ) );
        TEST_ASSERT_MESSAGE( threadHandles[ xTaskNumber ] != NULL, "Create thread failed." );
    }

    /* Wait for all tasks to finish. */
    for( xTaskNumber = 0; xTaskNumber < PKCS11_TEST_MULTI_THREAD_TASK_COUNT; xTaskNumber++ )
    {
        retThreadTimedWait = testParam.pThreadTimedWait( threadHandles[ xTaskNumber ], PKCS11_TEST_WAIT_THREAD_TIMEOUT_MS );

        if( retThreadTimedWait != 0 )
        {
            TEST_PRINTF( "Waiting for task %u to finish in multi-threaded test failed %d.\r\n",
                         xTaskNumber, retThreadTimedWait );
        }
    }

    /* Check the tasks' results. */
    if( TEST_PROTECT() )
    {
        for( xTaskNumber = 0; xTaskNumber < PKCS11_TEST_MULTI_THREAD_TASK_COUNT; xTaskNumber++ )
        {
            if( xGlobalTaskParams[ xTaskNumber ].xTestResult != 0 )
            {
                TEST_PRINTF( "Multi thread task %d returned failure.\r\n",
                             xGlobalTaskParams[ xTaskNumber ].xTaskNumber );
                TEST_FAIL();
            }
        }
    }
}

/*-----------------------------------------------------------*/

/* Thread function of generateRandomMultiThread test. */
static void prvGenerateRandomMultiThreadTask( void * pvParameters )
{
    MultithreadTaskParams_t * pxMultiTaskParam = pvParameters;
    uint32_t xCount;
    CK_RV xResult;
    CK_BYTE xRandomData[ PKCS11_TEST_RAND_BUFFER_SIZE ];
    CK_SESSION_HANDLE xSession;

    memcpy( &xSession, pxMultiTaskParam->pvTaskData, sizeof( CK_SESSION_HANDLE ) );

    for( xCount = 0; xCount < PKCS11_TEST_MULTI_THREAD_LOOP_COUNT; xCount++ )
    {
        xResult = pxGlobalFunctionList->C_GenerateRandom( xSession,
                                                          xRandomData,
                                                          sizeof( xRandomData ) );

        if( xResult != CKR_OK )
        {
            TEST_PRINTF( "GenerateRandom multi-thread task failed.  Error: %ld \r\n", xResult );
            break;
        }
    }

    /* Report the result of the loop. */
    pxMultiTaskParam->xTestResult = xResult;
}

/*--------------------------------------------------------*/
/*-------------- StartFinish Tests ---------------------- */
/*--------------------------------------------------------*/

TEST_SETUP( Full_PKCS11_StartFinish )
{
}

/*-----------------------------------------------------------*/

TEST_TEAR_DOWN( Full_PKCS11_StartFinish )
{
}

/*-----------------------------------------------------------*/

TEST_GROUP_RUNNER( Full_PKCS11_StartFinish )
{
    RUN_TEST_CASE( Full_PKCS11_StartFinish, AFQP_GetFunctionList );
    RUN_TEST_CASE( Full_PKCS11_StartFinish, AFQP_InitializeFinalize );
    RUN_TEST_CASE( Full_PKCS11_StartFinish, AFQP_GetSlotList );
    RUN_TEST_CASE( Full_PKCS11_StartFinish, AFQP_OpenSessionCloseSession );
}

/*-----------------------------------------------------------*/

/* C_GetFunctionList is the only Cryptoki function which an application may
 * call before calling C_Initialize. It is tested as first test. */
TEST( Full_PKCS11_StartFinish, AFQP_GetFunctionList )
{
    CK_FUNCTION_LIST_PTR pxFunctionList = NULL;
    CK_RV xResult;

    /* Call the C_GetFunctionList with NULL parameters. */
    xResult = C_GetFunctionList( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_ARGUMENTS_BAD, xResult, "CKR_ARGUMENTS_BAD should be returned if C_GetFunctionList is called with NULL pointer." );

    /* Call the C_GetFunctionList with valid parameters. */
    xResult = C_GetFunctionList( &pxFunctionList );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "C_GetFunctionList should returns CKR_OK with valid parameter." );

    /* Ensure that pxFunctionList was changed by C_GetFunctionList. */
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pxFunctionList, "C_GetFunctionList should returns valid function list pointer." );
}

/*-----------------------------------------------------------*/

/* C_Initialize initialize the Cryptoki library. C_Finalize is called to indicate
 * that an application is finished with the Cryptoki library. They are required by
 * other PKCS11 APIs. This test validate the implementaion of C_Initialize and C_Finalize
 * with valid/invalid parameters and function call order. */
TEST( Full_PKCS11_StartFinish, AFQP_InitializeFinalize )
{
    CK_FUNCTION_LIST_PTR pxFunctionList = NULL;
    CK_RV xResult;

    xResult = C_GetFunctionList( &pxFunctionList );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get function list." );
    TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pxFunctionList, "Invalid function list pointer." );

    xResult = xInitializePKCS11();
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to initialize PKCS #11 module." );

    if( TEST_PROTECT() )
    {
        /* Call initialize a second time.  Since this call may be made many times,
         * it is important that PKCS #11 implementations be tolerant of multiple calls. */
        xResult = xInitializePKCS11();
        TEST_ASSERT_EQUAL_MESSAGE( CKR_CRYPTOKI_ALREADY_INITIALIZED, xResult, "Second PKCS #11 module initialization. " );

        /* C_Finalize should fail if pReserved isn't NULL. */
        xResult = pxFunctionList->C_Finalize( ( CK_VOID_PTR ) 0x1234 );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_ARGUMENTS_BAD, xResult, "Negative Test: Finalize with invalid argument." );
    }

    /* C_Finalize with pReserved is NULL should return CKR_OK if C_Initalize is called successfully before. */
    xResult = pxFunctionList->C_Finalize( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Finalize failed." );

    /* Call C_Finalize a second time. Since C_Finalize may be called multiple times,
     * it is important that the PKCS #11 module is tolerant of multiple calls. */
    xResult = pxFunctionList->C_Finalize( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_CRYPTOKI_NOT_INITIALIZED, xResult, "Second PKCS #11 finalization failed." );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_StartFinish, AFQP_GetSlotList )
{
    CK_RV xResult;
    CK_SLOT_ID * pxSlotId = NULL;
    CK_ULONG xSlotCount = 0;
    CK_ULONG xExtraSlotCount = 0;

    /* Get the function list. */
    pxGlobalFunctionList = prvGetFunctionList();

    xResult = xInitializePKCS11();
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to initialize PKCS #11 module." );

    if( TEST_PROTECT() )
    {
        /* Happy path test. */
        /* When a NULL slot pointer is passed in, the number of slots should be updated. */
        xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, NULL, &xSlotCount );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get slot count." );
        TEST_ASSERT_GREATER_THAN_MESSAGE( 0, xSlotCount, "Slot count incorrectly updated." );

        /* Allocate memory to receive the list of slots, plus one extra. */
        pxSlotId = testParam.pMemoryAlloc( sizeof( CK_SLOT_ID ) * ( xSlotCount + 1 ) );
        TEST_ASSERT_NOT_EQUAL_MESSAGE( NULL, pxSlotId, "Failed malloc memory for slot list." );

        /* Call C_GetSlotList again to receive all slots with tokens present. */
        xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, pxSlotId, &xSlotCount );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get slot count." );

        /* Off the happy path. */
        /* Make sure that number of slots returned is updated when extra buffer room exists. */
        xExtraSlotCount = xSlotCount + 1;
        xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, pxSlotId, &xExtraSlotCount );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to get slot list." );
        TEST_ASSERT_EQUAL_MESSAGE( xSlotCount, xExtraSlotCount, "Failed to update the number of slots." );

        /* Claim that the buffer to receive slots is too small. */
        xSlotCount = 0;
        xResult = pxGlobalFunctionList->C_GetSlotList( CK_TRUE, pxSlotId, &xSlotCount );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_BUFFER_TOO_SMALL, xResult, "Negative Test: Improper handling of too-small slot buffer." );
    }

    if( pxSlotId != NULL )
    {
        testParam.pMemoryFree( pxSlotId );
    }

    xResult = pxGlobalFunctionList->C_Finalize( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Finalize failed." );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_StartFinish, AFQP_OpenSessionCloseSession )
{
    CK_SLOT_ID_PTR pxSlotId = NULL;
    CK_SLOT_ID xSlotId = 0;
    CK_ULONG xSlotCount = 0;
    CK_SESSION_HANDLE xSession = 0;
    CK_BBOOL xSessionOpen = CK_FALSE;
    CK_RV xResult = CKR_OK;

    /* Get function list. */
    pxGlobalFunctionList = prvGetFunctionList();

    /* Initialize PKCS11. */
    xResult = xInitializePKCS11();
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to initialize PKCS #11 module." );

    if( TEST_PROTECT() )
    {
        /* Get test slot ID. */
        xSlotId = prvGetTestSlotId();

        /* Open session with valid parameters should return CKR_OK. */
        xResult = pxGlobalFunctionList->C_OpenSession( xSlotId,
                                                       CKF_SERIAL_SESSION, /* This flag is mandatory for PKCS #11 legacy reasons. */
                                                       NULL,               /* Application defined pointer. */
                                                       NULL,               /* Callback function. */
                                                       &xSession );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to open session." );
        xSessionOpen = CK_TRUE;
    }

    if( xSessionOpen == CK_TRUE )
    {
        xResult = pxGlobalFunctionList->C_CloseSession( xSession );
        TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to close session." );
    }

    pxGlobalFunctionList->C_Finalize( NULL );

    /* Negative tests. */
    /* Try to open a session without having initialized the module. */
    xResult = pxGlobalFunctionList->C_OpenSession( xSlotId,
                                                   CKF_SERIAL_SESSION, /* This flag is mandatory for PKCS #11 legacy reasons. */
                                                   NULL,               /* Application defined pointer. */
                                                   NULL,               /* Callback function. */
                                                   &xSession );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_CRYPTOKI_NOT_INITIALIZED, xResult,
                               "Negative Test: Opened a session before initializing module." );
}

/*--------------------------------------------------------*/
/*-------------- Capabilities Tests --------------------- */
/*--------------------------------------------------------*/

TEST_SETUP( Full_PKCS11_Capabilities )
{
    CK_RV xResult;

    xResult = xInitializePKCS11();
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to initialize PKCS #11 module." );
    xResult = xInitializePkcs11Session( &xGlobalSession );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to open PKCS #11 session." );
}

/*-----------------------------------------------------------*/

TEST_TEAR_DOWN( Full_PKCS11_Capabilities )
{
    CK_RV xResult;

    xResult = pxGlobalFunctionList->C_CloseSession( xGlobalSession );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to close session." );
    xResult = pxGlobalFunctionList->C_Finalize( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to finalize session." );
}

/*-----------------------------------------------------------*/

TEST_GROUP_RUNNER( Full_PKCS11_Capabilities )
{
    /* Reset the cryptoki to uninitialize state. */
    prvBeforeRunningTests();

    RUN_TEST_CASE( Full_PKCS11_Capabilities, AFQP_Capabilities );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_Capabilities, AFQP_Capabilities )
{
    CK_RV xResult = 0;
    CK_SLOT_ID xSlotId;
    CK_MECHANISM_INFO MechanismInfo = { 0 };
    CK_BBOOL xSupportsKeyGen = CK_FALSE;

    /* Determine the number of slots. */
    xSlotId = prvGetTestSlotId();

    /* Check for RSA PKCS #1 signing support. */
    xResult = pxGlobalFunctionList->C_GetMechanismInfo( xSlotId, CKM_RSA_PKCS, &MechanismInfo );
    TEST_ASSERT_TRUE_MESSAGE( ( CKR_OK == xResult || CKR_MECHANISM_INVALID == xResult ),
                              "C_GetMechanismInfo CKM_RSA_PKCS returns unexpected value." );

    if( CKR_OK == xResult )
    {
        TEST_ASSERT_TRUE( 0 != ( CKF_SIGN & MechanismInfo.flags ) );

        TEST_ASSERT_TRUE( MechanismInfo.ulMaxKeySize >= pkcs11RSA_2048_MODULUS_BITS &&
                          MechanismInfo.ulMinKeySize <= pkcs11RSA_2048_MODULUS_BITS );

        /* Check for pre-padded signature verification support. This is required
         * for round-trip testing. */
        xResult = pxGlobalFunctionList->C_GetMechanismInfo( xSlotId, CKM_RSA_X_509, &MechanismInfo );
        TEST_ASSERT_TRUE( CKR_OK == xResult );

        TEST_ASSERT_TRUE( 0 != ( CKF_VERIFY & MechanismInfo.flags ) );

        TEST_ASSERT_TRUE( MechanismInfo.ulMaxKeySize >= pkcs11RSA_2048_MODULUS_BITS &&
                          MechanismInfo.ulMinKeySize <= pkcs11RSA_2048_MODULUS_BITS );

        /* Check consistency with static configuration. */
        #if ( 0 == PKCS11_TEST_RSA_KEY_SUPPORT )
            TEST_FAIL_MESSAGE( "Static and runtime configuration for key generation support are inconsistent." );
        #endif

        TEST_PRINTF( "%s", "The PKCS #11 module supports RSA signing.\r\n" );
    }

    /* Check for ECDSA support, if applicable. */
    xResult = pxGlobalFunctionList->C_GetMechanismInfo( xSlotId, CKM_ECDSA, &MechanismInfo );
    TEST_ASSERT_TRUE( CKR_OK == xResult || CKR_MECHANISM_INVALID == xResult );

    if( CKR_OK == xResult )
    {
        TEST_ASSERT_TRUE( 0 != ( ( CKF_SIGN | CKF_VERIFY ) & MechanismInfo.flags ) );

        TEST_ASSERT_TRUE( MechanismInfo.ulMaxKeySize >= pkcs11ECDSA_P256_KEY_BITS &&
                          MechanismInfo.ulMinKeySize <= pkcs11ECDSA_P256_KEY_BITS );

        /* Check consistency with static configuration. */
        #if ( 0 == PKCS11_TEST_EC_KEY_SUPPORT )
            TEST_FAIL_MESSAGE( "Static and runtime configuration for key generation support are inconsistent." );
        #endif

        TEST_PRINTF( "%s", "The PKCS #11 module supports ECDSA.\r\n" );
    }

    #if ( PKCS11_TEST_PREPROVISIONED_SUPPORT != 1 )
        /* Check for elliptic-curve key generation support. */
        xResult = pxGlobalFunctionList->C_GetMechanismInfo( xSlotId, CKM_EC_KEY_PAIR_GEN, &MechanismInfo );
        TEST_ASSERT_TRUE( CKR_OK == xResult || CKR_MECHANISM_INVALID == xResult );

        if( CKR_OK == xResult )
        {
            TEST_ASSERT_TRUE( 0 != ( CKF_GENERATE_KEY_PAIR & MechanismInfo.flags ) );

            TEST_ASSERT_TRUE( MechanismInfo.ulMaxKeySize >= pkcs11ECDSA_P256_KEY_BITS &&
                              MechanismInfo.ulMinKeySize <= pkcs11ECDSA_P256_KEY_BITS );

            xSupportsKeyGen = CK_TRUE;
            TEST_PRINTF( "%s", "The PKCS #11 module supports elliptic-curve key generation.\r\n" );
        }

        /* Check for consistency between static configuration and runtime key
         * generation settings. */
        if( CK_TRUE == xSupportsKeyGen )
        {
            #if ( 0 == PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT )
                TEST_FAIL_MESSAGE( "Static and runtime configuration for key generation support are inconsistent." );
            #endif
        }
        else
        {
            #if ( 1 == PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT )
                TEST_FAIL_MESSAGE( "Static and runtime configuration for key generation support are inconsistent." );
            #endif
        }
    #endif /* if ( PKCS11_TEST_PREPROVISIONED_SUPPORT != 1 ) */

    /* SHA-256 support is required. */
    xResult = pxGlobalFunctionList->C_GetMechanismInfo( xSlotId, CKM_SHA256, &MechanismInfo );
    TEST_ASSERT_TRUE( CKR_OK == xResult );
    TEST_ASSERT_TRUE( 0 != ( CKF_DIGEST & MechanismInfo.flags ) );

    /* Report on static configuration for key import support. */
    #if ( 1 == PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT )
        TEST_PRINTF( "%s", "The PKCS #11 module supports private key import.\r\n" );
    #endif
}

/*--------------------------------------------------------*/
/*-------------- No Object Tests ------------------------ */
/*--------------------------------------------------------*/

TEST_SETUP( Full_PKCS11_NoObject )
{
    CK_RV xResult;

    xResult = xInitializePKCS11();
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to initialize PKCS #11 module." );

    xResult = xInitializePkcs11Session( &xGlobalSession );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to open PKCS #11 session." );
}

/*-----------------------------------------------------------*/

TEST_TEAR_DOWN( Full_PKCS11_NoObject )
{
    CK_RV xResult;

    xResult = pxGlobalFunctionList->C_CloseSession( xGlobalSession );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to close session." );

    xResult = pxGlobalFunctionList->C_Finalize( NULL );
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "Failed to finalize session." );
}

/*-----------------------------------------------------------*/

TEST_GROUP_RUNNER( Full_PKCS11_NoObject )
{
    /* Reset the cryptoki to uninitialize state. */
    prvBeforeRunningTests();

    RUN_TEST_CASE( Full_PKCS11_NoObject, AFQP_Digest );
    RUN_TEST_CASE( Full_PKCS11_NoObject, AFQP_Digest_ErrorConditions );
    RUN_TEST_CASE( Full_PKCS11_NoObject, AFQP_GenerateRandom );
    RUN_TEST_CASE( Full_PKCS11_NoObject, AFQP_GenerateRandomMultiThread );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_NoObject, AFQP_Digest )
{
    CK_RV xResult = 0;

    CK_MECHANISM xDigestMechanism;

    CK_BYTE xDigestResult[ pkcs11SHA256_DIGEST_LENGTH ] = { 0 };
    CK_ULONG xDigestLength = 0;

    /* Hash with SHA256 */
    xDigestMechanism.mechanism = CKM_SHA256;

    xResult = pxGlobalFunctionList->C_DigestInit( xGlobalSession, &xDigestMechanism );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );

    /* Subtract one because this hash was performed on the characters without the null terminator. */
    xResult = pxGlobalFunctionList->C_DigestUpdate( xGlobalSession, x896BitInput, sizeof( x896BitInput ) - 1 );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );

    /* Call C_DigestFinal on a NULL buffer to get the buffer length required. */
    xResult = pxGlobalFunctionList->C_DigestFinal( xGlobalSession, NULL, &xDigestLength );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );
    TEST_ASSERT_EQUAL( pkcs11SHA256_DIGEST_LENGTH, xDigestLength );

    xResult = pxGlobalFunctionList->C_DigestFinal( xGlobalSession, xDigestResult, &xDigestLength );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );
    TEST_ASSERT_EQUAL_INT8_ARRAY( xSha256HashOf896BitInput, xDigestResult, pkcs11SHA256_DIGEST_LENGTH );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_NoObject, AFQP_Digest_ErrorConditions )
{
    CK_RV xResult = 0;
    CK_MECHANISM xDigestMechanism;
    CK_BYTE xDigestResult[ pkcs11SHA256_DIGEST_LENGTH ] = { 0 };
    CK_ULONG xDigestLength = 0;

    /* Make sure that no NULL pointers in functions to be called in this test. */
    TEST_ASSERT_NOT_EQUAL( NULL, pxGlobalFunctionList->C_DigestInit );
    TEST_ASSERT_NOT_EQUAL( NULL, pxGlobalFunctionList->C_DigestUpdate );
    TEST_ASSERT_NOT_EQUAL( NULL, pxGlobalFunctionList->C_DigestFinal );

    /* Invalid hash mechanism. */
    xDigestMechanism.mechanism = 0x253; /*253 doesn't correspond to anything. */

    xResult = pxGlobalFunctionList->C_DigestInit( xGlobalSession, &xDigestMechanism );
    TEST_ASSERT_EQUAL( CKR_MECHANISM_INVALID, xResult );

    /* Null Session. */
    xDigestMechanism.mechanism = CKM_SHA256;
    xResult = pxGlobalFunctionList->C_DigestInit( ( CK_SESSION_HANDLE ) NULL, &xDigestMechanism );
    TEST_ASSERT_EQUAL( CKR_SESSION_HANDLE_INVALID, xResult );

    /* Make sure that digest update fails if DigestInit did not succeed. */
    xResult = pxGlobalFunctionList->C_DigestUpdate( xGlobalSession, x896BitInput, sizeof( x896BitInput ) - 1 );
    TEST_ASSERT_EQUAL( CKR_OPERATION_NOT_INITIALIZED, xResult );

    /* Initialize the session properly. */
    xResult = pxGlobalFunctionList->C_DigestInit( xGlobalSession, &xDigestMechanism );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );

    /* Try to update digest with a NULL session handle. */
    xResult = pxGlobalFunctionList->C_DigestUpdate( ( CK_SESSION_HANDLE ) NULL, x896BitInput, sizeof( x896BitInput ) - 1 );
    TEST_ASSERT_EQUAL( CKR_SESSION_HANDLE_INVALID, xResult );

    /* DigestUpdate correctly.  Note that digest is not terminated because we didn't tell the session handle last time. */
    xResult = pxGlobalFunctionList->C_DigestUpdate( xGlobalSession, x896BitInput, sizeof( x896BitInput ) - 1 );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );

    /* Call C_DigestFinal on a buffer that is too small. */
    xDigestLength = pkcs11SHA256_DIGEST_LENGTH - 1;
    xResult = pxGlobalFunctionList->C_DigestFinal( xGlobalSession, xDigestResult, &xDigestLength );
    TEST_ASSERT_EQUAL( CKR_BUFFER_TOO_SMALL, xResult );

    /* Call C_DigestFinal on a NULL session handle. */
    xDigestLength = pkcs11SHA256_DIGEST_LENGTH;
    xResult = pxGlobalFunctionList->C_DigestFinal( ( CK_SESSION_HANDLE ) NULL, xDigestResult, &xDigestLength );
    TEST_ASSERT_EQUAL( CKR_SESSION_HANDLE_INVALID, xResult );

    /* Call C_DigestFinal on a proper buffer size. Note that Digest is not terminated if error is "buffer too small" or if session handle wasn't present. */
    xDigestLength = pkcs11SHA256_DIGEST_LENGTH;
    xResult = pxGlobalFunctionList->C_DigestFinal( xGlobalSession, xDigestResult, &xDigestLength );
    TEST_ASSERT_EQUAL( CKR_OK, xResult );
    TEST_ASSERT_EQUAL_INT8_ARRAY( xSha256HashOf896BitInput, xDigestResult, pkcs11SHA256_DIGEST_LENGTH );

    /* Call C_DigestUpdate after the digest operation has been completed. */
    xResult = pxGlobalFunctionList->C_DigestUpdate( xGlobalSession, x896BitInput, sizeof( x896BitInput ) - 1 );
    TEST_ASSERT_EQUAL( CKR_OPERATION_NOT_INITIALIZED, xResult );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_NoObject, AFQP_GenerateRandom )
{
    CK_RV xResult = 0;
    uint32_t xSameSession = 0;
    uint32_t xDifferentSessions = 0;
    int i;

    CK_BYTE xBuf1[ PKCS11_TEST_RAND_BUFFER_SIZE ];
    CK_BYTE xBuf2[ PKCS11_TEST_RAND_BUFFER_SIZE ];
    CK_BYTE xBuf3[ PKCS11_TEST_RAND_BUFFER_SIZE ];

    /* Generate random bytes twice. */
    if( CKR_OK == xResult )
    {
        xResult = pxGlobalFunctionList->C_GenerateRandom( xGlobalSession, xBuf1, PKCS11_TEST_RAND_BUFFER_SIZE );
    }

    if( CKR_OK == xResult )
    {
        xResult = pxGlobalFunctionList->C_GenerateRandom( xGlobalSession, xBuf2, PKCS11_TEST_RAND_BUFFER_SIZE );
    }

    if( CKR_OK == xResult )
    {
        /* Close the session and PKCS #11 module */
        if( NULL != pxGlobalFunctionList )
        {
            ( void ) pxGlobalFunctionList->C_CloseSession( xGlobalSession );
        }
    }

    /* Re-open PKCS #11 session. */
    xResult = xInitializePkcs11Session( &xGlobalSession );

    if( CKR_OK == xResult )
    {
        xResult = pxGlobalFunctionList->C_GenerateRandom( xGlobalSession, xBuf3, PKCS11_TEST_RAND_BUFFER_SIZE );
    }

    /* Check that the result is good. */
    TEST_ASSERT_EQUAL_MESSAGE( CKR_OK, xResult, "C_GenerateRandom returns unexpected value." );

    /* Check that the random bytes generated within session
     * and between initializations of PKCS module are not the same. */
    for( i = 0; i < PKCS11_TEST_RAND_BUFFER_SIZE; i++ )
    {
        if( xBuf1[ i ] == xBuf2[ i ] )
        {
            xSameSession++;
        }

        if( xBuf1[ i ] == xBuf3[ i ] )
        {
            xDifferentSessions++;
        }
    }

    if( ( xSameSession > 1 ) || ( xDifferentSessions > 1 ) )
    {
        TEST_PRINTF( "First Random Bytes: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                     xBuf1[ 0 ], xBuf1[ 1 ], xBuf1[ 2 ], xBuf1[ 3 ], xBuf1[ 4 ],
                     xBuf1[ 5 ], xBuf1[ 6 ], xBuf1[ 7 ], xBuf1[ 8 ], xBuf1[ 9 ] );

        TEST_PRINTF( "Second Set of Random Bytes: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                     xBuf2[ 0 ], xBuf2[ 1 ], xBuf2[ 2 ], xBuf2[ 3 ], xBuf2[ 4 ],
                     xBuf2[ 5 ], xBuf2[ 6 ], xBuf2[ 7 ], xBuf2[ 8 ], xBuf2[ 9 ] );

        TEST_PRINTF( "Third Set of Random Bytes:  %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                     xBuf3[ 0 ], xBuf3[ 1 ], xBuf3[ 2 ], xBuf3[ 3 ], xBuf3[ 4 ],
                     xBuf3[ 5 ], xBuf3[ 6 ], xBuf3[ 7 ], xBuf3[ 8 ], xBuf3[ 9 ] );
    }

    TEST_ASSERT_LESS_THAN( 2, xSameSession );
    TEST_ASSERT_LESS_THAN( 2, xDifferentSessions );
}

/*-----------------------------------------------------------*/

TEST( Full_PKCS11_NoObject, AFQP_GenerateRandomMultiThread )
{
    uint32_t xTaskNumber;
    CK_SESSION_HANDLE xSessionHandle[ PKCS11_TEST_MULTI_THREAD_TASK_COUNT ];

    for( xTaskNumber = 0; xTaskNumber < PKCS11_TEST_MULTI_THREAD_TASK_COUNT; xTaskNumber++ )
    {
        xInitializePkcs11Session( &xSessionHandle[ xTaskNumber ] );
        xGlobalTaskParams[ xTaskNumber ].pvTaskData = &xSessionHandle[ xTaskNumber ];
    }

    prvMultiThreadHelper( ( void * ) prvGenerateRandomMultiThreadTask );

    for( xTaskNumber = 0; xTaskNumber < PKCS11_TEST_MULTI_THREAD_TASK_COUNT; xTaskNumber++ )
    {
        pxGlobalFunctionList->C_CloseSession( xSessionHandle[ xTaskNumber ] );
    }
}

/*-----------------------------------------------------------*/

int RunPkcs11Test( void )
{
    int status = -1;

    #if ( CORE_PKCS11_TEST_ENABLED == 1 )
        SetupPkcs11TestParam( &testParam );

        /* Initialize unity. */
        UnityFixture.Verbose = 1;
        UnityFixture.GroupFilter = 0;
        UnityFixture.NameFilter = 0;
        UnityFixture.RepeatCount = 1;
        UNITY_BEGIN();

        /* Run the test group. */
        /* Basic general purpose and slot token management tests. */
        RUN_TEST_GROUP( Full_PKCS11_StartFinish );

        /* Cryptoki capabilities test. */
        RUN_TEST_GROUP( Full_PKCS11_Capabilities );

        /* Digest and random number generate test. No object related operation
         * is required in this group. */
        RUN_TEST_GROUP( Full_PKCS11_NoObject );

        status = UNITY_END();
    #endif /* if ( CORE_PKCS11_TEST_ENABLED == 1 ) */

    return status;
}

/*-----------------------------------------------------------*/
