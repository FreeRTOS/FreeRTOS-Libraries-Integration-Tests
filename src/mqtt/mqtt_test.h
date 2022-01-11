#pragma once

#include "transport_interface.h"
#include "network_connection.h"

typedef struct MqttTestParam
{
    TransportInterface_t * pTransport; /**< @brief pNetworkContext, send, receive fields of the TransportInterface_t struct should be filled out. */
    NetworkConnectFunc pNetworkConnect;
    NetworkDisconnectFunc pNetworkDisconnect;
    void * pNetworkCredentials;
    void * pNetworkContext;
    void * pSecondNetworkContext;
} MqttTestParam_t;

/**
 * @brief Customers need to implement this fuction by filling in parameters
 * in the provided MqttTestParam_t.
 *
 * @param[in] pTestParam a pointer to MqttTestParam_t struct to be filled out by
 * caller.
 *
 */
void setupMqttTestParam( MqttTestParam_t * pTestParam );

/**
 * @brief Run MQTT test. This function should be called after calling `setupMqttTest()`.
 */
int runMqttTest();
