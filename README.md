## FreeRTOS-Libraries-Integration-Tests

### Overview
This repository contains tests that verify an integration of FreeRTOS IoT libraries
running on a specific microcontroller-based development board for robustness and
compatibility with AWS’s published best practices for AWS IoT Core connectivity.
Refer to [FreeRTOS Porting Guide](https://docs.aws.amazon.com/freertos/latest/portingguide/afr-porting.html) for how to port FreeRTOS libraries and test your project.
The tests are used by [AWS IoT Device Tester](https://docs.aws.amazon.com/freertos/latest/userguide/device-tester-for-freertos-ug.html)
as part of the [AWS Device Qualification for FreeRTOS](https://docs.aws.amazon.com/freertos/latest/qualificationguide/afr-qualification.html).

### Tests
The following test groups are included in this repository:
1. Transport Interface Test validates the implementation of transport interface. The implementation can be plain text or TLS. See [transport interface tests](/src/transport_interface) for details.
2. PKCS11 Test validates the implementation of PKCS11 interface required by [corePKCS11](https://docs.aws.amazon.com/freertos/latest/portingguide/afr-porting-pkcs.html).
3. OTA Test validates the implementation of Physical Abstract Layer for [Over-the-Air Updates](https://docs.aws.amazon.com/freertos/latest/portingguide/afr-porting-ota.html).
4. MQTT Test validates the integration with coreMQTT library.


### Folder Structure
The folder inside the repository is organized as follows:
```
├── config_template
├── src
│   ├── common
|   |── mqtt
|   |── ota
|   |── pkcs11
│   └── transport_interface
└── tools
```
The root of the repository contains following top level folders:
1. `config_template` contains configuration header templates. The templates should be copied into the parent project.
If running the tests with IDT, there is no need to edit the
copied templates. If running the tests without IDT, users need to fill in configuration values in the copied templates.
2. `src` contains source code for the tests. Each test set is contained in a subfolder inside `src`.
Refer to ReadMe in each subfolder for details of the test group, test cases and how to run these tests.
3. `tools` contains utility tools for the tests, such as echo server for Transport Interface Test.

### License
This library is distributed under the [MIT Open Source License](LICENSE).

