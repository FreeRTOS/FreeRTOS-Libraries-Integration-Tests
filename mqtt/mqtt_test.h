#pragma once

#include "transport_interface.h"

/**
 * To use MQTT test, first call setupMqttTest() to passed in required parameters. Then call runMqttTest().
 */

typedef struct TestHostInfo
{
    const char * pHostName; /**< @brief Server host name. The string should be nul terminated. */
    uint16_t port;          /**< @brief Server port in host-order. */
} TestHostInfo_t;

typedef struct TestNetworkCredentials
{
    const uint8_t * pRootCa;     /**< @brief String representing a trusted server root certificate. */
    size_t rootCaSize;           /**< @brief Size associated with #NetworkCredentials.pRootCa. */
    const uint8_t * pClientCert; /**< @brief String representing the client certificate. */
    size_t clientCertSize;       /**< @brief Size associated with #NetworkCredentials.pClientCert. */
    const uint8_t * pPrivateKey; /**< @brief String representing the client certificate's private key. */
    size_t privateKeySize;       /**< @brief Size associated with #NetworkCredentials.pPrivateKey. */
} TestNetworkCredentials_t;

typedef struct MqttTestParam
{
    TransportInterface_t * pTransport; /**< @brief pNetworkContext, send, receive fields of the TransportInterface_t struct should be filled out. */
    TestHostInfo_t * pHostInfo;
    TestNetworkCredentials_t * pNetworkCredentials;
    void * pNetworkContext;
    void * pSecondNetworkContext;
} MqttTestParam_t;

// Network_Disconnect_Func takes three arguments: network context, hostinfo and networkcredentials
typedef int (*Network_Connect_Func)( void *, TestHostInfo_t *,  TestNetworkCredentials_t * );
typedef void (*Network_Disconnect_Func)( void * );

/**
 * @brief This function takes in required parameters for MQTT test to run.
 *
 * @param[in] pTestParam Bundle of parameters for setting up the test.
 * @param[in] pNetworkConnect a function pointer for establishing network
 * connection to server.
 * @param[in] pNetworkDisconnect a function pointer for disconnecting from server.
 *
 */
void setupMqttTest( MqttTestParam_t * pTestParam,
                    Network_Connect_Func pNetworkConnect,
                    Network_Disconnect_Func pNetworkDisconnect );

/**
 * @brief Run MQTT test. This function should be called after calling `setupMqttTest()`.
 */
int runMqttTest();
