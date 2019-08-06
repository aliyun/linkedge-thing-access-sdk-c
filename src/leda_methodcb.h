/*
 * Copyright (c) 2014-2019 Alibaba Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _LEDA_METHODCB_H_
#define _LEDA_METHODCB_H_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#define DBUS_PROPERTIES_CHANGE_INTERFACE    "org.freedesktop.DBus.Properties"

#define RUN_STATE_NORMAL   0
#define RUN_STATE_EXIT     1

#define STATE_ONLINE       0
#define STATE_OFFLINE      1
#define STATE_ALL          2

typedef struct leda_device_info {
    struct list_head            list_node;
    device_handle_t             dev_handle;
    char                        *cloud_id;
    char                        *product_key;
    char                        *dev_name;
    int                         online;
    char                        *usr_data;
    get_properties_callback     get_properties_cb;
    set_properties_callback     set_properties_cb;
    call_service_callback       call_service_cb;
    int                         service_output_max_count;  /* 设备服务回调结果数组最大长度 */
    int                         is_local_name;
    int                         is_local;
} leda_device_info_t;

typedef struct leda_methodcall_info {
    char            *cloud_id;
    char            *method_name;
    char            *service_name;
    char            *params;
    DBusConnection  *connection;
    DBusMessage     *reply;
}leda_methodcall_info_t;

typedef struct leda_connect_info {
    int             trpool_num;
    DBusConnection  *connection;
} leda_connect_info_t;

typedef struct leda_reply {
    struct list_head    list_node;
    uint32_t            serial_id;
    DBusMessage         *reply;
    struct timespec     tout;
} leda_reply_t;

typedef struct leda_device_configcb {
    struct list_head        list_node;
    char                    *module_id;
    config_changed_callback device_configcb;
} leda_device_configcb_t;

void leda_set_runstate(int state);

leda_device_info_t *leda_get_methodcb_by_cloud_id(const char *cloud_id);
leda_device_info_t *leda_get_methodcb_by_device_handle(device_handle_t dev_handle);
leda_device_info_t *leda_get_methodcb_by_dn_pk(const char *product_key, const char *dev_name);

void leda_set_methodcb_online(device_handle_t dev_handle, int state);
leda_device_info_t * leda_insert_methodcb(const char *cloud_id, 
                                          device_handle_t dev_handle,
                                          const char *product_key,
                                          int is_local_name,
                                          const char *dev_name,
                                          const leda_device_callback_t *device_cb, 
                                          int is_local,
                                          void *usr_data);
void leda_remove_methodcb(device_handle_t dev_handle);

leda_reply_t *leda_insert_send_reply(uint32_t serial_id);
void leda_remove_reply(leda_reply_t *bus_reply);
int leda_get_reply_params(leda_reply_t *bus_reply, int timeout_ms);
void *leda_methodcb_thread(void *arg);

int leda_insert_device_configcb(const char *module_id, config_changed_callback onchanged_device_configcb);
leda_device_configcb_t *leda_get_device_configcb_by_name(const char *module_id);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
#endif
