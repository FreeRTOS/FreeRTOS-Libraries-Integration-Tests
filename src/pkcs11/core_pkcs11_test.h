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

/**
 * @file core_pkcs11_test.h
 * @brief Test function for corePKCS11 test.
 */
#ifndef CORE_PKCS11_TEST_H
#define CORE_PKCS11_TEST_H

/* Standard includes. */
#include <stdint.h>
#include <stdlib.h>

/* Thread function includes. */
#include "thread_function.h"

/* Memory function includes. */
#include "memory_function.h"

/**
 * @brief A struct representing core pkcs11 test parameters.
 */
typedef struct Pkcs11TestParam
{
    MemoryAlloc_t pMemoryAlloc;
    MemoryFree_t pMemoryFree;
    ThreadCreate_t pThreadCreate;
    ThreadTimedJoin_t pThreadTimedJoin;
    ThreadDelayFunc_t pThreadDelay;
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
