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

#include "plugins/guacusb/guacusb.h"
#include "plugins/ptr-string.h"

#include <freerdp/dvc.h>
#include <freerdp/settings.h>
#include <guacamole/client.h>
#include <guacamole/mem.h>
#include <winpr/stream.h>
#include <winpr/wtsapi.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Message types for control channel.
 */
#define USB_CONTROL_MSG_DEVICE_CONNECT    0x01
#define USB_CONTROL_MSG_DEVICE_DISCONNECT 0x02
#define USB_CONTROL_MSG_DEVICE_DATA       0x03

/**
 * Finds a free device slot.
 */
static int guac_rdp_usb_find_free_slot(guac_rdp_usb_plugin* plugin) {
    for (int i = 0; i < GUAC_USB_MAX_DEVICES; i++) {
        if (!plugin->devices[i].in_use)
            return i;
    }
    return -1;
}

/**
 * Finds a device slot by WebUSB ID.
 */
static int guac_rdp_usb_find_device_slot(guac_rdp_usb_plugin* plugin, const char* web_usb_id) {
    for (int i = 0; i < GUAC_USB_MAX_DEVICES; i++) {
        if (plugin->devices[i].in_use && 
            strcmp(plugin->devices[i].web_usb_id, web_usb_id) == 0)
            return i;
    }
    return -1;
}

/**
 * Device channel data handler.
 */
static UINT guac_rdp_usb_device_data(IWTSVirtualChannelCallback* channel_callback,
        wStream* stream) {

    guac_rdp_usb_device_channel_callback* device_callback =
            (guac_rdp_usb_device_channel_callback*) channel_callback;

    guac_client_log(device_callback->client, GUAC_LOG_DEBUG,
            "Data received on device channel %d (device: %s)",
            device_callback->device_index,
            device_callback->plugin->devices[device_callback->device_index].web_usb_id);

    /* TODO: Process device-specific URBDRC data */

    return CHANNEL_RC_OK;
}

/**
 * Device channel close handler.
 */
static UINT guac_rdp_usb_device_close(IWTSVirtualChannelCallback* channel_callback) {

    guac_rdp_usb_device_channel_callback* device_callback =
            (guac_rdp_usb_device_channel_callback*) channel_callback;

    guac_client_log(device_callback->client, GUAC_LOG_DEBUG,
            "Device channel %d closed", device_callback->device_index);

    /* Clear device slot in plugin */
    if (device_callback->plugin && device_callback->device_index < GUAC_USB_MAX_DEVICES) {
        device_callback->plugin->devices[device_callback->device_index].channel = NULL;
    }

    guac_mem_free(device_callback);
    return CHANNEL_RC_OK;
}

/**
 * Device channel new connection handler.
 */
static UINT guac_rdp_usb_device_new_connection(
        IWTSListenerCallback* listener_callback, IWTSVirtualChannel* channel,
        BYTE* data, int* accept, IWTSVirtualChannelCallback** channel_callback) {

    guac_rdp_usb_device_listener_callback* device_listener =
            (guac_rdp_usb_device_listener_callback*) listener_callback;

    guac_client_log(device_listener->client, GUAC_LOG_DEBUG,
            "New connection on device channel %d", device_listener->device_index);

    /* Allocate channel callback */
    guac_rdp_usb_device_channel_callback* device_callback =
            guac_mem_zalloc(sizeof(guac_rdp_usb_device_channel_callback));

    device_callback->client = device_listener->client;
    device_callback->channel = channel;
    device_callback->device_index = device_listener->device_index;
    device_callback->plugin = device_listener->plugin;
    device_callback->parent.OnDataReceived = guac_rdp_usb_device_data;
    device_callback->parent.OnClose = guac_rdp_usb_device_close;

    /* Store channel reference in plugin */
    if (device_listener->plugin && device_listener->device_index < GUAC_USB_MAX_DEVICES) {
        device_listener->plugin->devices[device_listener->device_index].channel = channel;
    }

    *channel_callback = (IWTSVirtualChannelCallback*) device_callback;
    return CHANNEL_RC_OK;
}

/**
 * Creates a dynamic channel for a specific USB device.
 */
static int guac_rdp_usb_create_device_channel(guac_rdp_usb_plugin* plugin, 
        const char* web_usb_id, int slot) {

    guac_client* client = plugin->client;
    
    /* Create channel name like URBDRC_01, URBDRC_02, etc. */
    char channel_name[32];
    snprintf(channel_name, sizeof(channel_name), "URBDRC_%02d", slot);

    /* Create listener for this device */
    guac_rdp_usb_device_listener_callback* device_listener =
            guac_mem_zalloc(sizeof(guac_rdp_usb_device_listener_callback));
    
    device_listener->client = client;
    device_listener->plugin = plugin;
    device_listener->device_index = slot;
    device_listener->parent.OnNewChannelConnection = guac_rdp_usb_device_new_connection;

    /* Register device channel */
    UINT result = plugin->channel_manager->CreateListener(
            plugin->channel_manager, channel_name, 0,
            (IWTSListenerCallback*) device_listener, NULL);

    if (result != CHANNEL_RC_OK) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to create device channel %s: %d", channel_name, result);
        guac_mem_free(device_listener);
        return -1;
    }

    /* Store device info */
    strncpy(plugin->devices[slot].web_usb_id, web_usb_id, 127);
    plugin->devices[slot].web_usb_id[127] = '\0';
    plugin->devices[slot].listener = (IWTSListenerCallback*) device_listener;
    plugin->devices[slot].in_use = TRUE;

    guac_client_log(client, GUAC_LOG_INFO,
            "Created channel %s for device %s", channel_name, web_usb_id);

    return slot;
}

/**
 * Removes a device channel.
 */
static void guac_rdp_usb_remove_device_channel(guac_rdp_usb_plugin* plugin, int slot) {
    
    guac_client* client = plugin->client;
    
    if (slot < 0 || slot >= GUAC_USB_MAX_DEVICES)
        return;
    
    if (!plugin->devices[slot].in_use)
        return;
    
    guac_client_log(client, GUAC_LOG_INFO,
            "Removing channel for device %s (slot %d)",
            plugin->devices[slot].web_usb_id, slot);
    
    /* Free listener if allocated */
    if (plugin->devices[slot].listener) {
        guac_mem_free(plugin->devices[slot].listener);
        plugin->devices[slot].listener = NULL;
    }
    
    /* Clear device slot */
    plugin->devices[slot].channel = NULL;
    plugin->devices[slot].in_use = FALSE;
    memset(plugin->devices[slot].web_usb_id, 0, 128);
}

/**
 * Control channel data handler.
 */
static UINT guac_rdp_usb_control_data(IWTSVirtualChannelCallback* channel_callback,
        wStream* stream) {

    guac_rdp_usb_control_channel_callback* control_callback =
            (guac_rdp_usb_control_channel_callback*) channel_callback;
    
    guac_rdp_usb_plugin* plugin = control_callback->plugin;
    guac_client* client = control_callback->client;

    /* Read message type */
    if (Stream_GetRemainingLength(stream) < 1)
        return CHANNEL_RC_OK;

    BYTE message_type;
    Stream_Read_UINT8(stream, message_type);

    switch (message_type) {
        
        case USB_CONTROL_MSG_DEVICE_CONNECT: {
            /* For testing, use a dummy device ID */
            char test_device_id[128] = "test_device_001";
            
            /* Check if device already has a channel */
            int existing_slot = guac_rdp_usb_find_device_slot(plugin, test_device_id);
            if (existing_slot >= 0) {
                guac_client_log(client, GUAC_LOG_WARNING,
                        "Device %s already has channel at slot %d",
                        test_device_id, existing_slot);
                break;
            }
            
            /* Find free slot */
            int slot = guac_rdp_usb_find_free_slot(plugin);
            if (slot < 0) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "No free slots for device %s", test_device_id);
                break;
            }
            
            /* Create channel for this device */
            if (guac_rdp_usb_create_device_channel(plugin, test_device_id, slot) < 0) {
                guac_client_log(client, GUAC_LOG_ERROR,
                        "Failed to create channel for device %s", test_device_id);
            }
            
            break;
        }
        
        case USB_CONTROL_MSG_DEVICE_DISCONNECT: {
            /* For testing, use a dummy device ID */
            char test_device_id[128] = "test_device_001";
            
            /* Find device slot */
            int slot = guac_rdp_usb_find_device_slot(plugin, test_device_id);
            if (slot < 0) {
                guac_client_log(client, GUAC_LOG_WARNING,
                        "Device %s not found for disconnect", test_device_id);
                break;
            }
            
            /* Remove channel */
            guac_rdp_usb_remove_device_channel(plugin, slot);
            
            break;
        }
        
        case USB_CONTROL_MSG_DEVICE_DATA: {
            /* Data should go through device-specific channels */
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Data message received on control channel - should use device channel");
            break;
        }
        
        default:
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Unknown control message type: 0x%02x", message_type);
            break;
    }

    return CHANNEL_RC_OK;
}

/**
 * Control channel close handler.
 */
static UINT guac_rdp_usb_control_close(IWTSVirtualChannelCallback* channel_callback) {

    guac_rdp_usb_control_channel_callback* control_callback =
            (guac_rdp_usb_control_channel_callback*) channel_callback;

    guac_client_log(control_callback->client, GUAC_LOG_DEBUG,
            "USB control channel closed");

    /* Clear control channel reference in plugin */
    if (control_callback->plugin) {
        control_callback->plugin->control_channel = NULL;
    }

    guac_mem_free(control_callback);
    return CHANNEL_RC_OK;
}

/**
 * Control channel new connection handler.
 */
static UINT guac_rdp_usb_control_new_connection(
        IWTSListenerCallback* listener_callback, IWTSVirtualChannel* channel,
        BYTE* data, int* accept, IWTSVirtualChannelCallback** channel_callback) {

    guac_rdp_usb_control_listener_callback* control_listener =
            (guac_rdp_usb_control_listener_callback*) listener_callback;

    guac_client_log(control_listener->client, GUAC_LOG_DEBUG,
            "New USB control channel connection");

    /* Allocate channel callback */
    guac_rdp_usb_control_channel_callback* control_callback =
            guac_mem_zalloc(sizeof(guac_rdp_usb_control_channel_callback));

    control_callback->client = control_listener->client;
    control_callback->channel = channel;
    control_callback->plugin = control_listener->plugin;
    control_callback->parent.OnDataReceived = guac_rdp_usb_control_data;
    control_callback->parent.OnClose = guac_rdp_usb_control_close;

    /* Store control channel reference in plugin */
    if (control_listener->plugin) {
        control_listener->plugin->control_channel = channel;
    }

    *channel_callback = (IWTSVirtualChannelCallback*) control_callback;
    return CHANNEL_RC_OK;
}

/**
 * Plugin initialization.
 */
static UINT guac_rdp_usb_initialize(IWTSPlugin* plugin,
        IWTSVirtualChannelManager* manager) {

    guac_rdp_usb_plugin* usb_plugin = (guac_rdp_usb_plugin*) plugin;
    guac_client* client = usb_plugin->client;
    
    guac_client_log(client, GUAC_LOG_DEBUG, "Initializing USB plugin");

    usb_plugin->channel_manager = manager;

    /* Create control channel listener */
    usb_plugin->control_listener = guac_mem_zalloc(sizeof(guac_rdp_usb_control_listener_callback));
    usb_plugin->control_listener->client = client;
    usb_plugin->control_listener->plugin = usb_plugin;
    usb_plugin->control_listener->parent.OnNewChannelConnection = guac_rdp_usb_control_new_connection;

    /* Register control channel only */
    UINT result = manager->CreateListener(manager, "URBDRC", 0,
            (IWTSListenerCallback*) usb_plugin->control_listener, NULL);

    if (result != CHANNEL_RC_OK) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to create USB control channel: %d", result);
        return result;
    }

    guac_client_log(client, GUAC_LOG_INFO,
            "USB plugin initialized with control channel");

    return CHANNEL_RC_OK;
}

/**
 * Plugin termination.
 */
static UINT guac_rdp_usb_terminated(IWTSPlugin* plugin) {

    guac_rdp_usb_plugin* usb_plugin = (guac_rdp_usb_plugin*) plugin;
    guac_client* client = usb_plugin->client;

    guac_client_log(client, GUAC_LOG_DEBUG, "USB plugin terminating");

    /* Free control listener */
    if (usb_plugin->control_listener)
        guac_mem_free(usb_plugin->control_listener);

    /* Free any device listeners and close channels */
    for (int i = 0; i < GUAC_USB_MAX_DEVICES; i++) {
        if (usb_plugin->devices[i].in_use) {
            guac_rdp_usb_remove_device_channel(usb_plugin, i);
        }
    }

    guac_mem_free(usb_plugin);

    guac_client_log(client, GUAC_LOG_DEBUG, "USB plugin terminated");
    return CHANNEL_RC_OK;
}

/**
 * Entry point for USB dynamic virtual channel.
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints) {

    /* Pull client from arguments */
#ifdef PLUGIN_DATA_CONST
    const ADDIN_ARGV* args = pEntryPoints->GetPluginData(pEntryPoints);
#else
    ADDIN_ARGV* args = pEntryPoints->GetPluginData(pEntryPoints);
#endif

    guac_client* client = (guac_client*) guac_rdp_string_to_ptr(args->argv[1]);

    /* Pull previously-allocated plugin */
    guac_rdp_usb_plugin* usb_plugin = (guac_rdp_usb_plugin*)
            pEntryPoints->GetPlugin(pEntryPoints, "guacusb");

    /* If no such plugin allocated, allocate and register it now */
    if (usb_plugin == NULL) {

        /* Allocate plugin */
        usb_plugin = guac_mem_zalloc(sizeof(guac_rdp_usb_plugin));
        usb_plugin->parent.Initialize = guac_rdp_usb_initialize;
        usb_plugin->parent.Terminated = guac_rdp_usb_terminated;
        usb_plugin->client = client;

        /* Clear device array */
        memset(usb_plugin->devices, 0, sizeof(usb_plugin->devices));

        /* Register plugin */
        UINT result = pEntryPoints->RegisterPlugin(pEntryPoints, "guacusb",
                (IWTSPlugin*) usb_plugin);

        if (result != CHANNEL_RC_OK) {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Failed to register USB plugin: %d", result);
            guac_mem_free(usb_plugin);
            return result;
        }

        guac_client_log(client, GUAC_LOG_DEBUG, "USB plugin loaded");
    }

    return CHANNEL_RC_OK;
}