/*
 * Copyright (c) 2014-2018 Alibaba Group. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <dbus/dbus.h>
#include <cJSON.h>
#include <sys/prctl.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"
#include "linux-list.h"
#include "leda_base.h"
#include "leda_trpool.h"
#include "leda_methodcb.h"

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

LIST_HEAD(leda_cb_head);
pthread_mutex_t g_methodcb_list_lock;

LIST_HEAD(leda_send_head);
LIST_HEAD(leda_receive_head);
pthread_mutex_t g_leda_replay_lock;

LIST_HEAD(leda_device_configcb_head);
pthread_mutex_t g_device_configcb_lock;

#define LEDA_INTROSPECT_HEADER_STRING \
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"\
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
#define LEDA_INTROSPECT_END_STRING \
    "    <method name=\"callServices\">\n"\
    "      <arg direction=\"in\" type=\"s\" />\n"\
    "      <arg direction=\"in\" type=\"s\" />\n"\
    "      <arg direction=\"out\" type=\"s\" />\n"\
    "    </method>\n"\
    "  </interface>\n"\
    "</node>\n"
    
#define LEDA_DRIVER_INTROSPECT_END_STRING \
    "    <method name=\"getDeviceList\">\n"\
    "      <arg direction=\"out\" type=\"s\" />\n"\
    "    </method>\n"\
    "  </interface>\n"\
    "</node>\n"
    
static int g_run_state = RUN_STATE_NORMAL;

void leda_set_runstate(int state)
{
    if((RUN_STATE_NORMAL != state) && (RUN_STATE_EXIT != state))
    {
        return;
    }

    g_run_state = state;
}

leda_device_info_t *leda_get_methodcb_by_cloud_id(const char *cloud_id)
{
    leda_device_info_t *pos, *next, *device_info = NULL;

    pthread_mutex_lock(&g_methodcb_list_lock);
    list_for_each_entry_safe(pos, next, &leda_cb_head, list_node)
    {
        if(strcmp(pos->cloud_id, cloud_id) == 0)
        {
            device_info = pos;
            break;
        }
    }
    pthread_mutex_unlock(&g_methodcb_list_lock);
    return device_info;
}

leda_device_info_t *leda_get_methodcb_by_device_handle(device_handle_t dev_handle)
{
    leda_device_info_t *pos, *next, *device_info = NULL;

    pthread_mutex_lock(&g_methodcb_list_lock);
    list_for_each_entry_safe(pos, next, &leda_cb_head, list_node)
    {
        if(dev_handle == pos->dev_handle)
        {
            device_info = pos;
            break;
        }
    }
    pthread_mutex_unlock(&g_methodcb_list_lock);
    return device_info;
}

leda_device_info_t *leda_get_methodcb_by_dn_pk(const char *product_key, const char *dev_name)
{
    leda_device_info_t *pos, *next, *device_info = NULL;

    pthread_mutex_lock(&g_methodcb_list_lock);
    list_for_each_entry_safe(pos, next, &leda_cb_head, list_node)
    {
        if((strcmp(pos->product_key, product_key) == 0) 
            && (strcmp(pos->dev_name, dev_name) == 0))
        {
            device_info = pos;
            break;
        }
    }
    pthread_mutex_unlock(&g_methodcb_list_lock);
    return device_info;
}

void leda_set_methodcb_online(device_handle_t dev_handle, int state)
{
    leda_device_info_t *pos = NULL, *next = NULL;

    if((STATE_ONLINE != state) && (STATE_OFFLINE != state))
    {
        log_e(LEDA_TAG_NAME, "state is invalid\r\n");
        return;
    }

    pthread_mutex_lock(&g_methodcb_list_lock);
    list_for_each_entry_safe(pos, next, &leda_cb_head, list_node)
    {
        if (dev_handle == pos->dev_handle)
        {
            pos->online = state;
            break;
        }
    }
    pthread_mutex_unlock(&g_methodcb_list_lock);
}

leda_device_info_t * leda_insert_methodcb(const char *cloud_id, 
                                          device_handle_t dev_handle,
                                          const char *product_key,
                                          int is_local_name,
                                          const char *dev_name,
                                          const leda_device_callback_t *device_cb, 
                                          int is_local,
                                          void *usr_data)
{
    leda_device_info_t *device_info;

    device_info = (leda_device_info_t *)malloc(sizeof(leda_device_info_t));
    if(NULL == device_info)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        return NULL;
    }
    memset(device_info, 0, sizeof(leda_device_info_t));
    device_info->cloud_id = (char *)malloc(strlen(cloud_id)+1);
    if(NULL == device_info->cloud_id)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info);
        return NULL;
    }
    strcpy(device_info->cloud_id, cloud_id);
    
    device_info->product_key = (char *)malloc(strlen(product_key)+1);
    if(NULL == device_info->product_key)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info->cloud_id);
        free(device_info);
        return NULL;
    }
    strcpy(device_info->product_key, product_key);

    device_info->dev_name = (char *)malloc(strlen(dev_name)+1);
    if(NULL == device_info->dev_name)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info->cloud_id);
        free(device_info->product_key);
        free(device_info);
        return NULL;
    }
    strcpy(device_info->dev_name, dev_name);

    device_info->dev_handle = dev_handle;
    device_info->service_output_max_count = device_cb->service_output_max_count;
    device_info->call_service_cb = device_cb->call_service_cb;
    device_info->get_properties_cb = device_cb->get_properties_cb;
    device_info->set_properties_cb = device_cb->set_properties_cb;
    device_info->usr_data = usr_data;
    device_info->is_local_name = is_local_name;
    device_info->is_local = is_local;
    device_info->online  = STATE_OFFLINE;

    pthread_mutex_lock(&g_methodcb_list_lock);
    list_add(&device_info->list_node, &leda_cb_head);
    pthread_mutex_unlock(&g_methodcb_list_lock);

    return device_info;
}

void leda_remove_methodcb(device_handle_t dev_handle)
{
    leda_device_info_t *device_info;

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if(NULL == device_info)
    {
        return;
    }
    pthread_mutex_lock(&g_methodcb_list_lock);
    list_del(&device_info->list_node);
    if(device_info->cloud_id)
    {
        free(device_info->cloud_id);
    }
    if(device_info->dev_name)
    {
        free(device_info->dev_name);
    }
    if(device_info->product_key)
    {
        free(device_info->product_key);
    }
    if(device_info)
    {
        free(device_info);
    }
    pthread_mutex_unlock(&g_methodcb_list_lock);

    return;
}

leda_device_info_t *leda_copy_device_info(const leda_device_info_t *device_info_src)
{
    leda_device_info_t *device_info;

    if (NULL == device_info_src)
    {
        return NULL;
    }

    device_info = (leda_device_info_t *)malloc(sizeof(leda_device_info_t));
    if (NULL == device_info)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        return NULL;
    }
    memset(device_info, 0, sizeof(leda_device_info_t));
    device_info->cloud_id = (char *)malloc(strlen(device_info_src->cloud_id) + 1);
    if (NULL == device_info->cloud_id)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info);
        return NULL;
    }
    strcpy(device_info->cloud_id, device_info_src->cloud_id);
    
    device_info->product_key = (char *)malloc(strlen(device_info_src->product_key) + 1);
    if (NULL == device_info->product_key)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info->cloud_id);
        free(device_info);
        return NULL;
    }
    strcpy(device_info->product_key, device_info_src->product_key);

    device_info->dev_name = (char *)malloc(strlen(device_info_src->dev_name) + 1);
    if (NULL == device_info->dev_name)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_info->cloud_id);
        free(device_info->product_key);
        free(device_info);
        return NULL;
    }
    strcpy(device_info->dev_name, device_info_src->dev_name);

    device_info->dev_handle                 = device_info_src->dev_handle;
    device_info->call_service_cb            = device_info_src->call_service_cb;
    device_info->get_properties_cb          = device_info_src->get_properties_cb;
    device_info->set_properties_cb          = device_info_src->set_properties_cb;
    device_info->usr_data                   = device_info_src->usr_data;
    device_info->online                     = device_info_src->online;
    device_info->service_output_max_count   = device_info_src->service_output_max_count;
    device_info->is_local_name              = device_info_src->is_local_name;
    device_info->is_local                   = device_info_src->is_local;

    return device_info;
}

void leda_delete_device_info(leda_device_info_t *device_info)
{
    if (NULL == device_info)
    {
        return;
    }

    if (device_info->cloud_id)
    {
        free(device_info->cloud_id);
    }

    if (device_info->dev_name)
    {
        free(device_info->dev_name);
    }

    if (device_info->product_key)
    {
        free(device_info->product_key);
    }

    if (device_info)
    {
        free(device_info);
        device_info = NULL;
    }
}

static leda_reply_t *_leda_get_reply_from_reveive(uint32_t serial_id)
{
    struct timespec tout;
    leda_reply_t *pos, *next, *bus_reply = NULL;

    clock_gettime(CLOCK_REALTIME, &tout);
    list_for_each_entry_safe(pos, next, &leda_receive_head, list_node)
    {
        if(pos->serial_id == serial_id)
        {
            bus_reply = pos;
            break;
        }

        if((tout.tv_sec-pos->tout.tv_sec) > 10)
        {
            log_d(LEDA_TAG_NAME, "reply:%d timeout:%d\n", pos->serial_id, pos->tout.tv_sec);
            list_del(&pos->list_node);
            if(pos->reply)
            {
                dbus_message_unref(pos->reply);
            }
            free(pos);
        }
    }

    return bus_reply;
}

static leda_reply_t *_leda_get_reply_from_send(uint32_t serial_id)
{
    leda_reply_t *pos, *next, *bus_reply = NULL;

    list_for_each_entry_safe(pos, next, &leda_send_head, list_node)
    {
        if(pos->serial_id == serial_id)
        {
            bus_reply = pos;
            break;
        }
    }

    return bus_reply;
}

static int _leda_set_send_reply(uint32_t serial_id, DBusMessage *reply)
{
    leda_reply_t *bus_reply;

    pthread_mutex_lock(&g_leda_replay_lock);
    bus_reply = _leda_get_reply_from_send(serial_id);
    if(NULL == bus_reply)
    {
        pthread_mutex_unlock(&g_leda_replay_lock);
        return LE_ERROR_UNKNOWN;
    }
    
    log_d(LEDA_TAG_NAME, "method reply:%d\r\n", serial_id);
    bus_reply->reply = reply;
    pthread_mutex_unlock(&g_leda_replay_lock);

    return LE_SUCCESS;
}

int leda_insert_receive_reply(uint32_t serial_id, DBusMessage *reply)
{
    leda_reply_t *bus_reply;

    if(NULL == reply)
    {
        return LE_ERROR_INVAILD_PARAM;
    }

    bus_reply = (leda_reply_t *)malloc(sizeof(leda_reply_t));
    if(NULL == bus_reply)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    bus_reply->serial_id = serial_id;
    bus_reply->reply = reply;
    clock_gettime(CLOCK_REALTIME, &bus_reply->tout);

    pthread_mutex_lock(&g_leda_replay_lock);
    list_add(&bus_reply->list_node, &leda_receive_head);
    pthread_mutex_unlock(&g_leda_replay_lock);

    log_i(LEDA_TAG_NAME, "insert receive reply serial_id:%d\r\n", serial_id);

    return LE_SUCCESS;
}

leda_reply_t *leda_insert_send_reply(uint32_t serial_id)
{
    leda_reply_t *bus_reply = NULL;

    pthread_mutex_lock(&g_leda_replay_lock);
    bus_reply = _leda_get_reply_from_reveive(serial_id);
    if(NULL != bus_reply)
    {
        list_del(&bus_reply->list_node);
        list_add(&bus_reply->list_node, &leda_send_head);
    }
    else
    {
        bus_reply = (leda_reply_t *)malloc(sizeof(leda_reply_t));
        if(NULL == bus_reply)
        {
            log_e(LEDA_TAG_NAME, "no memory\n");
            pthread_mutex_unlock(&g_leda_replay_lock);
            return NULL;
        }
        bus_reply->serial_id = serial_id;
        bus_reply->reply     = NULL;
        clock_gettime(CLOCK_REALTIME, &bus_reply->tout);

        list_add(&bus_reply->list_node, &leda_send_head);
    }
    pthread_mutex_unlock(&g_leda_replay_lock);

    log_i(LEDA_TAG_NAME, "insert reply serial_id:%d\n", serial_id);

    return bus_reply;
}

void leda_remove_reply(leda_reply_t *bus_reply)
{
    if(NULL == bus_reply)
    {
        return;
    }

    pthread_mutex_lock(&g_leda_replay_lock);
    list_del(&bus_reply->list_node);
    pthread_mutex_unlock(&g_leda_replay_lock);
    if(bus_reply->reply)
    {
        dbus_message_unref(bus_reply->reply);
    }
    free(bus_reply);

    return;
}

int leda_get_reply_params(leda_reply_t *bus_reply, int timeout_ms)
{
    int time_count = 0;

    if(NULL == bus_reply)
    {
        return LE_ERROR_UNKNOWN;
    }

    while(timeout_ms > time_count)
    {
        if(NULL != bus_reply->reply)
        {
            return LE_SUCCESS;
        }
        time_count++;
        usleep(1000);
    }

    return LE_ERROR_TIMEOUT;
}

static int _leda_method_is_driver_message_proc(DBusMessage *message)
{
    const char *method_name = NULL;
    method_name = dbus_message_get_member(message);

    if (!strcmp(method_name, DMP_METHOD_INTROSPECT)
        || !strcmp(method_name, LEDA_DRV_METHOD_GET_DEV_LIST))
    {
        return 0;
    }

    return 1;
}

static void _leda_deviceconnect_message_proc(DBusConnection *connection, DBusMessage *message)
{
    DBusMessage *reply = NULL;
    DBusError   dbus_error;
    char        *info = NULL;
    char        *result = NULL;

    dbus_error_init(&dbus_error);

    dbus_message_get_args(message, &dbus_error, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
    if (!(dbus_error_is_set(&dbus_error)) && (NULL != info))
    {
        log_d(LEDA_TAG_NAME, "device connect result: %s \r\n", info);
    }
    dbus_error_free(&dbus_error);

    result = leda_retmsg_create(LE_SUCCESS, NULL);
    
    reply = dbus_message_new_method_return(message);
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);

    if (NULL != result)
    {
        leda_retmsg_free(result);
    }
    dbus_message_unref(reply);

    return;
}

static void _leda_deviceconfig_message_proc(DBusConnection *connection, DBusMessage *message)
{
    int                     ret = LE_ERROR_UNKNOWN;
    DBusMessage             *reply = NULL;
    DBusError               dbus_error;
    const char              *method_name = NULL;
    char                    *key = NULL;
    char                    *value = NULL;
    char                    *result = NULL;
    leda_device_configcb_t  *pos;
    leda_device_configcb_t  *next;

    reply = dbus_message_new_method_return(message);

    method_name = dbus_message_get_member(message);
    if (!strcmp(method_name, DMP_CONFIGMANAGER_METHOD_NOTIFY))
    {
        dbus_error_init(&dbus_error);
        if (!dbus_message_get_args(message, &dbus_error, DBUS_TYPE_STRING, &key, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID)
            || (NULL == key))
        {
            log_e(LEDA_TAG_NAME, "configmanager notify device config failed");
            return;
        }
        dbus_error_free(&dbus_error);

        pthread_mutex_lock(&g_device_configcb_lock);
        list_for_each_entry_safe(pos, next, &leda_device_configcb_head, list_node)
        {
            if (strstr(key, pos->module_name))
            {
                ret = pos->device_configcb(key, value);
                break;
            }
        }
        pthread_mutex_unlock(&g_device_configcb_lock);
        
        result = leda_retmsg_create(ret, NULL);
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID);
        dbus_connection_send(connection, reply, NULL);
        leda_retmsg_free(result);
    }

    dbus_message_unref(reply);

    return;
}

static void _leda_device_message_proc(DBusConnection *connection, DBusMessage *message)
{
    const char *method_name = NULL;

    method_name = dbus_message_get_member(message);
    log_d(LEDA_TAG_NAME, "method_name: %s \r\n", method_name);

    if (!strcmp(method_name, DMP_CONFIGMANAGER_METHOD_NOTIFY))
    {
        _leda_deviceconfig_message_proc(connection, message);
    }
    else if(!strcmp(method_name, DMP_METHOD_RESULT_NOTIFY))
    {
        _leda_deviceconnect_message_proc(connection, message);
    }

    return;
}

static void _leda_driver_message_proc(DBusConnection *connection, DBusMessage *message)
{
    int type;
    char *driver_name;
    DBusMessage *reply = NULL;
    DBusError dbus_error;
    const char *method_name = NULL;
    cJSON *object, *sub_object, *item, *sub_item;
    leda_device_info_t *pos, *next;
    int dev_cnt, online_state;
    char *buff, *params = NULL;
    
    if((NULL != dbus_message_get_path(message)) 
        && (!strncmp(dbus_message_get_path(message), LEDA_DRIVER_PATH, strlen(LEDA_DRIVER_PATH))))
    {
        driver_name = (char *)malloc(strlen(dbus_message_get_path(message))-strlen(LEDA_DRIVER_PATH)+1);
        if(NULL == driver_name)
        {
            return;
        }
        snprintf(driver_name, (strlen(dbus_message_get_path(message))-strlen(LEDA_DRIVER_PATH)+1), 
            "%s", dbus_message_get_path(message)+strlen(LEDA_DRIVER_PATH));
    }
    else if((NULL != dbus_message_get_interface(message)) 
        && (!strncmp(dbus_message_get_interface(message), LEDA_DRIVER_WKN, strlen(LEDA_DRIVER_WKN))))
    {
        driver_name = (char *)malloc(strlen(dbus_message_get_interface(message))-strlen(LEDA_DRIVER_WKN)+1);
        if(NULL == driver_name)
        {
            return;
        }
        snprintf(driver_name, (strlen(dbus_message_get_interface(message))-strlen(LEDA_DRIVER_WKN)+1), 
            "%s", dbus_message_get_interface(message)+strlen(LEDA_DRIVER_WKN));
    }
    else if((NULL != dbus_message_get_destination(message)) 
        && (!strncmp(dbus_message_get_destination(message), LEDA_DRIVER_WKN, strlen(LEDA_DRIVER_WKN))))
    {
        driver_name = (char *)malloc(strlen(dbus_message_get_destination(message))-strlen(LEDA_DRIVER_WKN)+1);
        if(NULL == driver_name)
        {
            return;
        }
        snprintf(driver_name, (strlen(dbus_message_get_destination(message))-strlen(LEDA_DRIVER_WKN)+1), 
            "%s", dbus_message_get_destination(message)+strlen(LEDA_DRIVER_WKN));
    }
    else
    {
        return;
    }
    log_d(LEDA_TAG_NAME, "driver_name :%s\r\n", driver_name);
    type = dbus_message_get_type(message);
    if(DBUS_MESSAGE_TYPE_METHOD_CALL == type)
    {
        reply = dbus_message_new_method_return(message);
        method_name = dbus_message_get_member(message);
        if(!strcmp(method_name, DMP_METHOD_INTROSPECT))
        {
            buff = (char *)malloc(1024);
            if(NULL == buff)
            {
                dbus_connection_send(connection, reply, NULL);
            }
            else
            {
                memset(buff, 0, 1024);
                snprintf(buff, 1024, "%s<node>\n  <interface name=\"%s%s\">\n%s", 
                    LEDA_INTROSPECT_HEADER_STRING, LEDA_DRIVER_WKN, driver_name, LEDA_DRIVER_INTROSPECT_END_STRING);

                log_d(LEDA_TAG_NAME, "introspect:%s\r\n", buff);
                dbus_message_append_args(reply, DBUS_TYPE_STRING, &buff, DBUS_TYPE_INVALID);
                dbus_connection_send(connection, reply, NULL);
                free(buff);
            }
        }
        else if(!strcmp(method_name, LEDA_DRV_METHOD_GET_DEV_LIST))
        {
            dbus_error_init(&dbus_error);
            dbus_message_get_args(message, &dbus_error, DBUS_TYPE_STRING, &params, DBUS_TYPE_INVALID);
            dbus_error_free(&dbus_error);
            if(NULL == params)
            {
                online_state = STATE_ALL;
            }
            else
            {
                if(!strcmp(params, "deviceState=online"))
                {
                    online_state = STATE_ONLINE;
                }
                else if(!strcmp(params, "deviceState=offline"))
                {
                    online_state = STATE_OFFLINE;
                }
                else
                {
                    online_state = STATE_ALL;
                }
            }
            item = cJSON_CreateArray();
            dev_cnt = 0;
            pthread_mutex_lock(&g_methodcb_list_lock);
            list_for_each_entry_safe(pos, next, &leda_cb_head, list_node)
            {
                if((STATE_ALL == online_state) || (online_state == pos->online))
                {
                    sub_item = cJSON_CreateString((const char *)pos->cloud_id);
                    cJSON_AddItemToArray(item, sub_item);
                    dev_cnt++;
                }
            }
            pthread_mutex_unlock(&g_methodcb_list_lock);
            sub_object = cJSON_CreateObject();
            cJSON_AddItemToObject(sub_object, "devList", item);
            cJSON_AddNumberToObject(sub_object, "devNum", dev_cnt);
            
            object = cJSON_CreateObject();
            cJSON_AddItemToObject(object, "params", sub_object);
            buff = cJSON_Print(object);
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &buff, DBUS_TYPE_INVALID);
            dbus_connection_send(connection, reply, NULL);
            cJSON_free(buff);
            cJSON_Delete(object);
        }
        dbus_message_unref(reply);
    }
    free(driver_name);
}

static void _leda_introspect_proc(DBusConnection *connection, char *cloud_id, DBusMessage *reply)
{
    char *introspect;

    introspect = (char *)malloc(1024);
    if(NULL == introspect)
    {
        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);
        return;
    }
    memset(introspect, 0, 1024);
    snprintf(introspect, 1024, "%s<node>\n  <interface name=\"%s%s\">\n%s", 
        LEDA_INTROSPECT_HEADER_STRING, LEDA_DEVICE_WKN, cloud_id, LEDA_INTROSPECT_END_STRING);

    log_d(LEDA_TAG_NAME, "introspect:%s\r\n", introspect);
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspect, DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    free(introspect);
}

static void _leda_method_reply_proc(DBusMessage *reply)
{
    if (LE_SUCCESS == _leda_set_send_reply(dbus_message_get_reply_serial(reply), reply))
    {
        log_i(LEDA_TAG_NAME, "serial:%d set success.\n", dbus_message_get_reply_serial(reply));
        return;
    }

    if (LE_SUCCESS == leda_insert_receive_reply(dbus_message_get_reply_serial(reply), reply))
    {
        log_i(LEDA_TAG_NAME, "serial:%d insert success.\n", dbus_message_get_reply_serial(reply));
        return;
    }

    log_e(LEDA_TAG_NAME, "serial:%d reply proc failed.\n", dbus_message_get_reply_serial(reply));
    dbus_message_unref(reply);
}

static void *_leda_methodcb_proc(void *arg)
{
    leda_methodcall_info_t *methodcall_info = (leda_methodcall_info_t *)arg;
    leda_device_info_t *device_info = NULL;
    cJSON *object = NULL, *item = NULL;
    int params_count = 0;
    leda_device_data_t *dev_data_input = NULL, *dev_data_output = NULL;
    char *params = NULL;
    char *info = NULL;
    int i;
    int ret;

    if(NULL == methodcall_info)
    {
        return NULL;
    }

    device_info = leda_copy_device_info(leda_get_methodcb_by_cloud_id(methodcall_info->cloud_id));
    if(NULL == device_info)
    {
        info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
        goto err;
    }

    log_d(LEDA_TAG_NAME, "proc cloud_id:%s, dev_handle:%d, serial:%d\r\n", 
        methodcall_info->cloud_id, device_info->dev_handle, 
        dbus_message_get_reply_serial(methodcall_info->reply));

    if(strcmp(methodcall_info->method_name, DMP_METHOD_CALLMETHOD))
    {
        info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
        goto err;
    }

    object = cJSON_Parse(methodcall_info->params);
    if(NULL != object)
    {
        item = cJSON_GetObjectItem(object, "params");
    }

    if(!strcmp(methodcall_info->service_name, LEDA_DEV_METHOD_GET_PROPERTIES))
    {
        params_count = leda_transform_data_json_to_struct(device_info->product_key, methodcall_info->service_name, item, &dev_data_input);        
        if ((params_count == 0) || (NULL == dev_data_input))
        {
            info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
            goto err;
        }

        ret = (*device_info->get_properties_cb)(device_info->dev_handle, 
                                                dev_data_input, 
                                                params_count, 
                                                device_info->usr_data);        
        params = leda_transform_data_struct_to_string(dev_data_input, params_count);        
        info = leda_retmsg_create(ret, params);
    }
    else if(!strcmp(methodcall_info->service_name, LEDA_DEV_METHOD_SET_PROPERTIES))
    {
        params_count = leda_transform_data_json_to_struct(device_info->product_key, methodcall_info->service_name, item, &dev_data_input);
        if ((params_count == 0) || (NULL == dev_data_input))
        {
            info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
            goto err;
        }
        
        ret = (*device_info->set_properties_cb)(device_info->dev_handle, 
                                                dev_data_input, 
                                                params_count, 
                                                device_info->usr_data);
        info = leda_retmsg_create(ret, NULL);
    }
    else
    {
        params_count = leda_transform_data_json_to_struct(device_info->product_key, methodcall_info->service_name, item, &dev_data_input);
        if (device_info->service_output_max_count > 0)
        {
            dev_data_output = malloc(sizeof(leda_device_data_t) * device_info->service_output_max_count);
            if (NULL == dev_data_output)
            {
                info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
                goto err;
            }

            memset(dev_data_output, 0, sizeof(leda_device_data_t) * device_info->service_output_max_count);
            for (i = 0; i < device_info->service_output_max_count; i++)
            {
                dev_data_output[i].type = LEDA_TYPE_BUTT;
            }
        }

        ret = (*device_info->call_service_cb)(device_info->dev_handle, 
                                                methodcall_info->service_name, 
                                                dev_data_input, 
                                                params_count, 
                                                dev_data_output, 
                                                device_info->usr_data);
        params = leda_mothedret_create(ret, dev_data_output, device_info->service_output_max_count);
        info = leda_retmsg_create(ret, params);
    }

err:    
    log_d(LEDA_TAG_NAME, "return cloud_id:%s, method:%s, services:%s, serial:%d params:%s \r\n", 
                         methodcall_info->cloud_id, 
                         methodcall_info->method_name, 
                         methodcall_info->service_name, 
                         dbus_message_get_reply_serial(methodcall_info->reply), 
                         info);

    leda_delete_device_info(device_info);

    dbus_message_append_args(methodcall_info->reply, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
    dbus_connection_send(methodcall_info->connection, methodcall_info->reply, NULL);
    leda_retmsg_free(info);
    dbus_message_unref(methodcall_info->reply);

    if(methodcall_info->cloud_id)
    {
        free(methodcall_info->cloud_id);
    }
    if(methodcall_info->method_name)
    {
        free(methodcall_info->method_name);
    }
    if(methodcall_info->service_name)
    {
        free(methodcall_info->service_name);
    }
    if(methodcall_info->params)
    {
        free(methodcall_info->params);
    }
    if(methodcall_info)
    {
        free(methodcall_info);
    }
    if(NULL != object)
    {
        cJSON_Delete(object);
    }
    if(NULL != dev_data_input)
    {
        free(dev_data_input);
    }
    if(NULL != dev_data_output)
    {
        free(dev_data_output);
    }
    if(NULL != params)
    {
        free(params);
    }
    return NULL;
}

static void _leda_methodcb_send(DBusConnection *connection, char *cloud_id, DBusMessage *call_msg, DBusMessage *reply)
{
    const char *method_name = NULL;
    char *service_name = NULL;
    char *params = NULL;
    char *info = NULL;
    DBusError dbus_error;
    leda_methodcall_info_t *methodcall_info = NULL;

    dbus_error_init(&dbus_error);
    method_name = dbus_message_get_member(call_msg);

    if(NULL == method_name)
    {
        info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
    }
    else
    {
        methodcall_info = (leda_methodcall_info_t *)malloc(sizeof(leda_methodcall_info_t));
        if(NULL == methodcall_info)
        {
            info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
            goto err;
        }
        memset(methodcall_info, 0, sizeof(leda_methodcall_info_t));
        
        methodcall_info->cloud_id = (char *)malloc(strlen(cloud_id)+1);
        if(NULL == methodcall_info->cloud_id)
        {
            info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
            goto err;
        }
        strcpy(methodcall_info->cloud_id, cloud_id);
        
        methodcall_info->method_name = (char *)malloc(strlen(method_name)+1);
        if(NULL == methodcall_info->method_name)
        {
            info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
            goto err;
        }
        strcpy(methodcall_info->method_name, method_name);

        if(!strcmp(methodcall_info->method_name, DMP_METHOD_CALLMETHOD))
        {
            dbus_message_get_args(call_msg, &dbus_error, DBUS_TYPE_STRING, &service_name, 
                DBUS_TYPE_STRING, &params, DBUS_TYPE_INVALID);
            if(NULL == service_name)
            {
                info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
                goto err;
            }
            if((dbus_error_is_set(&dbus_error)) || (NULL == params))
            {
                methodcall_info->params = NULL;
            }
            else
            {
                methodcall_info->params = (char *)malloc(strlen(params)+1);
                if(NULL == methodcall_info->params)
                {
                    info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
                    goto err;
                }
                strcpy(methodcall_info->params, params);
            }
            methodcall_info->service_name = (char *)malloc(strlen(service_name)+1);
            if(NULL == methodcall_info->service_name)
            {
                info = leda_retmsg_create(LE_ERROR_ALLOCATING_MEM, NULL);
                goto err;
            }
            strcpy(methodcall_info->service_name, service_name);
        }
        else
        {
            log_e(LEDA_TAG_NAME, "method:%s not support\r\n", methodcall_info->method_name);
            info = leda_retmsg_create(LE_ERROR_INVAILD_PARAM, NULL);
            goto err;
        }
        
        methodcall_info->connection = connection;
        methodcall_info->reply = reply;

        log_d(LEDA_TAG_NAME, "add thread cloud_id:%s, method:%s, services:%s, serial:%d, params:%s \r\n", 
            methodcall_info->cloud_id, methodcall_info->method_name, 
            methodcall_info->service_name, dbus_message_get_reply_serial(reply), methodcall_info->params);
        leda_pool_add_worker(&_leda_methodcb_proc, (void *)methodcall_info);
    }

err:
    if(NULL != info)
    {
        dbus_message_append_args(reply, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
        dbus_connection_send(connection, reply, NULL);
        dbus_message_unref(reply);
        leda_retmsg_free(info);
        if(methodcall_info)
        {
            if(methodcall_info->cloud_id)
            {
                free(methodcall_info->cloud_id);
            }
            if(methodcall_info->method_name)
            {
                free(methodcall_info->method_name);
            }
            if(methodcall_info->service_name)
            {
                free(methodcall_info->service_name);
            }
            if(methodcall_info->params)
            {
                free(methodcall_info->params);
            }
            free(methodcall_info);
        }
    }
    dbus_error_free(&dbus_error);
}

void *leda_methodcb_thread(void *arg)
{
    leda_connect_info_t *connect_info = (leda_connect_info_t *)arg;
    DBusMessage *message = NULL;
    DBusMessage *reply = NULL;
    const char *method_name = NULL;
    int msg_type;
    char *cloud_id = NULL;
    
    log_d(LEDA_TAG_NAME, "leda_method_thread\r\n");

    if ((NULL == connect_info) || (NULL == connect_info->connection))
    {
        log_e(LEDA_TAG_NAME, "connect_info is invalid\r\n");
        pthread_exit(0);
    }

    leda_pool_init(connect_info->trpool_num);

    prctl(PR_SET_NAME, "leda_main_thread");

    while (dbus_connection_get_is_connected(connect_info->connection))
    {
        dbus_connection_read_write(connect_info->connection, 10);
        do
        {
            message = dbus_connection_pop_message(connect_info->connection);
            if (message == NULL)
            {
                break;
            }

            msg_type = dbus_message_get_type(message);
            if (DBUS_MESSAGE_TYPE_METHOD_RETURN == msg_type)
            {
                _leda_method_reply_proc(message);
                continue;
            }

            if (LE_SUCCESS == leda_path_is_vaild(dbus_message_get_path(message)))
            {
                cloud_id = (char *)malloc(strlen(dbus_message_get_path(message))-strlen(LEDA_PATH_NAME)+1);
                if (NULL == cloud_id)
                {
                    dbus_message_unref(message);
                    continue;
                }
                snprintf(cloud_id, (strlen(dbus_message_get_path(message))-strlen(LEDA_PATH_NAME)+1), 
                    "%s", dbus_message_get_path(message)+strlen(LEDA_PATH_NAME));
            }
            else if (LE_SUCCESS == leda_interface_is_vaild(dbus_message_get_interface(message)))
            {
                cloud_id = (char *)malloc(strlen(dbus_message_get_interface(message))-strlen(LEDA_DEVICE_WKN)+1);
                if (NULL == cloud_id)
                {
                    dbus_message_unref(message);
                    continue;
                }
                snprintf(cloud_id, (strlen(dbus_message_get_interface(message))-strlen(LEDA_DEVICE_WKN)+1), 
                    "%s", dbus_message_get_interface(message)+strlen(LEDA_DEVICE_WKN));
            }
            else if (LE_SUCCESS == leda_interface_is_vaild(dbus_message_get_destination(message)))
            {
                cloud_id = (char *)malloc(strlen(dbus_message_get_destination(message))-strlen(LEDA_DEVICE_WKN)+1);
                if (NULL == cloud_id) 
                {
                    dbus_message_unref(message);
                    continue;
                }
                snprintf(cloud_id, (strlen(dbus_message_get_destination(message))-strlen(LEDA_DEVICE_WKN)+1), 
                    "%s", dbus_message_get_destination(message)+strlen(LEDA_DEVICE_WKN));
            }
            else
            {   
                if (NULL != dbus_message_get_interface(message) 
                    && !strcmp(DBUS_PROPERTIES_CHANGE_INTERFACE, dbus_message_get_interface(message)))
                {
                    reply = dbus_message_new_method_return(message);
                    dbus_connection_send(connect_info->connection, reply, NULL);
                    dbus_message_unref(reply);
                }
                else
                {
                    msg_type = dbus_message_get_type(message);
                    if (DBUS_MESSAGE_TYPE_METHOD_CALL == msg_type)
                    {
                        if (_leda_method_is_driver_message_proc(message))
                        {
                            _leda_device_message_proc(connect_info->connection, message);
                        }
                        else
                        {
                            _leda_driver_message_proc(connect_info->connection, message);
                        }
                    }
                }

                dbus_message_unref(message);
                continue;
            }

            switch (msg_type)
            {
            case DBUS_MESSAGE_TYPE_METHOD_CALL:
                {
                    reply = dbus_message_new_method_return(message);
                    method_name = dbus_message_get_member(message);
                    if (!strcmp(method_name, DMP_METHOD_INTROSPECT))
                    {
                        _leda_introspect_proc(connect_info->connection, cloud_id, reply);
                    }
                    else
                    {
                        if (NULL != leda_get_methodcb_by_cloud_id(cloud_id))
                        {
                            _leda_methodcb_send(connect_info->connection, cloud_id, message, reply);
                        }
                    }
                    break;
                }
            default:
                {
                    break;
                } 
            }
            if(cloud_id)
            {
                free(cloud_id);
            }
            dbus_message_unref(message);
        } while(message != NULL);

        if (RUN_STATE_EXIT == g_run_state)
        {
            leda_pool_destroy();
            free(connect_info);
            pthread_exit(0);
        }
    }

    if (RUN_STATE_NORMAL == g_run_state)
    {
        log_f(LEDA_TAG_NAME, "the connection with the dbus daemon disconnect!!! the driver have to exit!!!\r\n");
        exit(0);
    }

    return NULL;
}

leda_device_configcb_t *leda_get_device_configcb_by_name(const char *module_name)
{
    leda_device_configcb_t *pos             = NULL; 
    leda_device_configcb_t *next            = NULL; 
    leda_device_configcb_t *device_configcb = NULL;

    pthread_mutex_lock(&g_device_configcb_lock);
    list_for_each_entry_safe(pos, next, &leda_device_configcb_head, list_node)
    {
        if (strcmp(pos->module_name, module_name) == 0)
        {
            device_configcb = pos;
            break;
        }
    }
    pthread_mutex_unlock(&g_device_configcb_lock);

    return device_configcb;
}

int leda_insert_device_configcb(const char *module_name, config_changed_callback onchanged_device_configcb)
{
    char                    *tmp_module_name = NULL;
    leda_device_configcb_t  *device_configcb = NULL;

    device_configcb = leda_get_device_configcb_by_name(module_name);
    if (NULL != device_configcb)
    {
        device_configcb->device_configcb = onchanged_device_configcb;
        return LE_SUCCESS;        
    }

    device_configcb = (leda_device_configcb_t *)malloc(sizeof(leda_device_configcb_t));
    if (NULL == device_configcb)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    tmp_module_name = (char*)malloc(strlen(module_name) + 1);
    if (NULL == tmp_module_name)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        free(device_configcb);
        return LE_ERROR_ALLOCATING_MEM;
    }
    strcpy(tmp_module_name, module_name);

    device_configcb->module_name = tmp_module_name;
    device_configcb->device_configcb = onchanged_device_configcb;

    pthread_mutex_lock(&g_device_configcb_lock);
    list_add(&device_configcb->list_node, &leda_device_configcb_head);
    pthread_mutex_unlock(&g_device_configcb_lock);

    return LE_SUCCESS;
}

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
