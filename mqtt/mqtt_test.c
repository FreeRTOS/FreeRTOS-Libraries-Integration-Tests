#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "demo_config.h"
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "tls_freertos.h"
#include "clock.h"
#include "unity.h"

#include "mqtt_test.h"

/**
 * @brief Length of MQTT server host name.
 */
#define BROKER_ENDPOINT_LENGTH                  ( ( uint16_t ) ( sizeof( BROKER_ENDPOINT ) - 1 ) )

/**
 * @brief A valid starting packet ID per MQTT spec. Start from 1.
 */
#define MQTT_FIRST_VALID_PACKET_ID              ( 1 )

/**
 * @brief A PINGREQ packet is always 2 bytes in size, defined by MQTT 3.1.1 spec.
 */
#define MQTT_PACKET_PINGREQ_SIZE                ( 2U )

/**
 * @brief A packet type not handled by MQTT_ProcessLoop.
 */
#define MQTT_PACKET_TYPE_INVALID                ( 0U )

/**
 * @brief Number of milliseconds in a second.
 */
#define MQTT_ONE_SECOND_TO_MS                   ( 1000U )

/**
 * @brief Length of the MQTT network buffer.
 */
#define MQTT_TEST_BUFFER_LENGTH                 ( 128 )

/**
 * @brief Sample length of remaining serialized data.
 */
#define MQTT_SAMPLE_REMAINING_LENGTH            ( 64 )

/**
 * @brief Subtract this value from max value of global entry time
 * for the timer overflow test.
 */
#define MQTT_OVERFLOW_OFFSET                    ( 3 )

/**
 * @brief Sample topic filter to subscribe to.
 */
#define TEST_MQTT_TOPIC                         CLIENT_IDENTIFIER "/iot/integration/test"

/**
 * @brief Sample topic filter 2 to use in tests.
 */
#define TEST_MQTT_TOPIC_2                       CLIENT_IDENTIFIER "/iot/integration/test2"

/**
 * @brief Length of sample topic filter.
 */
#define TEST_MQTT_TOPIC_LENGTH                  ( sizeof( TEST_MQTT_TOPIC ) - 1 )

/**
 * @brief Sample topic filter to subscribe to.
 */
#define TEST_MQTT_LWT_TOPIC                     CLIENT_IDENTIFIER "/iot/integration/test/lwt"

/**
 * @brief Length of sample topic filter.
 */
#define TEST_MQTT_LWT_TOPIC_LENGTH              ( sizeof( TEST_MQTT_LWT_TOPIC ) - 1 )


/**
 * @brief Client identifier for MQTT session in the tests.
 */
#define TEST_CLIENT_IDENTIFIER                  CLIENT_IDENTIFIER

/**
 * @brief Length of the client identifier.
 */
#define TEST_CLIENT_IDENTIFIER_LENGTH           ( sizeof( TEST_CLIENT_IDENTIFIER ) - 1u )

/**
 * @brief Client identifier for use in LWT tests.
 */
#define TEST_CLIENT_IDENTIFIER_LWT              CLIENT_IDENTIFIER "-LWT"

/**
 * @brief Length of LWT client identifier.
 */
#define TEST_CLIENT_IDENTIFIER_LWT_LENGTH       ( sizeof( TEST_CLIENT_IDENTIFIER_LWT ) - 1u )

/**
 * @brief The largest random number to use in client identifier.
 *
 * @note Random number is added to MQTT client identifier to avoid client
 * identifier collisions while connecting to MQTT broker.
 */
#define MAX_RAND_NUMBER_FOR_CLIENT_ID           ( 999u )

/**
 * @brief Maximum number of random number digits in Client Identifier.
 * @note The value is derived from the #MAX_RAND_NUM_IN_FOR_CLIENT_ID.
 */
#define MAX_RAND_NUMBER_DIGITS_FOR_CLIENT_ID    ( 3u )


/**
 * @brief Timeout for receiving CONNACK packet in milli seconds.
 */
#define CONNACK_RECV_TIMEOUT_MS                 ( 1000U )

/**
 * @brief Time interval in seconds at which an MQTT PINGREQ need to be sent to
 * broker.
 */
#define MQTT_KEEP_ALIVE_INTERVAL_SECONDS        ( 5U )

/**
 * @brief Timeout for MQTT_ProcessLoop() function in milliseconds.
 * The timeout value is appropriately chosen for receiving an incoming
 * PUBLISH message and ack responses for QoS 1 and QoS 2 communications
 * with the broker.
 */
#define MQTT_PROCESS_LOOP_TIMEOUT_MS            ( 700U )

/**
 * @brief The MQTT message published in this example.
 */
#define MQTT_EXAMPLE_MESSAGE                    "Hello World!"

static TransportInterface_t * pTestTransport;
static TestHostInfo_t * pTestHostInfo;
static TestNetworkCredentials_t * pTestNetworkCredentials;
static void * pTestNetworkContext;

static Network_Connect_Func pTestNetworkConnect;
static Network_Disconnect_Func pTestNetworkDisconnect;


/**
 * @brief Packet Identifier generated when Subscribe request was sent to the broker;
 * it is used to match received Subscribe ACK to the transmitted subscribe.
 */
static uint16_t globalSubscribePacketIdentifier = 0U;

/**
 * @brief Packet Identifier generated when Unsubscribe request was sent to the broker;
 * it is used to match received Unsubscribe ACK to the transmitted unsubscribe
 * request.
 */
static uint16_t globalUnsubscribePacketIdentifier = 0U;

/**
 * @brief Packet Identifier generated when Publish request was sent to the broker;
 * it is used to match acknowledgement responses to the transmitted publish
 * request.
 */
static uint16_t globalPublishPacketIdentifier = 0U;

/**
 * @brief The context representing the MQTT connection with the broker for
 * the test case.
 */
static MQTTContext_t context;

/**
 * @brief Flag that represents whether a persistent session was resumed
 * with the broker for the test.
 */
static bool persistentSession = false;

/**
 * @brief Flag to indicate if LWT is being used when establishing a connection.
 */
static bool useLWTClientIdentifier = false;

/**
 * @brief Flag to represent whether a SUBACK is received from the broker.
 */
static bool receivedSubAck = false;

/**
 * @brief Flag to represent whether an UNSUBACK is received from the broker.
 */
static bool receivedUnsubAck = false;

/**
 * @brief Flag to represent whether a PUBACK is received from the broker.
 */
static bool receivedPubAck = false;

/**
 * @brief Flag to represent whether a PUBREC is received from the broker.
 */
static bool receivedPubRec = false;

/**
 * @brief Flag to represent whether a PUBREL is received from the broker.
 */
static bool receivedPubRel = false;

/**
 * @brief Flag to represent whether a PUBCOMP is received from the broker.
 */
static bool receivedPubComp = false;

/**
 * @brief Flag to represent whether an incoming PUBLISH packet is received
 * with the "retain" flag set.
 */
static bool receivedRetainedMessage = false;

/**
 * @brief Represents incoming PUBLISH information.
 */
static MQTTPublishInfo_t incomingInfo;

/**
 * @brief Disconnect when receiving this packet type. Used for session
 * restoration tests.
 */
static uint8_t packetTypeForDisconnection = MQTT_PACKET_TYPE_INVALID;

/**
 * @brief Random number for the client identifier of the MQTT connection(s) in
 * the test.
 *
 * Random number is used to avoid client identifier collisions while connecting
 * to MQTT broker.
 */
static int clientIdRandNumber;


/**
 * @brief Sends an MQTT CONNECT packet over the already connected TCP socket.
 *
 * @param[in] pContext MQTT context pointer.
 * @param[in] createCleanSession Creates a new MQTT session if true.
 * If false, tries to establish the existing session if there was session
 * already present in broker.
 * @param[out] pSessionPresent Session was already present in the broker or not.
 * Session present response is obtained from the CONNACK from broker.
 */
static void establishMqttSession( MQTTContext_t * pContext,
                                  bool createCleanSession,
                                  bool * pSessionPresent );

/**
 * @brief Handler for incoming acknowledgement packets from the broker.
 * @param[in] pPacketInfo Info for the incoming acknowledgement packet.
 * @param[in] packetIdentifier The ID of the incoming packet.
 */
static void handleAckEvents( MQTTPacketInfo_t * pPacketInfo,
                             uint16_t packetIdentifier );

/**
 * @brief The application callback function that is expected to be invoked by the
 * MQTT library for incoming publish and incoming acks received over the network.
 *
 * @param[in] pContext MQTT context pointer.
 * @param[in] pPacketInfo Packet Info pointer for the incoming packet.
 * @param[in] pDeserializedInfo Deserialized information from the incoming packet.
 */
static void eventCallback( MQTTContext_t * pContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo );


static void establishMqttSession( MQTTContext_t * pContext,
                                  bool createCleanSession,
                                  bool * pSessionPresent )
{
    MQTTConnectInfo_t connectInfo;
    MQTTFixedBuffer_t networkBuffer;

    assert( pContext != NULL );

    /* The network buffer must remain valid for the lifetime of the MQTT context. */
    static uint8_t buffer[ NETWORK_BUFFER_SIZE ];

    /* Buffer for storing client ID with random integer.
     * Note: Size value is chosen to accommodate both LWT and non-LWT client ID
     * strings along with NULL character.*/
    char clientIdBuffer[ TEST_CLIENT_IDENTIFIER_LWT_LENGTH +
                         MAX_RAND_NUMBER_DIGITS_FOR_CLIENT_ID + 1u ] = { 0 };

    /* Fill the values for network buffer. */
    networkBuffer.pBuffer = buffer;
    networkBuffer.size = NETWORK_BUFFER_SIZE;

    /* Clear the state of the MQTT context when creating a clean session. */
    if( createCleanSession == true )
    {
        /* Initialize MQTT library. */
        TEST_ASSERT_EQUAL( MQTTSuccess, MQTT_Init( pContext,
                                                   pTestTransport,
                                                   Clock_GetTimeMs,
                                                   eventCallback,
                                                   &networkBuffer ) );
    }

    /* Establish MQTT session with a CONNECT packet. */

    connectInfo.cleanSession = createCleanSession;

    /* Populate client identifier with random number. */
    connectInfo.clientIdentifierLength =
        snprintf( clientIdBuffer,
                  sizeof( clientIdBuffer ),
                  "%d%s", clientIdRandNumber,
                  TEST_CLIENT_IDENTIFIER );
    connectInfo.pClientIdentifier = clientIdBuffer;

    LogDebug( ( "Created randomized client ID for MQTT connection: ClientID={%.*s}", connectInfo.clientIdentifierLength,
                connectInfo.pClientIdentifier ) );

    /* The interval at which an MQTT PINGREQ needs to be sent out to broker. */
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_SECONDS;

    /* Username and password for authentication. Not used in this test. */
    connectInfo.pUserName = NULL;
    connectInfo.userNameLength = 0U;
    connectInfo.pPassword = NULL;
    connectInfo.passwordLength = 0U;

    /* Send MQTT CONNECT packet to broker. */
    TEST_ASSERT_EQUAL( MQTTSuccess, MQTT_Connect( pContext,
                                                  &connectInfo,
                                                  NULL,
                                                  CONNACK_RECV_TIMEOUT_MS,
                                                  pSessionPresent ) );
}

static void eventCallback( MQTTContext_t * pContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo )
{
    MQTTPublishInfo_t * pPublishInfo = NULL;

    assert( pContext != NULL );
    assert( pPacketInfo != NULL );
    assert( pDeserializedInfo != NULL );

    /* Suppress unused parameter warning when asserts are disabled in build. */
    ( void ) pContext;

    TEST_ASSERT_EQUAL( MQTTSuccess, pDeserializedInfo->deserializationResult );
    pPublishInfo = pDeserializedInfo->pPublishInfo;

    if( ( pPacketInfo->type == packetTypeForDisconnection ) ||
        ( ( pPacketInfo->type & 0xF0U ) == packetTypeForDisconnection ) )
    {
        /* Terminate TLS session and TCP connection to test session restoration
         * across network connection. */
         (*pTestNetworkDisconnect)( pTestNetworkContext );
    }
    else
    {
        /* Handle incoming publish. The lower 4 bits of the publish packet
         * type is used for the dup, QoS, and retain flags. Hence masking
         * out the lower bits to check if the packet is publish. */
        if( ( pPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
        {
            assert( pPublishInfo != NULL );
            /* Handle incoming publish. */

            /* Cache information about the incoming PUBLISH message to process
             * in test case. */
            memcpy( &incomingInfo, pPublishInfo, sizeof( MQTTPublishInfo_t ) );
            incomingInfo.pTopicName = NULL;
            incomingInfo.pPayload = NULL;
            /* Allocate buffers and copy information of topic name and payload. */
            incomingInfo.pTopicName = malloc( pPublishInfo->topicNameLength );
            TEST_ASSERT_NOT_NULL( incomingInfo.pTopicName );
            memcpy( ( void * ) incomingInfo.pTopicName, pPublishInfo->pTopicName, pPublishInfo->topicNameLength );
            incomingInfo.pPayload = malloc( pPublishInfo->payloadLength );
            TEST_ASSERT_NOT_NULL( incomingInfo.pPayload );
            memcpy( ( void * ) incomingInfo.pPayload, pPublishInfo->pPayload, pPublishInfo->payloadLength );

            /* Update the global variable if the incoming PUBLISH packet
             * represents a retained message. */
            receivedRetainedMessage = pPublishInfo->retain;
        }
        else
        {
            handleAckEvents( pPacketInfo, pDeserializedInfo->packetIdentifier );
        }
    }
}

static void handleAckEvents( MQTTPacketInfo_t * pPacketInfo,
                             uint16_t packetIdentifier )
{
    /* Handle other packets. */
    switch( pPacketInfo->type )
    {
        case MQTT_PACKET_TYPE_SUBACK:
            /* Set the flag to represent reception of SUBACK. */
            receivedSubAck = true;

            LogDebug( ( "Received SUBACK: PacketID=%u",
                        packetIdentifier ) );
            /* Make sure ACK packet identifier matches with Request packet identifier. */
            TEST_ASSERT_EQUAL( globalSubscribePacketIdentifier, packetIdentifier );
            break;

        case MQTT_PACKET_TYPE_PINGRESP:

            /* Nothing to be done from application as library handles
             * PINGRESP. */
            LogDebug( ( "Received PINGRESP" ) );
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            /* Set the flag to represent reception of UNSUBACK. */
            receivedUnsubAck = true;

            LogDebug( ( "Received UNSUBACK: PacketID=%u",
                        packetIdentifier ) );
            /* Make sure ACK packet identifier matches with Request packet identifier. */
            TEST_ASSERT_EQUAL( globalUnsubscribePacketIdentifier, packetIdentifier );
            break;

        case MQTT_PACKET_TYPE_PUBACK:
            /* Set the flag to represent reception of PUBACK. */
            receivedPubAck = true;

            /* Make sure ACK packet identifier matches with Request packet identifier. */
            TEST_ASSERT_EQUAL( globalPublishPacketIdentifier, packetIdentifier );

            LogDebug( ( "Received PUBACK: PacketID=%u",
                        packetIdentifier ) );
            break;

        case MQTT_PACKET_TYPE_PUBREC:
            /* Set the flag to represent reception of PUBREC. */
            receivedPubRec = true;

            /* Make sure ACK packet identifier matches with Request packet identifier. */
            TEST_ASSERT_EQUAL( globalPublishPacketIdentifier, packetIdentifier );

            LogDebug( ( "Received PUBREC: PacketID=%u",
                        packetIdentifier ) );
            break;

        case MQTT_PACKET_TYPE_PUBREL:
            /* Set the flag to represent reception of PUBREL. */
            receivedPubRel = true;

            /* Nothing to be done from application as library handles
             * PUBREL. */
            LogDebug( ( "Received PUBREL: PacketID=%u",
                        packetIdentifier ) );
            break;

        case MQTT_PACKET_TYPE_PUBCOMP:
            /* Set the flag to represent reception of PUBACK. */
            receivedPubComp = true;

            /* Make sure ACK packet identifier matches with Request packet identifier. */
            TEST_ASSERT_EQUAL( globalPublishPacketIdentifier, packetIdentifier );

            /* Nothing to be done from application as library handles
             * PUBCOMP. */
            LogDebug( ( "Received PUBCOMP: PacketID=%u",
                        packetIdentifier ) );
            break;

        /* Any other packet type is invalid. */
        default:
            LogError( ( "Unknown packet type received:(%02x).",
                        pPacketInfo->type ) );
    }
}

static MQTTStatus_t subscribeToTopic( MQTTContext_t * pContext,
                                      const char * pTopic,
                                      MQTTQoS_t qos )
{
    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];

    assert( pContext != NULL );

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopic;
    pSubscriptionList[ 0 ].topicFilterLength = strlen( pTopic );

    /* Generate packet identifier for the SUBSCRIBE packet. */
    globalSubscribePacketIdentifier = MQTT_GetPacketId( pContext );

    /* Send SUBSCRIBE packet. */
    return MQTT_Subscribe( pContext,
                           pSubscriptionList,
                           sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                           globalSubscribePacketIdentifier );
}

static MQTTStatus_t unsubscribeFromTopic( MQTTContext_t * pContext,
                                          const char * pTopic,
                                          MQTTQoS_t qos )
{
    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];

    assert( pContext != NULL );

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    pSubscriptionList[ 0 ].qos = qos;
    pSubscriptionList[ 0 ].pTopicFilter = pTopic;
    pSubscriptionList[ 0 ].topicFilterLength = strlen( pTopic );

    /* Generate packet identifier for the UNSUBSCRIBE packet. */
    globalUnsubscribePacketIdentifier = MQTT_GetPacketId( pContext );

    /* Send UNSUBSCRIBE packet. */
    return MQTT_Unsubscribe( pContext,
                             pSubscriptionList,
                             sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                             globalUnsubscribePacketIdentifier );
}

static MQTTStatus_t publishToTopic( MQTTContext_t * pContext,
                                    const char * pTopic,
                                    bool setRetainFlag,
                                    bool isDuplicate,
                                    MQTTQoS_t qos,
                                    uint16_t packetId )
{
    assert( pContext != NULL );
    MQTTPublishInfo_t publishInfo;

    publishInfo.retain = setRetainFlag;

    publishInfo.qos = qos;
    publishInfo.dup = isDuplicate;
    publishInfo.pTopicName = pTopic;
    publishInfo.topicNameLength = strlen( pTopic );
    publishInfo.pPayload = MQTT_EXAMPLE_MESSAGE;
    publishInfo.payloadLength = strlen( MQTT_EXAMPLE_MESSAGE );

    /* Get a new packet id. */
    globalPublishPacketIdentifier = packetId;

    /* Send PUBLISH packet. */
    return MQTT_Publish( pContext,
                         &publishInfo,
                         packetId );
}

void setUp(void) {
    struct timespec tp;

    /* Reset file-scoped global variables. */
    receivedSubAck = false;
    receivedUnsubAck = false;
    receivedPubAck = false;
    receivedPubRec = false;
    receivedPubRel = false;
    receivedPubComp = false;
    receivedRetainedMessage = false;
    persistentSession = false;
    useLWTClientIdentifier = false;
    packetTypeForDisconnection = MQTT_PACKET_TYPE_INVALID;
    memset( &incomingInfo, 0u, sizeof( MQTTPublishInfo_t ) );
    /* memset( &opensslParams, 0u, sizeof( OpensslParams_t ) ); */

    /* Get current time to seed pseudo random number generator. */
    ( void ) clock_gettime( CLOCK_REALTIME, &tp );

    /* Seed pseudo random number generator with nanoseconds. */
    srand( tp.tv_nsec );

    /* Generate a random number to use in the client identifier. */
    clientIdRandNumber = ( rand() % ( MAX_RAND_NUMBER_FOR_CLIENT_ID + 1u ) );

    /* Establish a TCP connection with the server endpoint, then
     * establish TLS session on top of TCP connection. */
    TEST_ASSERT_EQUAL( pdPASS, (*pTestNetworkConnect)( pTestNetworkContext,
                pTestHostInfo, pTestNetworkCredentials ) );

    /* Establish MQTT session on top of the TCP+TLS connection. */
    establishMqttSession( &context, true, &persistentSession );
}

void tearDown(void) {
    MQTTStatus_t mqttStatus;

    /* Free memory, if allocated during test case execution. */
    if( incomingInfo.pTopicName != NULL )
    {
        free( ( void * ) incomingInfo.pTopicName );
    }

    if( incomingInfo.pPayload != NULL )
    {
        free( ( void * ) incomingInfo.pPayload );
    }

    /* Terminate MQTT connection. */
    mqttStatus = MQTT_Disconnect( &context );

    (*pTestNetworkDisconnect)( pTestNetworkContext );

    /* Make any assertions at the end so that all memory is deallocated before
     * the end of this function. */
    TEST_ASSERT_EQUAL( MQTTSuccess, mqttStatus );
}

void setupMqttTest( MqttTestParam_t * pTestParam,
                    Network_Connect_Func pNetworkConnect,
                    Network_Disconnect_Func pNetworkDisconnect )
{
    pTestTransport = pTestParam->pTransport;
    pTestHostInfo = pTestParam->pHostInfo;
    pTestNetworkCredentials = pTestParam->pNetworkCredentials;
    pTestNetworkContext = pTestParam->pNetworkContext;
    pTestNetworkConnect = pNetworkConnect;
    pTestNetworkDisconnect = pNetworkDisconnect;
}

void test_MQTT_Subscribe_Publish_With_Qos_0( void )
{
    /* Subscribe to a topic with Qos 0. */
    TEST_ASSERT_EQUAL( MQTTSuccess, subscribeToTopic(
                           &context, TEST_MQTT_TOPIC, MQTTQoS0 ) );

    /* We expect a SUBACK from the broker for the subscribe operation. */
    TEST_ASSERT_FALSE( receivedSubAck );
    TEST_ASSERT_EQUAL( MQTTSuccess,
                       MQTT_ProcessLoop( &context, MQTT_PROCESS_LOOP_TIMEOUT_MS ) );
    TEST_ASSERT_TRUE( receivedSubAck );

    /* Publish to the same topic, that we subscribed to, with Qos 0. */
    TEST_ASSERT_EQUAL( MQTTSuccess, publishToTopic(
                           &context,
                           TEST_MQTT_TOPIC,
                           false, /* setRetainFlag */
                           false, /* isDuplicate */
                           MQTTQoS0,
                           MQTT_GetPacketId( &context ) ) );

    /* Call the MQTT library for the expectation to read an incoming PUBLISH for
     * the same message that we published (as we have subscribed to the same topic). */
    TEST_ASSERT_FALSE( receivedPubAck );
    TEST_ASSERT_EQUAL( MQTTSuccess,
                       MQTT_ProcessLoop( &context, MQTT_PROCESS_LOOP_TIMEOUT_MS ) );
    /* We do not expect a PUBACK from the broker for the QoS 0 PUBLISH. */
    TEST_ASSERT_FALSE( receivedPubAck );

    /* Make sure that we have received the same message from the server,
     * that was published (as we have subscribed to the same topic). */
    TEST_ASSERT_EQUAL( MQTTQoS0, incomingInfo.qos );
    TEST_ASSERT_EQUAL( TEST_MQTT_TOPIC_LENGTH, incomingInfo.topicNameLength );
    TEST_ASSERT_EQUAL_MEMORY( TEST_MQTT_TOPIC,
                              incomingInfo.pTopicName,
                              TEST_MQTT_TOPIC_LENGTH );
    TEST_ASSERT_EQUAL( strlen( MQTT_EXAMPLE_MESSAGE ), incomingInfo.payloadLength );
    TEST_ASSERT_EQUAL_MEMORY( MQTT_EXAMPLE_MESSAGE,
                              incomingInfo.pPayload,
                              incomingInfo.payloadLength );

    /* Un-subscribe from a topic with Qos 0. */
    TEST_ASSERT_EQUAL( MQTTSuccess, unsubscribeFromTopic(
                           &context, TEST_MQTT_TOPIC, MQTTQoS0 ) );

    /* We expect an UNSUBACK from the broker for the unsubscribe operation. */
    TEST_ASSERT_EQUAL( MQTTSuccess,
                       MQTT_ProcessLoop( &context, MQTT_PROCESS_LOOP_TIMEOUT_MS ) );
    TEST_ASSERT_TRUE( receivedUnsubAck );
}

int runMqttTest()
{
    UNITY_BEGIN();
    RUN_TEST(test_MQTT_Subscribe_Publish_With_Qos_0);
    return UNITY_END();
}
