#ifndef _NETWORK_CONNECTION_H_
#define _NETWORK_CONNECTION_H_

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

int NetworkConnect( void * pNetworkContext, TestHostInfo_t * pHostInfo,  TestNetworkCredentials_t * pNetworkCredentials );

void NetworkDisconnect( void * pNetworkContext );

#endif /* _NETWORK_CONNECTION_H_ */
