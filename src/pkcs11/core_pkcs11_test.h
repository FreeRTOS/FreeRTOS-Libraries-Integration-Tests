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

#ifndef CORE_PKCS11_TEST_H
#define CORE_PKCS11_TEST_H

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Thread handle data structure definition.
 */
typedef void * PkcsTestThreadHandle_t;

/**
 * @brief Malloc function to allocate memory for pkcs11 test.
 *
 * @param[in] size Size in bytes.
 */
typedef void * ( * PkcsTestMalloc_t )( size_t size );

/**
 * @brief Free function to free memory allocated by PkcsMalloc_t function.
 *
 * @param[in] size Size in bytes.
 */
typedef void ( * PkcsTestFree_t )( void * ptr );

/**
 * @brief Thread function to be executed in ThreadCreate_t function.
 *
 * @param[in] pParam The pParam parameter passed in ThreadCreate_t function.
 */
typedef void ( * PkcsTestThreadFunction_t )( void * pParam );

/**
 * @brief Thread create function for core PKCS11 test.
 *
 * @param[in] threadFunc The thread function to be executed in the created thread.
 * @param[in] pParam The pParam parameter passed to the thread function pParam parameter.
 *
 * @return NULL if create thread failed. Otherwise, return the handle of the created thread.
 */
typedef PkcsTestThreadHandle_t ( * PkcsTestThreadCreate_t )( PkcsTestThreadFunction_t threadFunc,
                                                             void * pParam );

/**
 * @brief Timed waiting function to wait for the created thread exit.
 *
 * @param[in] threadHandle The handle of the created thread to be waited.
 * @param[in] timeoutMs The timeout value of to wait for the created thread exit.
 *
 * @return 0 if the thread exits within timeoutMs. Other value will be regarded as error.
 */
typedef int ( * PkcsTestThreadTimedWait_t )( PkcsTestThreadHandle_t threadHandle,
                                             uint32_t timeoutMs );

/**
 * @brief Delay function to wait for the key generation.
 *
 * @param[in] delayMs Delay in milliseconds.
 */
typedef void (* PkcsTestDelayFunc_t )( uint32_t delayMs );

/**
 * @brief A struct representing core pkcs11 test parameters.
 */
typedef struct Pkcs11TestParam
{
    PkcsTestMalloc_t pPkcsMalloc;
    PkcsTestFree_t pPkcsFree;
    PkcsTestThreadCreate_t pThreadCreate;
    PkcsTestThreadTimedWait_t pThreadTimedWait;
    PkcsTestDelayFunc_t pDelay;
} Pkcs11TestParam_t;

/**
 * @brief Customers need to implement this fuction by filling in parameters
 * in the provided Pkcs11TestParam_t.
 *
 * @param[in] pTestParam a pointer to Pkcs11TestParam_t struct to be filled out by
 * caller.
 */
void SetupPkcs11TestParam( Pkcs11TestParam_t * pTestParam );

/**
 * @brief Run corePKCS11 tests. This function should be called after calling `SetupPkcs11TestParam()`.
 *
 * @return Negative value if the core PKCS11 test execution config is not set. Otherwise,
 * number of failure test cases is returned.
 */
int RunPkcs11Test( void );

#endif /* ifndef CORE_PKCS11_TEST_H */
