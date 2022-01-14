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
#include "transport_interface_tests.h"

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
 * @brief Test buffer used in tests.
 */
static uint8_t transportTestBuffer[ TRANSPORT_TEST_BUFFER_TOTAL_LENGTH ];

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
 * The test buffer after testSize should remain unchanged. It is verified in this
 * function.
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
        TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                             &( pTransportTestBuffer[ testSize ] ),
                                             ( maxBufferSize - testSize ),
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
                                  uint8_t * pTransportTestBuffer,
                                  uint32_t sendSize )
{
    uint32_t transferTotal = 0U;
    int32_t transportResult = 0;
    uint32_t i = 0U;

    for( i = 0U; i < TRANSPORT_TEST_SEND_RECEIVE_RETRY_COUNT; i++ )
    {
        transportResult = pTransport->send( pTransport->pNetworkContext,
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
        transportResult = pTransport->recv( pTransport->pNetworkContext,
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
        testParam.pTransportTestDelay( TRANSPORT_TEST_DELAY_MS );
    }

    /* Check if all the data is recevied. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( recvSize, transferTotal, "Fail to receive all the data expected." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test setup function for transport interface test.
 */
TEST_SETUP( Full_TransportInterfaceTest )
{
    int32_t networkConnectResult = 0;

    /* Ensure the tranpsort interface is valid. */
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport, "testParam.pTransport should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pNetworkContext, "testParam.pNetworkContext should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport->send, "testParam.pTransport->send should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransport->recv, "testParam.pTransport->recv should not be NULL." );
    TEST_ASSERT_NOT_NULL_MESSAGE( testParam.pTransportTestDelay, "testParam.pTransportTestDelay should not be NULL." );

    /* Setup the trasnport structure to use the primary network context. */
    testParam.pTransport->pNetworkContext = testParam.pNetworkContext;
    pTestTransport = testParam.pTransport;

    /* Initialize the transport test buffer with TRANSPORT_TEST_BUFFER_GUARD_PATTERN. */
    memset( &( transportTestBuffer[ 0 ] ), TRANSPORT_TEST_BUFFER_GUARD_PATTERN, TRANSPORT_TEST_BUFFER_TOTAL_LENGTH );

    /* Call the hook function implemented by the application to initialize the transport interface. */
    networkConnectResult = testParam.pNetworkConnect( pTestTransport->pNetworkContext,
                                                      &testHostInfo, testParam.pNetworkCredentials );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( 0, networkConnectResult, "Network connect failed." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down function for transport interface test.
 */
TEST_TEAR_DOWN( Full_TransportInterfaceTest )
{
    /* Prefix and Suffix guard buffers must never change. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ 0 ] ),
                                         TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH,
                                         "transportTestBuffer prefix guard should not be altered." );
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH +
                                                                 TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_SUFFIX_GUARD_LENGTH,
                                         "transportTestBuffer suffix guard should not be altered." );

    /* Call the hook function implemented by the application to de-initialize the transport interface. */
    testParam.pNetworkDisconnect( pTestTransport->pNetworkContext );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send with NULL network context pointer handling.
 */
TEST( Full_TransportInterfaceTest, TransportSend_NetworkContextNullPtr )
{
    int32_t sendResult = 0;

    /* Send with NULL network context pointer should return negative value. */
    sendResult = pTestTransport->send( NULL,
                                       &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
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

    /* Send with NULL buffer pointer should return negative value. */
    sendResult = pTestTransport->send( pTestTransport->pNetworkContext, NULL, 1 );
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

    /* Send with zero byte to send should return negative value. */
    sendResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                       &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
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

    /* Receive with NULL network context pointer should return negative value. */
    recvResult = pTestTransport->recv( NULL,
                                       &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, recvResult, "Transport interface recv with NULL network "
                                                        "context pointer should return negative value." );

    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
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

    /* Receive with NULL buffer pointer should return negative value. */
    recvResult = pTestTransport->recv( pTestTransport->pNetworkContext, NULL,
                                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
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

    /* Receive with zero byte to recv should return negative value. */
    recvResult = pTestTransport->recv( pTestTransport->pNetworkContext,
                                       &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                       0 );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, recvResult, "Transport interface recv with zero "
                                                        "byte to recv should return negative value." );

    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive and compare on one byte.
 *
 * Test send receive beharivor in the following order.
 * Send : 1 bytes
 * Receive : 1 byte
 */
TEST( Full_TransportInterfaceTest, Transport_SendOneByteRecvCompare )
{
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, 1U );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pTrasnportTestBufferStart, 1U );

    /* Receive the test data from server. */
    prvTransportRecvData( pTestTransport, pTrasnportTestBufferStart, 1U );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, 1U, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive and compare.
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
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pTrasnportTestBufferStart,
                          TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Reset the buffer. The buffer after received data is verified in prvVerifyTestData. */
    memset( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_GUARD_PATTERN, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive one byte test data from server. */
    prvTransportRecvData( pTestTransport, pTrasnportTestBufferStart, 1U );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, 1U, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive remain test data from server. */
    prvTransportRecvData( pTestTransport, &pTrasnportTestBufferStart[ 1U ],
                          ( TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH - 1U ) );

    /* Compare remain test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface with send, receive and compare on variable length.
 *
 * Test send receive beharivor in the following order.
 * Send : TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH bytes
 * Receive : TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH bytes
 */
TEST( Full_TransportInterfaceTest, Transport_SendRecvCompare )
{
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pTrasnportTestBufferStart,
                          TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Receive the test data from server. */
    prvTransportRecvData( pTestTransport, pTrasnportTestBufferStart,
                          TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                       TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface send function return value when disconnected.
 */
TEST( Full_TransportInterfaceTest, TransportSend_RemoteDisconnect )
{
    int32_t transportResult = 0;

    uint8_t * pTrasnportTestBufferStart =
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Send the disconnect command to remote server. */
    transportResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                            TRANSPORT_TEST_DISCONNECT_COMMAND,
                                            strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ) );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ), transportResult,
                                     "Transport send should not have any error." );

    /* Delay to wait for the command send to server and server disconnection. */
    testParam.pTransportTestDelay( TRANSPORT_TEST_NETWORK_DELAY_MS );

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
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Send the disconnect command to remote server. */
    transportResult = pTestTransport->send( pTestTransport->pNetworkContext,
                                            TRANSPORT_TEST_DISCONNECT_COMMAND,
                                            strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ) );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( strlen( TRANSPORT_TEST_DISCONNECT_COMMAND ), transportResult,
                                     "Transport send should not have any error." );

    /* Delay to wait for the command send to server. */
    testParam.pTransportTestDelay( TRANSPORT_TEST_NETWORK_DELAY_MS );

    /* Negative value should be returned if a network disconnection has occurred. */
    transportResult = pTestTransport->recv( pTestTransport->pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_LESS_THAN_INT32_MESSAGE( 0, transportResult,
                                         "Transport receive should return negative value when disconnected." );

    /* The buffer should remain unchanged when negative value returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface receive function return value when no data to receive.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_NoDataToReceive )
{
    int32_t transportResult = 0;
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Receive from remote server. No data will be recevied since no data is sent. */
    transportResult = pTestTransport->recv( pTestTransport->pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( 0, transportResult, "No data to receive should return 0." );

    /* The buffer should remain unchanged when zero returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test transport interface receive return zero retry.
 *
 * Transport interface recv function returns zero due to no data to receive. The test
 * sends test data to remote server and retry the transport receive operation. Transport
 * receive function should return postivie value after retry.
 */
TEST( Full_TransportInterfaceTest, TransportRecv_ReturnZeroRetry )
{
    int32_t transportResult = 0;
    uint32_t transportSendTotal = 0, transportRecvTotal = 0;
    /* Pointer of writable test buffer. */
    uint8_t * pTrasnportTestBufferStart =
        &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] );

    /* Receive from remote server. No data will be recevied since no data is sent. */
    transportResult = pTestTransport->recv( pTestTransport->pNetworkContext,
                                            pTrasnportTestBufferStart,
                                            TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
    TEST_ASSERT_EQUAL_INT32_MESSAGE( 0, transportResult, "No data to receive should return 0." );

    /* The buffer should remain unchanged when zero returned. */
    TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE( TRANSPORT_TEST_BUFFER_GUARD_PATTERN,
                                         &( transportTestBuffer[ TRANSPORT_TEST_BUFFER_PREFIX_GUARD_LENGTH ] ),
                                         TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH,
                                         "transportTestBuffer should not be altered." );

    /* Initialize the test data buffer. */
    prvInitializeTestData( pTrasnportTestBufferStart, 1U );

    /* Send the test data to the server. */
    prvTransportSendData( pTestTransport, pTrasnportTestBufferStart, 1U );

    /* Receive the test data from server. */
    prvTransportRecvData( pTestTransport, pTrasnportTestBufferStart, 1U );

    /* Compare the test data received from server. */
    prvVerifyTestData( pTrasnportTestBufferStart, 1U, TRANSPORT_TEST_BUFFER_WRITABLE_LENGTH );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for transport interface test against echo server.
 */
TEST_GROUP_RUNNER( Full_TransportInterfaceTest )
{
    /* Invalid parameter test. */
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

    /* Disconnect test. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportSend_RemoteDisconnect );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_RemoteDisconnect );

    /* Transport behavior test. */
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_NoDataToReceive );
    RUN_TEST_CASE( Full_TransportInterfaceTest, TransportRecv_ReturnZeroRetry );
}

/*-----------------------------------------------------------*/

int RunTransportInterfaceTests( void )
{
    int status = -1;

    #if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
        /* Assign the TransportInterface_t pointer used in test cases. */
        SetupTransportTestParam( &testParam );
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
