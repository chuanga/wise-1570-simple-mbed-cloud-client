# WISE-1570 Simple Mbed Cloud Client application

## Overview

This is a template application for platform vendors. It demonstrates how to create a simple application that can connect to the Pelion IoT Platform service, register resources and get ready to receive a firmware update.

It's intended to be forked and customized to add platform-specific features (such as sensors and actuators) and configure the connectivity and storage to work **out-of-the-box**. The template application works in **developer mode** by default.

There is a mirror version of the stable (master) template application on [this location](https://os.mbed.com/teams/mbed-os-examples/code/mbed-cloud-example) to facilitate the fork and publish on os.mbed.com.

## Table of Contents

This application is customized from the simple Mbed Cloud Client application [here](https://github.com/ARMmbed/simple-mbed-cloud-client-template-restricted). It has been configured to work **out-of-the-box** for Advantech's [WISE-1570 NBIoT module](https://os.mbed.com/modules/advantech-wise-1570/). The Simple Mbed Cloud Client application works in **developer mode** by default.

## Getting started with the application

This is a summary of the process for developers to get started and get a device connected to Pelion Device Management Client.

### Mbed CLI tools

1. Import the application into your desktop:

    ```
    mbed import --protocol ssh https://github.com/chuanga/wise-1570-simple-mbed-cloud-client-restricted
    cd wise-1570-simple-mbed-cloud-restricted
    ```
2. Download the developer certificate from Mbed Cloud.
3. Compile and program:

    ```
    mbed compile -t GCC_ARM -m MTB_ADV_WISE_1570 -c
    ```
4. Combine the program binary with the bootloader (a pre-built bootloader for WISE-1570 has been provided for you)
    ```
    tools/combine_bootloader_with_app.py -b mbed-bootloader-internal.hex -m MTB_ADV_WISE_1570 -a BUILD/MTB_ADV_WISE_1570/GCC_ARM/wise-1570-simple-mbed-cloud-client-restricted.hex -o combined.hex
    ```
## Porting to a new platform

Please refer to the [Simple Mbed Cloud Client template application](https://github.com/ARMmbed/simple-mbed-cloud-client-template-restricted) if you want to customize the program for different platform components or another platform. 

#### Update the application logic

The template example uses a ticker object to periodically fire a software interrupt to simulate button presses. Let’s say you want to make an actual button press.

By default, there is a Ticker object, which fires every five seconds and invokes a callback function:

```cpp
Ticker timer;
timer.attach(eventQueue.event(&fake_button_press), 5.0);
```

This callback function changes the `button_res` resource:

```cpp
void fake_button_press() {
    int v = button_res->get_value_int() + 1;

    button_res->set_value(v);

    printf("Simulated button clicked %d times\n", v);
}
```

If you want to change this to an actual button, here is how to do it:

1. Remove:

    ```cpp
    Ticker timer;
    timer.attach(eventQueue.event(&fake_button_press), 5.0);
    ```

2. Declare an `InterruptIn` object on the button, and attach the callback function to the `fall` handler:

    ```cpp
    InterruptIn btn(BUTTON1);
    btn.fall(eventQueue.event(&fake_button_press), 5.0);
    ```

3. Rename `fake_button_press` to `real_button_press`.


#### Pelion Client v1.3.x SOTP-specific changes

The version v1.3+ introduces a new feature called Software One-Time Programming (SOTP) that makes use of the internal flash of the MCU as an One-Time-Programmable section. It stores the keys required to decrypt the credentials stored in the persistent storage. Read more on this in the [porting documentation](https://cloud.mbed.com/docs/current/porting/changing-a-customized-porting-layer.html#rtos-module) under the RTOS module section.

The flash must be divided into two sections (default 2, maximum 2) for your target. You need to modify the `mbed_app.json` file to specify SOTP regions. A working version has been provided for the WISE-1570 module.

## Enabling firmware updates

Mbed OS 5.10 and Mbed CLI 1.8 simplifies the process to enable and perform Firmware Updates. Here is a summary on how to configure the device and verify its correct behaviour. 

##### Option 1: default & prebuilt bootloader
A pre-built bootloader for WISE-1570 has been provided in this repository.

If Mbed OS contains a prebuilt bootloader for the target, then you can indicate to use it in the mbed_app.json. For example:
```
	"target_overrides": {
            "K64F": {
                "target.features_add": ["BOOTLOADER"]
            }
        }
```

##### Option 2: custom bootloader

If you'd like to overide a default bootloader or use a custom one available in the application, then indicate the path to the booloader, `app_offset` and `header_offset` parameters in `mbed_app.json`. For example:

```
    "target_overrides": {
            "K64F": {
                "target.app_offset": "0xa400",
                "target.header_offset": "0xa000",
                "target.bootloader_img": "bootloader/my_bootloader.bin"
            }
        }
```

You may need to specify `header_format` as well. You could include the default header format from [Mbed OS](https://github.com/ARMmbed/mbed-os/blob/master/features/FEATURE_BOOTLOADER/mbed_lib.json) by adding `"target.features_add": ["BOOTLOADER"]`.

#### Verifying that firmware update works

Follow these steps to generate a manifest, compile and perform a firmware update of your device:

1. Configure the API key for your Pelion account.

     If you don't have an API key available, then login in [Pelion IoT Platform portal](https://portal.mbedcloud.com/), navigate to 'Access Management', 'API keys' and create a new one. Then specify the API key as global `mbed` configuration:
	 ```
	 mbed config -G CLOUD_SDK_API_KEY <your-api-key>
	 ```

2. Initialize the device management feature:
    ```
	mbed dm init -d "company.com" --model-name "product-model" -q --force
	```
3. Compile the application, include the firmware update credentials generated before, merge with the bootloader and progrma the device.
	```
    $ mbed compile -m YOUR_TARGET -t GCC_ARM
    $ tools/combine_bootloader_with_app.py -m YOUR_TARGET -a BUILD/YOUR_TARGET/GCC_ARM/wise-1570-simple-mbed-cloud-client-restricted.hex -o combined.hex
    ```
4. Open a serial terminal, verify the application boots and is able to register for the Device Management service. Write down the <endpoint ID>, as it's required to identify the device to perform a firmware update.

5. Update the firmware of the device through Mbed CLI:

    ```
    mbed dm update device -D <device ID> -t <toolchain> -m <target>
    ```

    Inspect the logs on the device to see the update progress. It should look similar to:

    ```
    Firmware download requested
    Authorization granted
    Downloading: [+++- ] 6 %
    ```

    When the download completes, the firmware is verified. If everything is OK, the firmware update is applied, the device reboots and attemps to connect to the Device Management service again. The `<endpoint ID>` should be preserved.

## Automated testing

The Simple Pelion Client provides Greentea tests to confirm your platform works as expected. The network and storage configuration is already defined in Mbed OS 5.10, but you may want to override the configuration in `mbed_app.json`.

For details on Simple Pelion Client testing, refer to the documentation [here](https://github.com/ARMmbed/simple-mbed-cloud-client#testing).

This template application contains a working application and tests passing for the `K64F` and `K66F` platforms.

## Known issues

Please check the issues reported on github.
