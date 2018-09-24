# WISE-1570 Simple Mbed Cloud Client application

## Overview

This is a template application for platform vendors. It demonstrates how to create a simple Mbed Cloud Client application that can connect to Mbed Cloud, register resources and get ready to receive a firmware update.

This application is customized from the simple Mbed Cloud Client application [here](https://github.com/ARMmbed/simple-mbed-cloud-client-template-restricted). It has been configured to work **out-of-the-box** for Advantech's [WISE-1570 NBIoT module](https://os.mbed.com/modules/advantech-wise-1570/). The Simple Mbed Cloud Client application works in **developer mode** by default.

## Setup process

This is a summary of the process for developers to get started and get a device connected to Mbed Cloud.

### Mbed CLI tools

1. Import the application into your desktop:

    ```
    mbed import --protocol ssh https://github.com/chuanga/wise-1570-simple-mbed-cloud-client-restricted
    cd wise-1570-simple-mbed-cloud-restricted
    ```

2. Apply the cloud client MTU patch
    ```
    cd simple-mbed-cloud-client/mbed-cloud-client/
    git apply ../../cloud-client_MTU_fix.diff
    ```
3. Apply the easy-cellular patch for getting access to low-level devices (e.g. uart, power, etc.)
   Note: this patch may not be needed in mbed os 5.10.
    ```
    cd mbed-os
    git apply ../easy_cellular-patch-7795.diff
    ```
4. Download the developer certificate from Mbed Cloud.
5. Compile and program:

    ```
    mbed compile -t GCC_ARM -m MTB_ADV_WISE_1570 -f
    ```
6. Combine the program binary with the bootloader (a pre-built bootloader for WISE-1570 has been provided for you)
    ```
    tools/combine_bootloader_with_app.py -b mbed-bootloader-internal.hex -m MTB_ADV_WISE_1570 -a BUILD/MTB_ADV_WISE_1570/GCC_ARM/wise-1570-simple-mbed-cloud-client-restricted.hex -o combined.hex
    ```
## Porting to a new platform

Please refer to the [Simple Mbed Cloud Client template application](https://github.com/ARMmbed/simple-mbed-cloud-client-template-restricted) if you want to customize the program for different platform components or another platform. 

#### Update the application logic

The template example uses a ticker object to periodically fire a software interrupt to simulate button presses. Let’s say you want to make an actual button press.

By default, there is a Ticker object, which fires every five seconds and invokes a callback function.

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

1. Declare an `InterruptIn` object on the button, and attach the callback function to the `fall` handler:

    ```cpp
    InterruptIn btn(BUTTON1);
    btn.fall(eventQueue.event(&fake_button_press), 5.0);
    ```

1. Rename `fake_button_press` to `real_button_press`.


#### Updating the LwM2M objects

See guide at [TODO]

#### Mbed Cloud Client v1.3.x SOTP-specific changes

Mbed Cloud Client v1.3.x introduces a new feature called Software One-Time Programming (SOTP) that makes use of the internal flash of the MCU as an One-Time-Programmable section. It stores the keys required to decrypt the credentials stored in the persistent storage. Read more on this in the [porting documentation](https://cloud.mbed.com/docs/current/porting/changing-a-customized-porting-layer.html#rtos-module) under the RTOS module section.

The flash must be divided into two sections (default 2, maximum 2) for your target. You need to modify the `mbed_app.json` file to specify SOTP regions. A working version has been provided for the WISE-1570 module.

## Enabling firmware updates

To enable firmware updates, a compatible bootloader needs to be added in the `tools/` folder. The process to merge the application with the bootloader currently only works when building with Mbed CLI. In the future, this combine process will be done automatically by Mbed tools. 

A pre-built bootloader for WISE-1570 has been provided in this repository.

Next, instruct your users to do the following:

1. Install the [manifest tool](https://github.com/armmbed/manifest-tool).
1. Generate an update certificate:

    ```
    $ manifest-tool init -a YOUR_MBED_CLOUD_API_KEY -d yourdomain.com -m device-model-id -q --force
    ```

    <span class="notes">**Note:** Make sure to replace `YOUR_MBED_CLOUD_API_KEY` with an Mbed Cloud API key.

1. Build the application and combine it with the bootloader:

    ```
    $ mbed compile -m YOUR_TARGET -t GCC_ARM
    $ tools/combine_bootloader_with_app.py -m YOUR_TARGET -a BUILD/YOUR_TARGET/GCC_ARM/wise-1570-simple-mbed-cloud-client-restricted.hex -o combined.hex
    ```

1. Flash `combined.hex` to the development board.
1. Write down the endpoint ID of the board. You need it to start the update.

Now, a firmware update can be scheduled as explained in the [Mbed Cloud documentation](https://cloud.mbed.com/docs/current/updating-firmware/index.html). You can do it with the manifest tool itself or via the Mbed Cloud portal. Here we explain how to do it with the manifest tool.

1. Change the application, for example by changing some strings in `main.cpp`.
1. Compile the application:

    ```
    $ mbed compile -m YOUR_TARGET -t GCC_ARM
    ```

1. The manifest tool can both sign the update - using the private key generated earlier - and upload it to Mbed Cloud in a single command. Run:

    ```
    $ manifest-tool update device -p BUILD/YOUR_BOARD_NAME/GCC_ARM/simple-mbed-cloud-client-example_application.bin -D YOUR_ENDPOINT_NAME
    ```

    Replace `YOUR_BOARD_NAME` with the name of your development board, and replace `YOUR_ENDPOINT_NAME` with the endpoint name in Mbed Cloud.

1. Inspect the logs on the device to see the update progress. It looks similar to:

    ```
    Firmware download requested
    Authorization granted
    Downloading: [+++- ] 6 %
    ```

1. When the download completes, the firmware is verified. If everything is OK, the firmware update is applied.

## Known issues

Please check the issues reported on github.
