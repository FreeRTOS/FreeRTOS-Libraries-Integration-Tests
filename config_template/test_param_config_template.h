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
 * @file test_param_config.h
 * @brief This setup the test parameters for LTS qualification test.
 */

#ifndef TEST_PARAM_CONFIG_H
#define TEST_PARAM_CONFIG_H

/**
 * @brief Configuration that indicates if the device should generate a key pair.
 *
 * @note When FORCE_GENERATE_NEW_KEY_PAIR is set to 1, the device should generate
 * a new on-device key pair and output public key. When set to 0, the device
 * should keep existing key pair.
 *
 * #define FORCE_GENERATE_NEW_KEY_PAIR   0
 */

/**
 * @brief Endpoint of the MQTT broker to connect to in mqtt test.
 *
 * #define MQTT_SERVER_ENDPOINT   "PLACE_HOLDER"
 */

/**
 * @brief Port of the MQTT broker to connect to in mqtt test.
 *
 * #define MQTT_SERVER_PORT       (8883)
 */

/**
 * @brief The client identifier for MQTT test.
 *
 * #define MQTT_TEST_CLIENT_IDENTIFIER    "PLACE_HOLDER"
 */

 /**
 * @brief Network buffer size specified in bytes. Must be large enough to hold the maximum
 * anticipated MQTT payload.
 *
 * #define MQTT_TEST_NETWORK_BUFFER_SIZE  ( 5000 )
 */

 /**
 * @brief Timeout for MQTT_ProcessLoop() function in milliseconds.
 * The timeout value is appropriately chosen for receiving an incoming
 * PUBLISH message and ack responses for QoS 1 and QoS 2 communications
 * with the broker.
 *
 * #define MQTT_TEST_PROCESS_LOOP_TIMEOUT_MS  ( 700 )
 */

/**
 * @brief Root certificate of the IoT Core.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define IOT_CORE_ROOT_CA NULL
 */

/**
 * @brief Client certificate to connect to MQTT server.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define MQTT_CLIENT_CERTIFICATE NULL
 */

/**
 * @brief Client private key to connect to MQTT server.
 *
 * @note This is should only be used for testing purpose.
 *
 * For qualification, the key should be generated on-device.
 *
 * #define MQTT_CLIENT_PRIVATE_KEY  NULL
 */

/**
 * @brief Endpoint of the echo server to connect to in transport interface test.
 *
 * #define ECHO_SERVER_ENDPOINT   "PLACE_HOLDER"
 */

/**
 * @brief Port of the echo server to connect to in transport interface test.
 *
 * #define ECHO_SERVER_PORT       (9000)
 */

/**
 * @brief Root certificate of the echo server.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define ECHO_SERVER_ROOT_CA "PLACE_HOLDER"
 */

/**
 * @brief Client certificate to connect to echo server.
 *
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 *
 * #define TRANSPORT_CLIENT_CERTIFICATE NULL
 */

/**
 * @brief Client private key to connect to echo server.
 *
 * @note This is should only be used for testing purpose.
 *
 * For qualification, the key should be generated on-device.
 *
 * #define TRANSPORT_CLIENT_PRIVATE_KEY  NULL
 */

/**
 * @brief The PKCS #11 supports RSA key function.
 *
 * Set to 1 if RSA private keys are supported by the platform. 0 if not.
 *
 * #define PKCS11_TEST_RSA_KEY_SUPPORT                     ( 1 )
 */

/**
 * @brief The PKCS #11 supports EC key function.
 *
 * Set to 1 if elliptic curve private keys are supported by the platform. 0 if not.
 *
 * #define PKCS11_TEST_EC_KEY_SUPPORT                      ( 1 )
 */

/**
 * @brief The PKCS #11 supports import key method.
 *
 * Set to 1 if importing device private key via C_CreateObject is supported. 0 if not.
 *
 * #define PKCS11_TEST_IMPORT_PRIVATE_KEY_SUPPORT          ( 1 )
 */

/**
 * @brief The PKCS #11 supports generate keypair method.
 *
 * Set to 1 if generating a device private-public key pair via C_GenerateKeyPair. 0 if not.
 *
 * #define PKCS11_TEST_GENERATE_KEYPAIR_SUPPORT            ( 1 )
 */

/**
 * @brief The PKCS #11 supports preprovisioning method.
 *
 * Set to 1 if preprovisioning is supported.
 *
 * #define PKCS11_TEST_PREPROVISIONED_SUPPORT              ( 0 )
 */

/**
 * @brief The PKCS #11 label for device private key for test.
 *
 * For devices with on-chip storage, this should match the non-test label.
 * For devices with secure elements or hardware limitations, this may be defined
 * to a different label to preserve AWS IoT credentials for other test suites.
 *
 * #define PKCS11_TEST_LABEL_DEVICE_PRIVATE_KEY_FOR_TLS    pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS
 */

/**
 * @brief The PKCS #11 label for device public key.
 *
 * For devices with on-chip storage, this should match the non-test label.
 * For devices with secure elements or hardware limitations, this may be defined
 * to a different label to preserve AWS IoT credentials for other test suites.
 *
 * #define PKCS11_TEST_LABEL_DEVICE_PUBLIC_KEY_FOR_TLS     pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
 */

/**
 * @brief The PKCS #11 label for the device certificate.
 *
 * For devices with on-chip storage, this should match the non-test label.
 * For devices with secure elements or hardware limitations, this may be defined
 * to a different label to preserve AWS IoT credentials for other test suites.
 *
 * #define PKCS11_TEST_LABEL_DEVICE_CERTIFICATE_FOR_TLS    pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS
 */

/**
 * @brief The PKCS #11 supports storage for JITP.
 *
 * The PKCS11 test will verify the following labels with create/destroy objects.
 * PKCS11_TEST_LABEL_CODE_VERIFICATION_KEY
 * PKCS11_TEST_LABEL_JITP_CERTIFICATE
 * PKCS11_TEST_LABEL_ROOT_CERTIFICATE
 * Set to 1 if PKCS #11 supports storage for JITP certificate, code verify certificate,
 * and trusted server root certificate.
 *
 * #define PKCS11_TEST_JITP_CODEVERIFY_ROOT_CERT_SUPPORTED pkcs11configJITP_CODEVERIFY_ROOT_CERT_SUPPORTED
 */

/**
 * @brief The PKCS #11 label for the object to be used for code verification.
 *
 * This label has to be defined if PKCS11_TEST_JITP_CODEVERIFY_ROOT_CERT_SUPPORTED is set to 1.
 *
 * #define PKCS11_TEST_LABEL_CODE_VERIFICATION_KEY    pkcs11configLABEL_CODE_VERIFICATION_KEY
 */

/**
 * @brief The PKCS #11 label for Just-In-Time-Provisioning.
 *
 * This label has to be defined if PKCS11_TEST_JITP_CODEVERIFY_ROOT_CERT_SUPPORTED is set to 1.
 *
 * #define PKCS11_TEST_LABEL_JITP_CERTIFICATE    pkcs11configLABEL_JITP_CERTIFICATE
 */

/**
 * @brief The PKCS #11 label for the AWS Trusted Root Certificate.
 *
 * This label has to be defined if PKCS11_TEST_JITP_CODEVERIFY_ROOT_CERT_SUPPORTED is set to 1.
 *
 * #define PKCS11_TEST_LABEL_ROOT_CERTIFICATE    pkcs11configLABEL_ROOT_CERTIFICATE
 */

#endif /* TEST_PARAM_CONFIG_H */
