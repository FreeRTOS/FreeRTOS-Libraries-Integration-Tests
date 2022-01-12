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

#ifndef TRANSPORT_INTERFACE_TESTS_H_
#define TRANSPORT_INTERFACE_TESTS_H_

/* Standard header includes. */
#include <stdint.h>

/* Include for transport interface. */
#include "transport_interface.h"

/**
 * @brief Hook function to initialize the transport interface.
 *
 * This function needs to be supplied by the application and is called at the
 * start of each test case. The application needs to ensure that the transport
 * interface can be used to send and receive data after this function returns - this,
 * in case of TCP/IP means, that a connection with the server is established.
 *
 * @param[in] pTransport The transport interface to init.
 *
 * @return 0 if transport interface init succes or other value to indicate error.
 */
int32_t TransportInit( TransportInterface_t * pTransport );

/**
 * @brief Hook function to de-initialize the transport interface.
 *
 * This function needs to be supplied by the application and is called at the
 * end of each test case. The application should free all the resources allocated in
 * TransportInit.
 *
 * @param[in] pTransport The transport interface to de-init.
 */
void TransportDeinit( TransportInterface_t * pTransport );

/**
 * @brief Delay function to wait for the data transfer over transport network.
 *
 * This function needs to be supplied by the application and is called during the
 * test to wait for the response from the transport network.
 *
 * @param[in] delayMs Delay in milliseconds.
 */
void TransportTestDelay( uint32_t delayMs );

/**
 * @brief Run transport interface tests.
 *
 * The application needs to pass supply the transport interface to be used in
 * tests.
 *
 * @param[in] pTransport transport interface to be used in the tests.
 */
void RunTransportInterfaceTests( TransportInterface_t * pTransport );

#endif /* TRANSPORT_INTERFACE_TESTS_H_ */
