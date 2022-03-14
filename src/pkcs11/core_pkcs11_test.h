#ifndef CORE_PKCS11_TEST_H
#define CORE_PKCS11_TEST_H


/**
 * @brief Malloc function to allocate memory for pkcs11 test.
 *
 * @param[in] size Size in bytes.
 */
typedef void * ( * PkcsMalloc_t )( size_t size );

/**
 * @brief Malloc function to allocate memory for pkcs11 test.
 *
 * @param[in] size Size in bytes.
 */
typedef void ( * PkcsFree_t )( void *ptr );


typedef void * ( * ThreadFunction_t )( void * pParam );

typedef void * ThreadHandle_t;

typedef ThreadHandle_t ( * ThreadCreate_t )( ThreadFunction_t threadFunc, void * pParam );

typedef int ( * ThreadTimedJoin_t )( ThreadHandle_t threadHandle, uint32_t timeoutMs );


/**
 * @brief A struct representing core pkcs11 test parameters.
 */
typedef struct Pkcs11TestParam
{
    PkcsMalloc_t pPkcsMalloc;
    PkcsFree_t pPkcsFree;
    ThreadCreate_t pThreadCreate;
    ThreadTimedJoin_t pThreadTimedJoin;
} Pkcs11TestParam_t;

/**
 * @brief Customers need to implement this fuction by filling in parameters
 * in the provided TransportTestParam_t.
 *
 * @param[in] pTestParam a pointer to TransportTestParam_t struct to be filled out by
 * caller.
 */
void SetupPkcs11TestParam( Pkcs11TestParam_t * pTestParam );

/**
 * @brief Run Transport interface tests. This function should be called after calling `setupTransportTestParam()`.
 *
 * @return Negative value if the transport test execution config is not set. Otherwise,
 * number of failure test cases is returned.
 */
int RunPkcs11Test( void );

#endif
