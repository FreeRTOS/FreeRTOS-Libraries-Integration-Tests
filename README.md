## Labs-FreeRTOS-Libraries-Integration-Tests

### Overview
This repository contains tests that verify an integration of FreeRTOS IoT libraries
running on a specific microcontroller-based development board for robustness and
compatibility with AWS’s published best practices for AWS IoT Core connectivity.
The tests are used by [AWS IoT Device Tester](https://docs.aws.amazon.com/freertos/latest/userguide/device-tester-for-freertos-ug.html)
as part of the [AWS Device Qualification for FreeRTOS](https://docs.aws.amazon.com/freertos/latest/qualificationguide/afr-qualification.html).

### Tests
There are two test groups in this repository:
1. Transport Interface Test validates the implementation of transport interface. The implementation can be plain text or TLS.
2. MQTT Test validates the integration with coreMQTT library.


### Folder Structure
The folder inside the repository is organized as follows:
```
├── config_template
├── src
│   ├── common
│   ├── transport_interface
│   └── mqtt
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

