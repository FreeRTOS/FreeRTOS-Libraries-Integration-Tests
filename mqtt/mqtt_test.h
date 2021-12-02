#pragma once

#include "transport_interface.h"

/**
 * To use MQTT test, first call setupMqttTest() to passed in required parameters. Then call runMqttTest().
 */

typedef struct HostInfo
{
    const char * pHostName; /**< @brief Server host name. The string should be nul terminated. */
    uint16_t port;          /**< @brief Server port in host-order. */
} HostInfo_t;

typedef int (*Network_Connect_Func)( HostInfo_t * );
typedef void (*Network_Disconnect_Func)( void );

/**
 * @brief This function takes in required parameters for MQTT test to run.
 *
 * @param[in] pTransport Transport interface for the test. pNetworkContext,
 * send, receive fields of the TransportInterface_t struct should be filled out.
 * @param[in] pNetworkConnect a function pointer for establishing network
 * connection to server.
 * @param[in] pNetworkDisconnect a function pointer for disconnecting from server.
 * @param[in] pHostInfo Hostname and port for MQTT server.
 *
 */
void setupMqttTest( TransportInterface_t * pTransport,
                    Network_Connect_Func pNetworkConnect,
                    Network_Disconnect_Func pNetworkDisconnect,
                    HostInfo_t * pHostInfo );

/**
 * @brief Run MQTT test. This function should be called after calling `setupMqttTest()`.
 */
int runMqttTest();
