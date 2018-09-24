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

#include "mbed.h"
#include "simple-mbed-cloud-client.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
//#include "LittleFileSystem.h"
#include "EasyCellularConnection.h"
#include "mbed-trace/mbed_trace.h"


#define RETRY_COUNT 3


// Connect to the internet (DHCP is expected to be on)
EasyCellularConnection net;

// An event queue is a very useful structure to debounce information between contexts (e.g. ISR and normal threads)
// This is great because things such as network operations are illegal in ISR, so updating a resource in a button's fall() function is not allowed
EventQueue eventQueue;

// Storage implementation definition, currently using SDBlockDevice (SPI flash, DataFlash, and internal flash are also available)
SDBlockDevice sd(PA_7, PA_6, PA_5 , PA_4);
FATFileSystem fs("sd", &sd);
//LittleFileSystem fs("fs", &sd);

// Declaring pointers for access to Mbed Cloud Client resources outside of main()
MbedCloudClientResource *button_res;
MbedCloudClientResource *pattern_res;

// This function gets triggered by the timer. It's easy to replace it by an InterruptIn and fall() mode on a real button
void fake_button_press() {
    int v = button_res->get_value_int() + 1;

    button_res->set_value(v);

    printf("Simulated button clicked %d times\n", v);
}

/**
 * PUT handler
 * @param resource The resource that triggered the callback
 * @param newValue Updated value for the resource
 */
void pattern_updated(MbedCloudClientResource *resource, m2m::String newValue) {
    printf("PUT received, new value: %s\n", newValue.c_str());
}

/**
 * POST handler
 * @param resource The resource that triggered the callback
 * @param buffer If a body was passed to the POST function, this contains the data.
 *               Note that the buffer is deallocated after leaving this function, so copy it if you need it longer.
 * @param size Size of the body
 */
void blink_callback(MbedCloudClientResource *resource, const uint8_t *buffer, uint16_t size) {
    printf("POST received. Going to blink LED pattern: %s\n", pattern_res->get_value().c_str());

    static DigitalOut augmentedLed(LED1); // LED that is used for blinking the pattern

    // Parse the pattern string, and toggle the LED in that pattern
    string s = std::string(pattern_res->get_value().c_str());
    size_t i = 0;
    size_t pos = s.find(':');
    while (pos != string::npos) {
        wait_ms(atoi(s.substr(i, pos - i).c_str()));
        augmentedLed = !augmentedLed;

        i = ++pos;
        pos = s.find(':', pos);

        if (pos == string::npos) {
            wait_ms(atoi(s.substr(i, s.length()).c_str()));
            augmentedLed = !augmentedLed;
        }
    }
}

/**
 * Notification callback handler
 * @param resource The resource that triggered the callback
 * @param status The delivery status of the notification
 */
void button_callback(MbedCloudClientResource *resource, const NoticationDeliveryStatus status) {
    printf("Button notification, status %s (%d)\n", MbedCloudClientResource::delivery_status_to_string(status), status);
}

/**
 * Registration callback handler
 * @param endpoint Information about the registered endpoint such as the name (so you can find it back in portal)
 */
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Connected to Mbed Cloud. Endpoint Name: %s\n", endpoint->internal_endpoint_name.c_str());
}


nsapi_error_t do_connect()
{
    nsapi_error_t retcode;
    uint8_t retry_counter = 0;

    while (!net.is_connected()) {
        printf("Trying to connect ...\n");
	retcode = net.connect();
	if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
	    printf("\n\nAuthentication Failure. Exiting application\n");
	    return retcode;
	} else if (retcode != NSAPI_ERROR_OK && retry_counter > RETRY_COUNT) {
	    printf("\n\nFatal Connection failure: %d\n", retcode);
	    return retcode;
        } else if (retcode != NSAPI_ERROR_OK) {
	    printf("\n\nCouldn't connect: %d, will retry\n", retcode);
	    retry_counter++;
	    continue;
	}
    }
    printf("\n\nConnection Established.\n");
    return NSAPI_ERROR_OK;
}

void get_modem_info() {
    char rsp_buf[200]="";
    int rsp_buf_len = 200;
    CellularInformation *modem_info = net.get_device()->open_information(net.get_serial());

    if (modem_info) {
        modem_info->get_revision(rsp_buf, (rsp_buf_len -1));
        printf("***********  Modem Info ***********\n");
        printf("%s\n", rsp_buf);
        printf("*********************** ***********\n");
    } else {
        printf("modem_info is NULL !!!\n");
    }
}

int main(void) {
    nsapi_error_t status;

    printf("Starting Simple Mbed Cloud Client example\n");

    mbed_trace_init();

    printf("Connecting to the network using Cellular...\n");

    net.modem_debug_on(MBED_CONF_APP_MODEM_TRACE);
    net.set_sim_pin(MBED_CONF_APP_SIM_PIN_CODE);
    net.set_credentials(MBED_CONF_APP_APN, MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD);


    if ((status = do_connect()) != NSAPI_ERROR_OK) {
        printf("Network connection failed %d!\n", status);
        return -1;
    }

    printf("Connected to the network successfully. ...\n");
    get_modem_info();

    // SimpleMbedCloudClient handles registering over LwM2M to Mbed Cloud
    SimpleMbedCloudClient client(&net, &sd, &fs);
    int client_status = client.init();
    if (client_status != 0) {
        printf("Initializing Mbed Cloud Client failed (%d)\n", client_status);
        return -1;
    }

    // Creating resources, which can be written or read from the cloud
    button_res = client.create_resource("3200/0/5501", "button_count");
    button_res->set_value(0);
    button_res->methods(M2MMethod::GET);
    button_res->observable(true);
    button_res->attach_notification_callback(button_callback);

    pattern_res = client.create_resource("3201/0/5853", "blink_pattern");
    pattern_res->set_value("500:500:500:500:500:500:500:500");
    pattern_res->methods(M2MMethod::GET | M2MMethod::PUT);
    pattern_res->attach_put_callback(pattern_updated);

    MbedCloudClientResource *blink_res = client.create_resource("3201/0/5850", "blink_action");
    blink_res->methods(M2MMethod::POST);
    blink_res->attach_post_callback(blink_callback);

    printf("Initialized Mbed Cloud Client. Registering...\n");

    // Callback that fires when registering is complete
    client.on_registered(&registered);

    // Register with Mbed Cloud
    client.register_and_connect();

    // Placeholder for callback to update local resource when GET comes.
    // The timer fires on an interrupt context, but debounces it to the eventqueue, so it's safe to do network operations
   // Ticker timer;
   // timer.attach(eventQueue.event(&fake_button_press), 5.0);

    // You can easily run the eventQueue in a separate thread if required
    eventQueue.dispatch_forever();
}
