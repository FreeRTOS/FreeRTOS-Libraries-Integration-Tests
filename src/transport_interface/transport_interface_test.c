/*
 * FreeRTOS Integration Toolkit preview
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * @file transport_interface_test.c
 * @brief Integration tests for the transport interface test implementation.
 */

/* Standard header includes. */
#include <string.h>

/* Include for init and de-init functions. */
#include "transport_interface_test.h"

/* Include for test parameter and execution configs. */
#include "test_execution_config.h"
#include "test_param_config.h"

/* Include for Unity framework. */
#include "unity.h"
#include "unity_fixture.h"

/*-----------------------------------------------------------*/

/**
 * @brief Transport Interface test buffer length.
 *
 * Length of the test buffer used in tests. Guard buffers are placed before and
 * after the topic buffer to verify that transport interface APIs do not write
 * out of bounds. The memory layout is:
 *
 *     +--------------+-------------------------------+------------+
 *     |    Guard     |    Writable Topic Buffer      |   Guard    |
 *     +--------------+-------------------------------+------------+
 *
 * Both guard buffers are filled with a known pattern before each test and are
 * verified to remain unchanged after each test.
 */
#define TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH    ( 32U )
#define TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH        ( 2048U )
#define TRANSPORT_TEST_BUFFER_SUFFIX_GUARD_LENGTH    ( 32U )
#define TRANSPORT_TEST_BUFFER_TOTAL_LENGTH        \
    ( TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH + \
      TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH +     \
      TRANSPORT_TEST_BUFFER_SUFFIX_GUARD_LENGTH )

/**
 * @brief Transport Interface test buffer guard pattern.
 */
#define TRANSPORT_TEST_BUFFER_GUARD_PATTERN        ( 0xA5 )

/**
 * @brief Transport Interface send receive retry count.
 */
#define TRANSPORT_TEST_SEND_RECEIVE_RETRY_COUNT    ( 10U )

/**
 * @brief Transport Interface delay in milliseconds.
 *
 * The Delay time for the transport interface test to wait for the data echo-ed
 * back from transport network.
 */
#define TRANSPORT_TEST_DELAY_MS                    ( 150U )

/**
 * @brief Echo server disconnect command.
 *
 * The command to inform echo server to disconnect the test connection.
 */
#define TRANSPORT_TEST_DISCONNECT_COMMAND          "DISCONNECT"

/**
 * @brief Transport Interface disconnect delay in milliseconds.
 *
 * The Delay time for the transport interface test to wait for the send operation
 * and response from the network. The delay time should be long enough to cover different
 * network environment.
 */
#define TRANSPORT_TEST_NETWORK_DELAY_MS            ( 3000U )

/**
 * @brief Number of simultaneous tasks for multithreaded tests.
 */
#define TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT     ( 2U )

/**
 * @brief The thread parameter index used in non-multithreaded test.
 */
#define TRANSPORT_TEST_INDEX                       ( 0U )

/**
 * @brief Timeout to wait thread function finish the test.
 */
#ifndef TRANSPORT_TEST_WAIT_THREAD_TIMEOUT_MS
    #define TRANSPORT_TEST_WAIT_THREAD_TIMEOUT_MS    ( 1000000U )
#endif

/**
 * @brief Timeout to wait no data to receive test thread.
 */
#define TRANSPORT_TEST_WAIT_THREAD_RECEIVE_TIMEOUT_MS    ( 180000U )

/**
 * @brief Print progress in SendRecvCompare test.
 *
 * This debug function should be disabled if testing with IDT.
 */
#ifndef TRANSPORT_TEST_PRINT_DEBUG_PROGRESS
    #define TRANSPORT_TEST_PRINT_DEBUG_PROGRESS    ( 0 )
#endif

/*-----------------------------------------------------------*/

typedef struct threadParameter
{
    NetworkContext_t * pNetworkContext;
    uint8_t transportTestBuffer[ TRANSPORT_TEST_BUFFER_TOTAL_LENGTH ];
    bool stopFlag;
} threadParameter_t;

/*-----------------------------------------------------------*/

/**
 * @brief Struct of test parameters filled in by user.
 */
static TransportTestParam_t testParam = { 0 };

/**
 * @brief Info of the server to connect to for tests.
 */
static TestHostInfo_t testHostInfo = { 0 };

/**
 * @brief Transport Interface used in test cases.
 */
static TransportInterface_t * pTestTransport = NULL;

/**
 * @brief Thread paramter to hold the network context and test buffer.
 */
static threadParameter_t threadParameter[ TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT ];

/**
 * @brief Test group for transport interface test.
 */
TEST_GROUP( Full_TransportInterfaceTest );

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the test data with 0,1,...,255,0,1,...
 */
static void prvInitializeTestData( uint8_t * pTransportTestBuffer,
                                   uint32_t testSize )
{
    uint32_t i = 0U;

    for( i = 0U; i < testSize; i++ )
    {
        pTransportTestBuffer[ i ] = ( uint8_t ) i;
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Verify the data received and buffer after testSize should remain unchanged.
 *
 * The received data is verified with the pattern initialized in prvInitializeTestData.
 * The test buffer after testSize should remain unchanged.
 */
static void prvVerifyTestData( uint8_t * pTransportTestBuffer,
                               uint32_t testSize,
                               uint32_t maxBufferSize )
{
    uint32_t i = 0U;

    /* Check the data received is correct. */
    for( i = 0U; i < testSize; i++ )
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE( ( ( uint8_t ) i ), pTransportTestBuffer[ i ],
                                         "Received data is not the same as expected." );
    }

    /* Check the buffer after testSize is unchanged. */
    if( testSize < maxBufferSize )
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE( pTransportTestBuffer[ i ], TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         "Buffer after testSize should not be altered." );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Send the data over transport network with retry.
 *
 * The send API may return less bytes then requested. If the transport send function
 * returns zero, it should represent the send operation can be retired by calling
 * the API function. The retry operation is handled in this function.
 */
static void prvTransportSendData( TransportInterface_t * pTransport,
                                  NetworkContext_t * pNetworkContext,
                                  uint8_t * pTransportTestBuffer,
                                  uint32_t sendSize )
{
    uint32_t transferTotal = 0U;
    int32_t transportResult = 0;
    uint32_t i = 0U;

    for( i = 0U; i < TRANSPORT_TEST_SEND_RECEIVE_RETRY_COUNT; i++ )
    {
        transportResult = pTransport->send( pNetworkContext,
                                            &pTransportTestBuffer[ transferTotal ],
                                            sendSize - transferTotal );
        /* Send should not have any error. */
        TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE( 0, transportResult,
                                                    "Transport send data should not have any error." );
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE( ( sendSize - transferTotal ), ( uint32_t ) transportResult,
                                                  "More data is sent than expected." );
        transferTotal = transferTotal + ( uint32_t ) transportResult;

        if( sendSize == transferTotal )
        {
            /* All the data is sent. Break the retry loop. */
            break;
        }
    }

    /* Check if all the data is sent. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( sendSize, transferTotal, "Fail to send all the data expected." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Receive the data from transport network with retry.
 *
 * The receive API may return less btyes then requested. If the transport receive
 * function returns zero, it should represent the read operation can be retried
 * by calling the API function. The retry operation is handled in this function.
 */
static void prvTransportRecvData( TransportInterface_t * pTransport,
                                  NetworkContext_t * pNetworkContext,
                                  uint8_t * pTransportTestBuffer,
                                  uint32_t recvSize )
{
    uint32_t transferTotal = 0U;
    int32_t transportResult = 0;
    uint32_t i = 0U;

    /* Initialize the receive buffer with TRANSPORT_TEST_BUFFER_GUARD_PATTERN. */
    memset( &( pTransportTestBuffer[ 0 ] ), TRANSPORT_TEST_BUFFER_GUARD_PATTERN, recvSize );

    for( i = 0U; i < TRANSPORT_TEST_SEND_RECEIVE_RETRY_COUNT; i++ )
    {
        transportResult = pTransport->recv( pNetworkContext,
                                            &pTransportTestBuffer[ transferTotal ],
                                            recvSize - transferTotal );

        /* Receive should not have any error. */
        TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE( 0, transportResult,
                                                    "Transport receive data should not have any error." );
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE( ( recvSize - transferTotal ), ( uint32_t ) transportResult,
                                                  "More data is received than expected." );
        transferTotal = transferTotal + transportResult;

        if( recvSize > transferTotal )
        {
            /* Check the remain buffer after receive. */
            TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                                 &( pTransportTestBuffer[ transferTotal ] ),
                                                 ( recvSize - transferTotal ),
                                                 "Buffer after received data should not be altered." );
        }
        else
        {
            /* This is test testSize == transferTotal case. All the data is received.
             * Break the retry loop. */
            break;
        }

        /* Delay to wait for the test data from the transport network. */
        testParam.pThreadDelay( TRANSPORT_TEST_DELAY_MS );
    }

    /* Check if all the data is recevied. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( recvSize, transferTotal, "Fail to receive all the data expected." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Verify the buffer guard of the test buffer.
 */
static void prvVerifyTestBufferGuard( uint8_t * pTransportTestBuffer )
{
    /* Prefix and Suffix guard buffers must never change. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         pTransportTestBuffer,
                                         TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH,
                                         "transportTestBuffer prefix guard should not be altered." );
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH +
                                                                  TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_SUFFIX_GUARD_LENGTH,
                                         "transportTestBuffer suffix guard should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Thread function of the send/receive/compare test.
 */
static void prvSendRecvCompareFunc( void * pParam )
{
    threadParameter_t * pThreadParameter = pParam;
    uint8_t * pTrasnportTestBufferStart =
        &( pThreadParameter->transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );
    uint32_t testBufferSize = 0;
    NetworkContext_t * pNextworkContext = pThreadParameter->pNetworkContext;

    for( testBufferSize = 1; testBufferSize <= TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH; testBufferSize = testBufferSize + 1 )
    {
        /* Initialize the test data buffer. */
        prvInitializeTestData( pTrasnportTestBufferStart, testBufferSize );

        /* Send the test data to the server. */
        prvTransportSendData( pTestTransport, pNextworkContext, pTrasnportTestBufferStart,
                              testBufferSize );

        /* Receive the test data from server. */
        prvTransportRecvData( pTestTransport, pNextworkContext, pTrasnportTestBufferStart,
                              testBufferSize );

        /* Compare the test data received from server. */
        prvVerifyTestData( pTrasnportTestBufferStart, testBufferSize, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

        if( pThreadParameter->stopFlag == true )
        {
            break;
        }

        #if ( TRANSPORT_TEST_PRINT_DEBUG_PROGRESS == 1 )
            /* Output information to indicate the test is running. */
            UNITY_OUTPUT_CHAR( '.' );
        #endif
    }
}

/*-----------------------------------------------------------*/

static void prvRetunZeroRetryFunc( void * pParam )
{
    threadParameter_t * pThreadParameter = pParam;
    int32_t transportResult = 0;
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( pThreadParameter->transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );
    NetworkContext_t * pNetworkContext = pThreadParameter->pNetworkContext;

    /* Receive from remote server. No data will be recevied since no data is sent. */
    transportResult = pTestTransport->recv( pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( 0, transportResult, "No data to receive should return 0." );

    /* The buffer should remain unchanged when zero returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         pTrasnportTestBufferStart,
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );

    /* Send some data to echo server then retry the receive operation. The receive
     * operation should be able to receive all the data. */
    /* Initialize the test data. */
    prvInitializeTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive the test data from server. */
    prvTransportRecvData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

static void prvNoDataToReceiveFunc( void * pParam )
{
    threadParameter_t * pThreadParameter = pParam;
    int32_t transportResult = 0;
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( pThreadParameter->transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );
    NetworkContext_t * pNetworkContext = pThreadParameter->pNetworkContext;

    /* Receive from remote server. No data will be recevied since no data is sent. */
    transportResult = pTestTransport->recv( pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( 0, transportResult, "No data to receive should return 0." );

    /* The buffer should remain unchanged when zero returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         pTrasnportTestBufferStart,
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test setup function for transport interface test.
 */
TEST_SETUP( Full_TransportInterfaceTest )
{
    NetworkConnectStatus_t networkConnectResult = NETWORK_CONNECT_SUCCESS;
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Ensure the tranpsort interface is valid. */
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport, "testParam.pTransport should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pNetworkContext, "testParam.pNetworkContext should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport->send, "testParam.pTransport->send should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport->recv, "testParam.pTransport->recv should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pThreadDelay, "testParam.pThreadDelay should not be NULL." );

    /* Setup the trasnport structure to use the primary network context. */
    pTestTransport = testParam.pTransport;

    /* Initialize the transport test buffer with TRANSPORT_TEST_BUFFER_GUARD_PATTERN. */
    memset( pTransportTestBuffer, TRANSPORT_TEST_BUFFER_GUARD_PATTERN, TRANSPORT_TEST_BUFFER_TOTAL_LENGTH );

    /* Call the hook function implemented by the application to initialize the transport interface. */
    networkConnectResult = testParam.pNetworkConnect( pNetworkContext,
                                                      &testHostInfo, testParam.pNetworkCredentials );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( NETWORK_CONNECT_SUCCESS, networkConnectResult, "Network connect failed." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down function for transport interface test.
 */
TEST_TEAR_DOWN( Full_TransportInterfaceTest )
{
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    prvVerifyTestBufferGuard( pTransportTestBuffer );

    /* Call the hook function implemented by the application to de-initialize the transport interface. */
    testParam.pNetworkDisconnect( pNetworkContext );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send with NULL network context pointer handling.
 */
TEST( Full_TransportInterfaceTest, TransportSend_NetworkContextNullPtr )
{
    int32_t sendResult = 0;
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;

    /* Send with NULL network context pointer should return negative value. */
    sendResult = pTestTransport->send( NULL,
                                       &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, sendResult, "Transport interface send with NULL NetworkContext_t "
                                                        "pointer should return negative value." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send with NULL buffer pointer handling.
 */
TEST( Full_TransportInterfaceTest, TransportSend_BufferNullPtr )
{
    int32_t sendResult = 0;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Send with NULL buffer pointer should return negative value. */
    sendResult = pTestTransport->send( pNetworkContext, NULL, 1 );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, sendResult, "Transport interface send with NULL buffer "
                                                        "pointer should return negative value." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send with zero byte to send handling.
 */
TEST( Full_TransportInterfaceTest, TransportSend_ZeroByteToSend )
{
    int32_t sendResult = 0;
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Send with zero byte to send should return negative value. */
    sendResult = pTestTransport->send( pNetworkContext,
                                       &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       0 );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, sendResult, "Transport interface send with zero byte "
                                                        "to send should return negative value." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface recv with NULL network context pointer handling.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_NetworkContextNullPtr )
{
    int32_t recvResult = 0;
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;

    /* Receive with NULL network context pointer should return negative value. */
    recvResult = pTestTransport->recv( NULL,
                                       &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, recvResult, "Transport interface recv with NULL network "
                                                        "context pointer should return negative value." );

    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface recv with NULL buffer pointer handling.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_BufferNullPtr )
{
    int32_t recvResult = 0;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Receive with NULL buffer pointer should return negative value. */
    recvResult = pTestTransport->recv( pNetworkContext, NULL, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, recvResult, "Transport interface recv with NULL buffer "
                                                        "pointer should return negative value." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface recv zero byte to receive handling.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_ZeroByteToRecv )
{
    int32_t recvResult = 0;
    uint8_t * pTransportTestBuffer = threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer;
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Receive with zero byte to recv should return negative value. */
    recvResult = pTestTransport->recv( pNetworkContext,
                                       &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       0 );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, recvResult, "Transport interface recv with zero "
                                                        "byte to recv should return negative value." );

    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( pTransportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send one byte, receive and compare
 *
 * Test send receive beharivor in the following order.
 * Send : 1 byte
 * Send : ( TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH - 1 ) bytes
 * Receive : TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH bytes
 */
TEST( Full_TransportInterfaceTest, Transport_SendOneByteRecvCompare )
{
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart, 1U );

    /* Send the rest test data to the server. */
    prvTransportSendData( pTestTransport, pNetworkContext, &pTrasnportTestBufferStart[ 1 ],
                          ( TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH - 1U ) );

    /* Receive the test data from server. */
    prvTransportRecvData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart,
                          TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive one byte and compare.
 *
 * Test send receive beharivor in the following order.
 * Send : TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH bytes
 * Receive : 1 byte
 * Receive : ( TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH - 1 ) bytes
 */
TEST( Full_TransportInterfaceTest, Transport_SendRecvOneByteCompare )
{
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );
    NetworkContext_t * pNetworkContext = threadParameter[ TRANSPORT_TEST_INDEX ].pNetworkContext;

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart,
                          TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Reset the buffer. The buffer after received data is verified in prvVerifyTestData. */
    memset( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_GUARD_PATTERN, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive one byte test data from server. */
    prvTransportRecvData( pTestTransport, pNetworkContext, pTrasnportTestBufferStart, 1U );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, 1U, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive remain test data from server. */
    prvTransportRecvData( pTestTransport, pNetworkContext, &pTrasnportTestBufferStart[ 1U ],
                          ( TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH - 1U ) );

    /* Compare remain test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive and compare on variable length.
 */
TEST( Full_TransportInterfaceTest, Transport_SendRecvCompare )
{
    prvSendRecvCompareFunc( &threadParameter[ TRANSPORT_TEST_INDEX ] );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive and compare on variable length in multiple threads.
 */
TEST( Full_TransportInterfaceTest, Transport_SendRecvCompareMultithreaded )
{
    NetworkConnectStatus_t networkConnectResult = NETWORK_CONNECT_SUCCESS;
    int timedWaitResult = 0;
    ThreadHandle_t threadHandle[ TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT ];
    uint32_t threadIndex = 0;

    /* The primary thread parameter already setup in the test setup function. */
    for( threadIndex = 1; threadIndex < TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT; threadIndex++ )
    {
        memset( threadParameter[ threadIndex ].transportTestBuffer,
                TRANSPORT_TEST_BUFFER_GUARD_PATTERN, TRANSPORT_TEST_BUFFER_TOTAL_LENGTH );
    }

    /* Connect the secondary network context. */
    networkConnectResult = testParam.pNetworkConnect( testParam.pSecondNetworkContext,
                                                      &testHostInfo, testParam.pNetworkCredentials );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( NETWORK_CONNECT_SUCCESS, networkConnectResult, "Network connect failed." );

    /* Create testing threads. */
    for( threadIndex = 0; threadIndex < TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT; threadIndex++ )
    {
        threadParameter[ threadIndex ].stopFlag = false;
        threadHandle[ threadIndex ] = testParam.pThreadCreate( prvSendRecvCompareFunc,
                                                               &threadParameter[ threadIndex ] );
        TEST_ASSERT_MESSAGE( threadHandle != NULL, "Create thread failed." );
    }

    /* Waiting for all test threads complete. */
    for( threadIndex = 0; threadIndex < TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT; threadIndex++ )
    {
        timedWaitResult = testParam.pThreadTimedWait( threadHandle[ threadIndex ],
                                                      TRANSPORT_TEST_WAIT_THREAD_TIMEOUT_MS );

        if( timedWaitResult != 0 )
        {
            threadParameter[ threadIndex ].stopFlag = true;
        }
    }

    /* The primary thread network context will be closed in the test shutdown function. */
    for( threadIndex = 1; threadIndex < TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT; threadIndex++ )
    {
        testParam.pNetworkDisconnect( threadParameter[ threadIndex ].pNetworkContext );

        /* Check the test buffer guard. */
        prvVerifyTestBufferGuard( threadParameter[ threadIndex ].transportTestBuffer );
    }

    /* Check if every thread finish the test in time. */
    for( threadIndex = 0; threadIndex < TRANSPORT_TEST_MULTI_THREAD_TASK_COUNT; threadIndex++ )
    {
        TEST_ASSERT_MESSAGE( threadParameter[ threadIndex ].stopFlag != true, "Test thread timeout." );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send function return value when disconnected.
 */
TEST( Full_TransportInterfaceTest, TransportSend_RemoteDisconnect )
{
    int32_t transportResult = 0;

    uint8_t * pTrasnportTestBufferStart =
        &( threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Send the disconnect command to remote server. */
    transportResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                            TRANSPORT_TEST_DISCONNECT_COMMAND,
                                            strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ) );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ), transportResult,
                                     "Transport send should not have any error." );

    /* Delay to wait for the command send to server and server disconnection. */
    testParam.pThreadDelay( TRANSPORT_TEST_NETWORK_DELAY_MS );

    /* Negative value should be returned if a network disconnection has occurred. */
    transportResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, transportResult,
                                         "Transport send should return negative value when disconnected." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface receive function return value when disconnected.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_RemoteDisconnect )
{
    int32_t transportResult = 0;

    uint8_t * pTrasnportTestBufferStart =
        &( threadParameter[ TRANSPORT_TEST_INDEX ].transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Send the disconnect command to remote server. */
    transportResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                            TRANSPORT_TEST_DISCONNECT_COMMAND,
                                            strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ) );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ), transportResult,
                                     "Transport send should not have any error." );

    /* Delay to wait for the command send to server. */
    testParam.pThreadDelay( TRANSPORT_TEST_NETWORK_DELAY_MS );

    /* Negative value should be returned if a network disconnection has occurred. */
    transportResult = pTestTransport->recv( pTestTransport->pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, transportResult,
                                         "Transport receive should return negative value when disconnected." );

    /* The buffer should remain unchanged when negative value returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         pTrasnportTestBufferStart,
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface receive function return value when no data to receive.
 *
 * Transport interface recv function returns zero due to no data to receive. A test
 * thread is used in this test to prevent the implementation blocked in the receive
 * function when no data to receive.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_NoDataToReceive )
{
    ThreadHandle_t threadHandle;
    int timedWaitResult = 0;

    /* Create testing threads. */
    threadParameter[ TRANSPORT_TEST_INDEX ].stopFlag = false;
    threadHandle = testParam.pThreadCreate( prvNoDataToReceiveFunc,
                                            &threadParameter[ TRANSPORT_TEST_INDEX ] );
    TEST_ASSERT_MESSAGE( threadHandle != NULL, "Create thread failed." );

    /* Waiting for the test thread complete. */
    timedWaitResult = testParam.pThreadTimedWait( threadHandle,
                                                  TRANSPORT_TEST_WAIT_THREAD_RECEIVE_TIMEOUT_MS );

    if( timedWaitResult != 0 )
    {
        threadParameter[ TRANSPORT_TEST_INDEX ].stopFlag = true;
    }

    TEST_ASSERT_MESSAGE( timedWaitResult == 0, "Waiting for test thread receive data failed." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface receive return zero retry.
 *
 * Transport interface recv function returns zero due to no data to receive. The test
 * sends test data to remote server and retry the transport receive operation. Transport
 * receive function should be able to receive data. A test thread is used in this test
 * to prevent the implementation blocked in the receive function when no data to receive.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_ReturnZeroRetry )
{
    ThreadHandle_t threadHandle;
    int timedWaitResult = 0;

    /* Create testing threads. */
    threadParameter[ TRANSPORT_TEST_INDEX ].stopFlag = false;
    threadHandle = testParam.pThreadCreate( prvRetunZeroRetryFunc,
                                            &threadParameter[ TRANSPORT_TEST_INDEX ] );
    TEST_ASSERT_MESSAGE( threadHandle != NULL, "Create thread failed." );

    /* Waiting for the test thread complete. */
    timedWaitResult = testParam.pThreadTimedWait( threadHandle,
                                                  TRANSPORT_TEST_WAIT_THREAD_RECEIVE_TIMEOUT_MS );

    if( timedWaitResult != 0 )
    {
        threadParameter[ TRANSPORT_TEST_INDEX ].stopFlag = true;
    }

    TEST_ASSERT_MESSAGE( timedWaitResult == 0, "Waiting for test thread receive data failed." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for transport interface test against echo server.
 */
TEST_GROUP_RUNNER( Full_TransportInterfaceTest )
{
    /* Invalid parameter test. Disable or replace assert may be required to run these tests. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportSend_NetworkContextNullPtr );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportSend_BufferNullPtr );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportSend_ZeroByteToSend );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_NetworkContextNullPtr );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_BufferNullPtr );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_ZeroByteToRecv );

    /* Send and receive correctness test. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, Transport_SendOneByteRecvCompare );
    RUN_TEST_CASE( Full_TransportInterfaceTest, Transport_SendRecvOneByteCompare );
    RUN_TEST_CASE( Full_TransportInterfaceTest, Transport_SendRecvCompare );
    RUN_TEST_CASE( Full_TransportInterfaceTest, Transport_SendRecvCompareMultithreaded );

    /* Disconnect test. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportSend_RemoteDisconnect );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_RemoteDisconnect );

    /* Transport behavior test. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_NoDataToReceive );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_ReturnZeroRetry );
}

/*-----------------------------------------------------------*/

int RunTransportInterfaceTest( void )
{
    int status = -1;

    #if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
        /* Assign the TransportInterface_t pointer used in test cases. */
        SetupTransportTestParam( &testParam );
        threadParameter[ 0 ].pNetworkContext = testParam.pNetworkContext;
        threadParameter[ 1 ].pNetworkContext = testParam.pSecondNetworkContext;
        testHostInfo.pHostName = ECHO_SERVER_ENDPOINT;
        testHostInfo.port = ECHO_SERVER_PORT;

        /* Initialize unity. */
        UnityFixture.Verbose = 1;
        UnityFixture.GroupFilter = 0;
        UnityFixture.NameFilter = 0;
        UnityFixture.RepeatCount = 1;
        UNITY_BEGIN();

        /* Run the test group. */
        RUN_TEST_GROUP( Full_TransportInterfaceTest );

        status = UNITY_END();
    #endif /* if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 ) */
    return status;
}

/*-----------------------------------------------------------*/
