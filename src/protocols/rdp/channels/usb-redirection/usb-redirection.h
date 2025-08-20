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

#ifndef GUAC_RDP_CHANNELS_USB_REDIRECTION_H
#define GUAC_RDP_CHANNELS_USB_REDIRECTION_H

#include <freerdp/freerdp.h>
#include <guacamole/user.h>

/**
 * Handler for USB device connection events.
 */
guac_user_usbconnect_handler guac_rdp_user_usbconnect_handler;

/**
 * Handler for USB data transfer events.
 */
guac_user_usbdata_handler guac_rdp_user_usbdata_handler;

/**
 * Handler for USB device disconnection events.
 */
guac_user_usbdisconnect_handler guac_rdp_user_usbdisconnect_handler;

/**
 * Adds Guacamole's "guacusb" plugin to the list of dynamic virtual channel
 * plugins to be loaded by FreeRDP's "drdynvc" plugin. The plugin will only
 * be loaded once the "drdynvc" plugin is loaded. The "guacusb" plugin
 * ultimately adds support for the "URBDRC" dynamic virtual channel.
 *
 * @param context
 *     The RDP context associated with the active RDP session.
 */
void guac_rdp_usb_load_plugin(rdpContext* context);

#endif