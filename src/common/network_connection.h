#ifndef _NETWORK_CONNECTION_H_
#define _NETWORK_CONNECTION_H_

typedef struct TestHostInfo
{
    const char * pHostName; /**< @brief Server host name. The string should be nul terminated. */
    uint16_t port;          /**< @brief Server port in host-order. */
} TestHostInfo_t;

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
 * NetworkDisconnectFunc is a function pointer type for functions that
 * disconnect a previously established network connection. It takes one argument:
 * a void * network context.
 *
 * Network context is defined by application implementations.
 */
typedef void (*NetworkDisconnectFunc)( void * );

#endif /* _NETWORK_CONNECTION_H_ */
