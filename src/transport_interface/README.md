# Transport Interface Test

## On this page:
1. [Introduction](#1-introduction)
2. [Transport Interface Test Cases](#2-transport-interface-test-cases)
3. [Prerequisites For Transport Interface Test](#3-prerequisites-for-transport-interface-test)
4. [Source Code Organization](#4-source-code-organization)
5. [Implement Transport Interface Test Application](#5-implement-transport-interface-test-application)
6. [Run The Transport Interface Test](#6-run-the-transport-interface-test)<br>
</t>6.1. [Start the echo server](#61-start-the-echo-server)<br>
</t>6.2. [Start the echo server with TLS](#62-start-the-echo-server-with-tls)<br>
</t>6.3. [Setup the host info and network credentials](#63-setup-the-host-info-and-network-credentials)<br>
</t>6.4. [Compile and run the transport interface test application](#64-compile-and-run-the-transport-interface-test-application)<br>

## 1. Introduction

Transport interface tests help to verify transport interface implementation. The implementation needs to follow the [guideline in transport interface header file](https://freertos.org/Documentation/api-ref/coreMQTT/docs/doxygen/output/html/group__mqtt__callback__types.html#gaff3e08cfa7368b526ec1f8d87d10d7c2).

The test directly exercises the transport interface on the device under testing and interacts with a remote echo server. The following are required to run the transport interface tests:

* A PC to run the provided echo server.
* A test application which runs on the device under test with transport interface implemented.

The test application is usually implemented by calling the provided transport interface test routine from the main function.


## 2. Transport Interface Test Cases

The transport interface tests verify the implementation by running various test cases. The test cases are designed according to the guidelines in the transport interface header file.

|Test Case	|Test Case Detail	|Expected result	|
|---	|---	|---	|
|TransportSend_NetworkContextNullPtr	|Test transport interface send with NULL network context pointer handling	|Negative value should be returned	|
|TransportSend_BufferNullPtr	|Test transport interface send with NULL buffer pointer handling	|Negative value should be returned	|
|TransportSend_ZeroByteToSend	|Test transport interface send with zero byte to send handling	|Negative value should be returned	|
|TransportRecv_NetworkContextNullPtr	|Test transport interface recv with NULL network context pointer handling	|Negative value should be returned	|
|TransportRecv_BufferNullPtr	|Test transport interface recv with NULL buffer pointer handling	|Negative value should be returned	|
|TransportRecv_ZeroByteToRecv	|Test transport interface recv with zero byte to receive handling	|Negative value should be returned	|
|Transport_SendOneByteRecvCompare 	|Test transport interface with send, receive and compare on one byte |Send/Receive/Verify 1 byte of data should has no error	|
|Transport_SendRecvOneByteCompare 	|Test transport interface with send bulk of data, receive one byte and compare |Send bulk of data. Receive/verify 1 byte then the rest of the data should has no error	|
|Transport_SendRecvCompare 	|Test transport interface with send, receive and compare on bulk of data. |Send/Receive/Verify bulk of data should has no error	|
|TransportSend_RemoteDisconnect	|Test transport interface send function return value when disconnected by remote server	|Negative value should be returned	|
|TransportRecv_RemoteDisconnect	|Test transport interface receive function return value when disconnected by remote server	|Negative value should be returned	|
|TransportRecv_NoDataToReceive	|Test transport interface receive function return value when no data to receive	|0 should be returned	|
|TransportRecv_ReturnZeroRetry	|Test transport interface receive function return zero then retry the receive function	|Postive value should be returned after retry	|


## 3. Prerequisites For Transport Interface Test

The transport interface test assumes the tested platform already has the following components integrated.

* **Transport interface implementation**
    The transport interface implementation should be able to setup a TCP network connection.
* **Unity test framework**
    Transport interface test make use of the Unity test framework. Reference the [website](https://github.com/ThrowTheSwitch/Unity) for integration guide.

A PC which can run Go language program and has network connectivity is required to run the Go-based echo server.


## 4. Source Code Organization

Folder structure of the transport interface tests. The tree only lists the required files to run the transport interface test.

```
.
├── include
│   ├── test_execution_config.h
│   └── test_param_config.h
└── src
    ├── common
    │   └── network_connection.h
    ├── qualification_test.c
    ├── qualification_test.h
    └── transport_interface
        ├── README.md
        ├── transport_interface_tests.c
        └── transport_interface_tests.h
```

## 5. Implement Transport Interface Test Application

Developer implements the transport interface test application with the following steps.

1. Add the following source files to your compilation source
* ./src/qualification_test.c
* ./src/transport_interface/transport_interface_tests.c

2. Add the folloing paths to your compilation include path.
* ./include
* ./src
* ./src/common
* ./src/transport_interface

3. Implement the setup function, **SetupTransportTestParam**, for transport interface tests to provide test parameters. The following are required test parameters:
```C
/**
 * NetworkConnectFunc is a function pointer type for functions that
 * establish a network connection with a server. It takes three arguments:
 * a void * network context,
 * a TestHostInfo_t * host info about which server to connect to,
 * a void * network credentials.
 *
 * Network context and crendentials are defined by application implementations.
 */
typedef int (*NetworkConnectFunc)( void *, TestHostInfo_t *, void * );

/**
 * @brief Delay function to wait for the data transfer over transport network.
 *
 * @param[in] delayMs Delay in milliseconds.
 */
typedef void (* TransportTestDelayFunc)( uint32_t delayMs );

/**
 * NetworkDisconnectFunc is a function pointer type for functions that
 * disconnect a previously established network connection. It takes one argument:
 * a void * network context.
 *
 * Network context is defined by application implementations.
 */
typedef void (*NetworkDisconnectFunc)( void * );

typedef struct TransportTestParam
{
    TransportInterface_t * pTransport;          /**< @brief Transport interface structure to test. */
    NetworkConnectFunc pNetworkConnect;         /**< @brief Network connect function pointer. */
    NetworkDisconnectFunc pNetworkDisconnect;   /**< @brief Network disconnect function pointer. */
    TransportTestDelayFunc pTransportTestDelay; /**< @brief Transport test delay function pointer. */
    void * pNetworkCredentials;                 /**< @brief Network credentials for network connection. */
    void * pNetworkContext;                     /**< @brief Primary network context. */
} TransportTestParam_t;

/**
 * @brief Customers need to implement this fuction by filling in parameters
 * in the provided TransportTestParam_t.
 *
 * @param[in] pTestParam a pointer to TransportTestParam_t struct to be filled out by
 * caller.
 */
void SetupTransportTestParam( TransportTestParam_t * pTestParam );
```

4. Enable the transport interface config, **TRANSPORT_INTERFACE_TEST_ENABLED**, in **./include/test_execution_config.h**.
```C
#define TRANSPORT_INTERFACE_TEST_ENABLED  ( 1 )     /* Set 1 to enable the transport interface test. */
```

5. Implement the main function and call the **runQualificationTest**.

The following is an example test application.

```C
#include "transport_interface.h"
#include "transport_interface_tests.h"
#include "qualification_test.h"

static NetworkContext_t xNetworkContext = { 0 };
static TransportInterface_t xTransport = { 0 };

int TransportNetworkConnect( void * pNetworkContext, TestHostInfo_t * pHostInfo, void * pNetworkCredentials )
{
    /* Connect the transport network. */
}

void TransportNetworkDisconnect( void * pNetworkContext )
{
    /* Disconnect the transport network. */
}

void TransportTestDelay( uint32_t delayMs )
{
    /* Delay function to wait for the response from network. */
}

void SetupTransportTestParam( TransportTestParam_t * pTestParam )
{
    if( pTestParam != NULL )
    {
        /* Setup the transport interface. */
        xTransport.send = /* YourTransportSendFunction. */;
        xTransport.recv = /* YourTransportRecvFunction. */;

        pTestParam->pTransport = &xTransport;
        pTestParam->pNetworkContext = &xNetworkContext;
        pTestParam->pNetworkConnect = TransportNetworkConnect;
        pTestParam->pNetworkDisconnect = TransportNetworkDisconnect;
        pTestParam->pTransportTestDelay = TransportTestDelay;
        pTestParam->pNetworkCredentials = /* YourNetworkCredentials. */;
    }
}

void yourMainFunction( void )
{
    runQualificationTest();
}
```

## 6. Run The Transport Interface Test

The go-based echo_server.go program can setup a TCP server to echo back data. This echo server is used in the transport interface test to verify the transport interface implementation.

The transport interface test starts with the following steps:

### 6.1. Start the echo server

The echo server needs a configuration file. The following is echo server configurations:

* **verbose**
    * Enable this option to output the contents of the message sent to the echo server.
* **logging**
    * Enable this option to log all messages received to a file.
* **secure-connection**
    * Enable this option to switch to using TLS for the echo server. Note you will have to complete the credential creation prerequisite.
* **server-port**
    * Specify which port to open a socket on.
* **server-certificate-location**
    * Relative or absolute path to the server certificate generated in the credential creation prerequisite.
* **server-key-location**
    * Relative or absolute path to the server key generated in the credential creation prerequisite.


To run the echo serve without TLS, the following configuraition file, "example_config.json", can be referenced as an example to run the echo server. 

```
{
    "verbose": false,
    "logging": false,
    "secure-connection": false,
    "server-port": "9000",
    "server-certificate-location": "",
    "server-key-location": ""
}
```


The **server address** and the **server-port** information will be required in the TransportConnectHook function. “**server-port**” in the example config can be changed to different port of developer’s choice.

Example command to start the echo server

```
go run echo_server.go -config=example_config.json
```

### 6.2. Start the echo server with TLS
Developer’s can also setup the transport interface test over mutual authenticated TLS with this echo server tool. Echo server must setup with the **secure-connection** configuration and provide server certificate and key in the configuration file. TLS connection capability and client certificate will be verified by the echo server.

This [document](https://github.com/FreeRTOS/freertos-integration-toolkit/blob/main/transport_interface_test/tools/echo_server/readme.md) describes how to create self-signed credentials for the echo server. The self-signed credentials is only for test transport interface test.

To run the echo serve with TLS, the following configuraition file, "example_tls_config.json", can be referenced as an example to run the echo server. 
```
{
    "verbose": false,
    "logging": false,
    "secure-connection": true,
    "server-port": "9000",
    "server-certificate-location": "YourServerCertifcate",
    "server-key-location": "YourServerKey"
}
```

Example command to start the echo server with TLS

```
go run echo_server.go -config=example_tls_config.json
```

### 6.3. Setup the host info and network credentials
Provide the **server-address** and **server-port** in **./include/test_param_config.h**.
```C
#define ECHO_SERVER_ENDPOINT   "server-address"
#define ECHO_SERVER_PORT       ( server-port )
```
To run the transport interface test with TLS, network credentials need to be assigned in the SetupTransportTestParam function.
```C
void SetupTransportTestParam( TransportTestParam_t * pTestParam )
{
    if( pTestParam != NULL )
    {
        ...
        pTestParam->pNetworkConnect = /* YourNetworkConnectionFunction. */;
        pTestParam->pNetworkDisconnect = /* YourNetworkDisonnectionFunction. */;
        pTestParam->pNetworkCredentials = /* YourNetworkCredentials. */;
        ...
    }
}
```
The pNetworkCredentials will be passed to the pNetworkConnect assigned in the same SetupTransportTestParam function.

### 6.4. Compile and run the transport interface test application
Compile and run the test application in your development environment. The following is a sample test result log:
```
TEST(Full_TransportInterfaceTest, TransportSend_NetworkContextNullPtr) PASS
TEST(Full_TransportInterfaceTest, TransportSend_BufferNullPtr) PASS
TEST(Full_TransportInterfaceTest, TransportSend_ZeroByteToSend) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_NetworkContextNullPtr) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_BufferNullPtr) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_ZeroByteToRecv) PASS
TEST(Full_TransportInterfaceTest, Transport_SendOneByteRecvCompare) PASS
TEST(Full_TransportInterfaceTest, Transport_SendRecvOneByteCompare) PASS
TEST(Full_TransportInterfaceTest, Transport_SendRecvCompare) PASS
TEST(Full_TransportInterfaceTest, TransportSend_RemoteDisconnect) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_RemoteDisconnect) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_NoDataToReceive) PASS
TEST(Full_TransportInterfaceTest, TransportRecv_ReturnZeroRetry) PASS

-----------------------
13 Tests 0 Failures 0 Ignored
OK
```
