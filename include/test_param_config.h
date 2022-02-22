#ifndef _TEST_PARAM_CONFIG_H_
#define _TEST_PARAM_CONFIG_H_

#define FORCE_GENERATE_NEW_KEY_PAIR   0

#define MQTT_SERVER_ENDPOINT   "PLACE_HOLDER"
#define MQTT_SERVER_PORT       (8883)

#define ECHO_SERVER_ENDPOINT   "PLACE_HOLDER"
#define ECHO_SERVER_PORT       (9000)

/* Root certificate of the echo server.
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 */
#define ECHO_SERVER_ROOT_CA "PLACE_HOLDER"

/* Client certificate to connect to echo server.
 * @note This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 */
#define TRANSPORT_CLIENT_CA NULL

/* Client private key to connect to echo server.
 * @note This is should only be used for testing purpose.
 * For qualification, the key should be generated on-device.
 */
#define TRANSPORT_CLIENT_KEY  NULL


#endif /* _TEST_PARAM_CONFIG_H_ */
