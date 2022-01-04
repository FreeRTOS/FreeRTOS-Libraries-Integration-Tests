
#include "test_execution_config.h"

#if ( MQTT_TEST_ENABLED == 1 )
#include "mqtt_test.h"
#endif

#if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
#include "transport_interface_tests.h"
#endif

void runQualificationTest(void)
{
    #if ( MQTT_TEST_ENABLED == 1 )
        runMqttTest();
    #endif
    
    #if ( TRANSPORT_INTERFACE_TEST_ENABLED == 1 )
        // TODO: we need to convert runTransportInterfaceTest() to take no param.
        // Instead, param should be passed in from init.
        runTransportInterfaceTest();
    #endif
}
