/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "channels/usb-redirection/usb-redirection.h"
#include "plugins/channels.h"
#include "plugins/ptr-string.h"
#include "rdp.h"

#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int guac_rdp_user_usbconnect_handler(guac_user* user, const char* device_id,
        int vendor_id, int product_id, const char* device_name, 
        const char* serial_number, int device_class, int device_subclass,
        int device_protocol, const char* interface_data) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Verify USB support is enabled */
    if (!rdp_client->settings->usb_enabled) {
        guac_client_log(client, GUAC_LOG_WARNING, 
                "USB redirection is not enabled for this connection");
        return 0;
    }

    guac_client_log(client, GUAC_LOG_INFO, 
            "USB device connect: %s (VID:0x%04x PID:0x%04x)", 
            device_id, vendor_id, product_id);

    /* TODO: Send connect message to plugin's control channel */
    /* The plugin will then create a dedicated channel for this device */
    guac_client_log(client, GUAC_LOG_DEBUG,
            "Will request plugin to create channel for device: %s", device_id);

    return 0;
}

int guac_rdp_user_usbdata_handler(guac_user* user, const char* device_id,
        int endpoint_number, const char* data, int length, 
        const char* transfer_type) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Verify USB support is enabled */
    if (!rdp_client->settings->usb_enabled) {
        guac_client_log(client, GUAC_LOG_WARNING, 
                "USB redirection is not enabled for this connection");
        return 0;
    }

    guac_client_log(client, GUAC_LOG_DEBUG, 
            "USB data: device=%s endpoint=%d len=%d type=%s", 
            device_id, endpoint_number, length, transfer_type);

    /* TODO: Send data via device's dedicated channel (once created) */

    return 0;
}

int guac_rdp_user_usbdisconnect_handler(guac_user* user, 
        const char* device_id) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Verify USB support is enabled */
    if (!rdp_client->settings->usb_enabled) {
        guac_client_log(client, GUAC_LOG_WARNING, 
                "USB redirection is not enabled for this connection");
        return 0;
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "USB device disconnect: %s", device_id);

    /* TODO: Send disconnect message to control channel */
    /* The plugin will then close the device's dedicated channel */

    return 0;
}

void guac_rdp_usb_load_plugin(rdpContext* context) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    char client_ref[GUAC_RDP_PTR_STRING_LENGTH];

    /* Add "URBDRC" channel */
    guac_rdp_ptr_to_string(client, client_ref);
    guac_freerdp_dynamic_channel_collection_add(context->settings, "guacusb", client_ref, NULL);

    guac_client_log(client, GUAC_LOG_DEBUG, "USB redirection plugin scheduled for loading");
}