// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------
#ifndef MBED_TEST_MODE

#include "mbed.h"
#include "simple-mbed-cloud-client.h"
#include "FATFileSystem.h"
//#include "LittleFileSystem.h"

#if defined(SENSOR_HDC1050)
#include "hdc1050.h"
#endif

// An event queue is a very useful structure to debounce information between contexts (e.g. ISR and normal threads)
// This is great because things such as network operations are illegal in ISR, so updating a resource in a button's fall() function is not allowed
EventQueue eventQueue;

// Default block device
BlockDevice *bd = BlockDevice::get_default_instance();
//LittleFileSystem fs(((const char*)PAL_FS_MOUNT_POINT_PRIMARY+1));
FATFileSystem fs(((const char*)PAL_FS_MOUNT_POINT_PRIMARY+1));
StorageHelper sh(bd, &fs);

// Default network interface object
NetworkInterface *net = NetworkInterface::get_default_instance();

// Declaring pointers for access to Pelion Device Management Client resources outside of main()
#if defined(SENSOR_HDC1050)
MbedCloudClientResource *temperature_res;
MbedCloudClientResource *humidity_res;
#endif

MbedCloudClientResource *button_res;

static uint16_t sim_count = 0;
static float fTemperature = 0.0f;
static uint16_t u16Humidity = 0;

// This function gets triggered by the timer. It's easy to replace it by an InterruptIn and fall() mode on a real button
void resource_scan() {

        int v = button_res->get_value_int() + 1;

        button_res->set_value(v);

        printf("Simulated button clicked %d times\n", v);	

#if defined(SENSOR_HDC1050)

	HDC1050_GetSensorData(&fTemperature, &u16Humidity);

	printf("### Observe_res data update per %ds to keep alive: count: %d, time: %ds, temperature:%.2f, humidity:%d\n", 
						20, v, 20*v, fTemperature, u16Humidity);
	temperature_res->set_value(fTemperature);
	humidity_res->set_value(u16Humidity);

#endif
}

/**
 * Notification callback handler
 * @param resource The resource that triggered the callback
 * @param status The delivery status of the notification
 */
void resource_callback(MbedCloudClientResource *resource, const NoticationDeliveryStatus status) {
    printf("Resource notification, status %s (%d)\n", MbedCloudClientResource::delivery_status_to_string(status), status);
}

/**
 * Registration callback handler
 * @param endpoint Information about the registered endpoint such as the name (so you can find it back in portal)
 */
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Connected to Pelion Device Management. Endpoint Name: %s\n", endpoint->internal_endpoint_name.c_str());
}

int main(void) {
    printf("[FOTA TEST]\n");
    printf("Starting Simple Pelion Device Management Client example\n");
    printf("Connecting to the network...\n");

#if defined(SENSOR_HDC1050)
	//
    // Initiation for board
    //
    HDC1050_Init();

    fTemperature = 0.0f;
    u16Humidity = 0;

#endif

    // Connect to the internet (DHCP is expected to be on)
    nsapi_error_t status = net->connect();

    if (status != NSAPI_ERROR_OK) {
        printf("Connecting to the network failed %d!\n", status);
        return -1;
    }

    printf("Connected to the network successfully. IP address: %s\n", net->get_ip_address());


    // SimpleMbedCloudClient handles registering over LwM2M to Pelion Device Management
    printf("PAL_FS_MOUNT_POINT_PRIMARY = %s\n", PAL_FS_MOUNT_POINT_PRIMARY);
    // Call StorageHelper to initialize FS for us
    sh.init();

    SimpleMbedCloudClient client(net, bd, &fs);
    int client_status = client.init();
    if (client_status != 0) {
        printf("Pelion Client initialization failed (%d)\n", client_status);
        return -1;
    }
    // Creating resources, which can be written or read from the cloud

#if defined(SENSOR_HDC1050)

    temperature_res = client.create_resource("3303/0/5700", "temperature");
    temperature_res->set_value(0.0f);
    temperature_res->methods(M2MMethod::GET);
    temperature_res->observable(true);
    temperature_res->attach_notification_callback(resource_callback);

    humidity_res = client.create_resource("3304/0/5700", "humidity");
    humidity_res->set_value(0);
    humidity_res->methods(M2MMethod::GET);
    humidity_res->observable(true);
    humidity_res->attach_notification_callback(resource_callback);

#endif

    button_res = client.create_resource("3200/0/5501", "button_count");
    button_res->set_value(0);
    button_res->methods(M2MMethod::GET);
    button_res->observable(true);
    button_res->attach_notification_callback(resource_callback);

    printf("Initialized Pelion Client. Registering...\n");

    // Callback that fires when registering is complete
    client.on_registered(&registered);

    // Register with Pelion Device Management
    client.register_and_connect();

    // Placeholder for callback to update local resource when GET comes.
    // The timer fires on an interrupt context, but debounces it to the eventqueue, so it's safe to do network operations
    Ticker timer;
    timer.attach(eventQueue.event(&resource_scan), 20.0);

    // You can easily run the eventQueue in a separate thread if required
    eventQueue.dispatch_forever();
}
#endif
