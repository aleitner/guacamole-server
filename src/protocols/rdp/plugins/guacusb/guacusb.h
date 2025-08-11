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

#ifndef GUAC_RDP_PLUGINS_GUACUSB_H
#define GUAC_RDP_PLUGINS_GUACUSB_H

#include <freerdp/constants.h>
#include <freerdp/dvc.h>
#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <winpr/wtypes.h>

/**
 * Maximum number of USB devices that can be connected simultaneously.
 */
#define GUAC_USB_MAX_DEVICES 16

/* Forward declaration */
typedef struct guac_rdp_usb_plugin guac_rdp_usb_plugin;

/**
 * Simple USB device tracking structure.
 */
typedef struct guac_rdp_usb_device {
    
    /**
     * WebUSB device ID (unique identifier from browser).
     */
    char web_usb_id[128];
    
    /**
     * Virtual channel for this specific device.
     */
    IWTSVirtualChannel* channel;
    
    /**
     * Listener for this device's channel.
     */
    IWTSListenerCallback* listener;
    
    /**
     * Whether this slot is in use.
     */
    BOOL in_use;
    
} guac_rdp_usb_device;

/**
 * Listener callback for control channel.
 */
typedef struct guac_rdp_usb_control_listener_callback {
    IWTSListenerCallback parent;
    guac_client* client;
    guac_rdp_usb_plugin* plugin;
} guac_rdp_usb_control_listener_callback;

/**
 * Channel callback for control channel.
 */
typedef struct guac_rdp_usb_control_channel_callback {
    IWTSVirtualChannelCallback parent;
    IWTSVirtualChannel* channel;
    guac_client* client;
    guac_rdp_usb_plugin* plugin;
} guac_rdp_usb_control_channel_callback;

/**
 * Listener callback for device channels.
 */
typedef struct guac_rdp_usb_device_listener_callback {
    IWTSListenerCallback parent;
    guac_client* client;
    guac_rdp_usb_plugin* plugin;
    int device_index;
} guac_rdp_usb_device_listener_callback;

/**
 * Channel callback for device channels.
 */
typedef struct guac_rdp_usb_device_channel_callback {
    IWTSVirtualChannelCallback parent;
    IWTSVirtualChannel* channel;
    guac_client* client;
    guac_rdp_usb_plugin* plugin;
    int device_index;
} guac_rdp_usb_device_channel_callback;

/**
 * Main USB plugin structure.
 */
struct guac_rdp_usb_plugin {
    
    /**
     * The parent plugin structure.
     */
    IWTSPlugin parent;
    
    /**
     * The client instance.
     */
    guac_client* client;
    
    /**
     * Virtual channel manager.
     */
    IWTSVirtualChannelManager* channel_manager;
    
    /**
     * Control channel reference.
     */
    IWTSVirtualChannel* control_channel;
    
    /**
     * Control channel listener.
     */
    guac_rdp_usb_control_listener_callback* control_listener;
    
    /**
     * Array of USB devices.
     */
    guac_rdp_usb_device devices[GUAC_USB_MAX_DEVICES];
    
};

#endif