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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <cJSON.h>
#include <dbus/dbus.h>
#include <sys/time.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"
#include "linux-list.h"
#include "leda_base.h"
#include "leda_methodcb.h"

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

typedef enum leda_info_type
{
    INFO_TYPE_DEVICE_CONFIG = 0,                                    /* 设备配置信息, 获取时key对应驱动模块ID*/
    INFO_TYPE_PRODUCT_TSL,                                          /* 产品tsl, 获取时key对应product_key*/
    INFO_TYPE_PRODUCT_TSL_CONFIG,                                   /* 产品tsl扩展信息, 获取时key对应product_key*/

    INFO_TYPE_BUTT
} leda_data_info_e;

static char                                 *g_module_id   = NULL;
static char                                 *g_module_name = NULL;
static DBusConnection                       *g_connection  = NULL;  /* dbus连接句柄 */
static pthread_t                            g_methodcb_thread_id;   /* 线程句柄 */
static device_handle_t                      g_device_handle  = 0;   /* device handle分配索引 */

extern pthread_mutex_t                      g_methodcb_list_lock;
extern pthread_mutex_t                      g_leda_reply_lock;
extern pthread_mutex_t                      g_device_configcb_lock;

static device_handle_t _get_unique_device_handle(void)
{
    return g_device_handle++;
}

static long long _leda_get_current_time_ms()
{
    long long current_time_ms = 0;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    current_time_ms = ((long long)tv.tv_sec * 1000) + tv.tv_usec / 1000;

    return current_time_ms;
}

static DBusMessage *_leda_create_methodcall(const char *wkn, const char *method)
{
    DBusMessage *msg_call   = NULL;
    char        *obj_path   = NULL;

    obj_path = (char *)malloc(strlen(wkn) + 2);
    if (NULL == obj_path)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return NULL;
    }

    memset(obj_path, 0, strlen(wkn) + 2);
    leda_wkn_to_path(wkn, obj_path);
    
    log_d(LEDA_TAG_NAME, "new method wkn: %s obj_path: %s method: %s\n", wkn, obj_path, method);

    msg_call = dbus_message_new_method_call(wkn, obj_path, wkn, method);
    free(obj_path);

    return msg_call;
}

static int _leda_send_with_replay(DBusMessage *msg_call, int timeout_milliseconds, leda_retinfo_t *retinfo)
{
    int         ret         = LE_SUCCESS;
    uint32_t    serial_id;
    leda_reply_t *bus_reply = NULL;

    leda_retmsg_init(retinfo);
    if (FALSE == dbus_connection_send(g_connection, msg_call, &serial_id))
    {
        log_w(LEDA_TAG_NAME, "dbus send failed\n");
        return LE_ERROR_UNKNOWN;
    }

    bus_reply = leda_insert_send_reply(serial_id);
    if (NULL == bus_reply)
    {
        log_w(LEDA_TAG_NAME, "serial_id insert failed\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    if (LE_SUCCESS != leda_get_reply_params(bus_reply, timeout_milliseconds))
    {
        log_w(LEDA_TAG_NAME, "get_reply: %d failed\n", serial_id);
        leda_remove_reply(bus_reply);
        return LE_ERROR_UNKNOWN;
    }

    ret = leda_retmsg_parse(bus_reply->reply, dbus_message_get_member(msg_call), retinfo);
    leda_remove_reply(bus_reply);
    
    log_d(LEDA_TAG_NAME, 
          "ret: %d retmsg: {code: %d message: %s params: %s}\n",
          ret,
          retinfo->code,
          retinfo->message,
          retinfo->params);

    return ret;
}

static int _leda_check_wkn(const char *head, const char *params)
{
    dbus_bool_t ret;
    char        *wkn;
    DBusError   dbus_error;

    wkn = (char *)malloc(strlen(head) + strlen(params) + 1);
    if (NULL == wkn)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    snprintf(wkn, strlen(head) + strlen(params) + 1, "%s%s", head, params);
    
    dbus_error_init(&dbus_error);
    ret = dbus_bus_name_has_owner(g_connection, wkn, &dbus_error);
    dbus_error_free(&dbus_error);
    free(wkn);

    if (FALSE != ret)
    {
        log_w(LEDA_TAG_NAME, "%s when check wkn %s\n", dbus_error.message, params);
        return LE_ERROR_UNKNOWN;
    }

    return LE_SUCCESS;
}

static int _leda_request_wkn(const char *head, const char *params)
{
    DBusMessage *message    = NULL;
    char        *wkn        = NULL;
    int         flags       = 0;

    wkn = (char *)malloc(strlen(head) + strlen(params) + 1);
    if (NULL == wkn)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    snprintf(wkn, strlen(head)+strlen(params)+1, "%s%s", head, params);
    
    message = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "RequestName");
    if (message == NULL)
    {
        log_w(LEDA_TAG_NAME, "create dbus new method failed cloud_id: %s\n", params);
        free(wkn);
        return LE_ERROR_UNKNOWN;
    }

    if (!dbus_message_append_args(message,DBUS_TYPE_STRING, &wkn, DBUS_TYPE_UINT32, &flags, DBUS_TYPE_INVALID))
    {
        dbus_message_unref (message);
        free(wkn);
        log_w(LEDA_TAG_NAME, "dbus message append failed cloud_id: %s\n", params);
        return LE_ERROR_UNKNOWN;
    }

    dbus_connection_send(g_connection, message, NULL);
    dbus_message_unref(message);
    free(wkn);

    return LE_SUCCESS;
}

static int _leda_release_wkn(const char *head, const char *params)
{
    DBusMessage *message    = NULL;
    char        *wkn        = NULL;
 
    wkn = (char *)malloc(strlen(head)+strlen(params)+1);
    if (NULL == wkn)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    snprintf(wkn, strlen(head)+strlen(params) + 1, "%s%s", head, params);

    message = dbus_message_new_method_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "ReleaseName");
    if (message == NULL)
    {
        log_w(LEDA_TAG_NAME, "create dbus new method failed cloud_id: %s\n", params);
        free(wkn);
        return LE_ERROR_UNKNOWN;
    }
 
    if (!dbus_message_append_args(message,DBUS_TYPE_STRING, &wkn, DBUS_TYPE_INVALID))
    {
        dbus_message_unref (message);
        free(wkn);
        log_w(LEDA_TAG_NAME, "dubs message append failed cloud_id: %s\n", params);
        return LE_ERROR_UNKNOWN;
    }
  
    dbus_connection_send(g_connection, message, NULL);
    dbus_message_unref(message);
    free(wkn);

    return LE_SUCCESS;
}

static int _leda_send_signal(const char *signal_name, const char *cloud_id, char *output)
{
    cJSON           *object         = NULL;
    char            *path           = NULL;
    char            *interface      = NULL;
    DBusMessage     *signal_msg     = NULL;

    if (LE_SUCCESS != leda_string_validate_utf8(output, strlen(output)))
    {
        log_w(LEDA_TAG_NAME, "output: %s is invaild\n", output);
        return LE_ERROR_INVAILD_PARAM;
    }
    else
    {
        object = cJSON_Parse(output);
        if (NULL == object)
        {
            log_w(LEDA_TAG_NAME, "output: %s parse failed\n", output);
            return LEDA_ERROR_INVALID_JSON;
        }
        cJSON_Delete(object);
    }

    path = (char *)malloc(strlen(LEDA_DEVICE_WKN) + strlen(cloud_id) + 2);
    if (NULL == path)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    interface = (char *)malloc(strlen(LEDA_DEVICE_WKN) + strlen(cloud_id) + 1);
    if (NULL == interface)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        free(path);
        return LE_ERROR_ALLOCATING_MEM;
    }
    snprintf(interface, strlen(LEDA_DEVICE_WKN)+strlen(cloud_id)+1, "%s%s", LEDA_DEVICE_WKN, cloud_id);
    leda_wkn_to_path(interface, path);
    
    signal_msg = dbus_message_new_signal(path, interface, signal_name);
    if (NULL == signal_msg)
    {
        log_w(LEDA_TAG_NAME, "create dbus new signal failed\n");
        free(path);
        free(interface);
        return LE_ERROR_UNKNOWN;
    }

    dbus_message_append_args(signal_msg, DBUS_TYPE_STRING, &output, DBUS_TYPE_INVALID);
    dbus_message_set_destination(signal_msg, DMP_SUB_WELL_KNOWN_NAME);
    if (TRUE != dbus_connection_send(g_connection, signal_msg, NULL))
    {
        log_w(LEDA_TAG_NAME, "dbus send failed\n");
        free(path);
        free(interface);
        dbus_message_unref(signal_msg);
        return LE_ERROR_UNKNOWN;
    }

    log_d(LEDA_TAG_NAME, "new_signal interface: %s signal_name: %s cloud_id: %s output: %s\n", interface, signal_name, cloud_id, output);

    free(path);
    free(interface);
    dbus_message_unref(signal_msg);

    return LE_SUCCESS;
}

static int _leda_add_property_timestamp(const char *cloud_id, char *output)
{
    cJSON       *object       = NULL; 
    cJSON       *item         = NULL; 
    cJSON       *new_object   = NULL;
    cJSON       *new_item     = NULL;
    char        *buff         = NULL;
    int         ret           = LE_SUCCESS;

    object = cJSON_Parse(output);
    if(NULL == object)
    {
        log_w(LEDA_TAG_NAME, "output: %s parse failed\n", output);
        return LEDA_ERROR_INVALID_JSON;
    }

    new_object = cJSON_CreateObject();
    if (NULL == new_object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return LEDA_ERROR_INVALID_JSON;
    }

    cJSON_ArrayForEach(item, object)
    {
        new_item = cJSON_CreateObject();
        if (NULL == new_item)
        {
            log_w(LEDA_TAG_NAME, "no memory can allocate\n");
            break;
        }

        cJSON_AddNumberToObject(new_item, "time", _leda_get_current_time_ms());
        if (cJSON_False == item->type)
        {
            cJSON_AddFalseToObject(new_item, "value");
        }
        else if (cJSON_True == item->type)
        {
            cJSON_AddTrueToObject(new_item, "value");
        }
        else if (cJSON_NULL == item->type)
        {
            cJSON_AddNullToObject(new_item, "value");
        }
        else if (cJSON_Number == item->type)
        {
            cJSON_AddNumberToObject(new_item, "value", item->valuedouble);
        }
        else if (cJSON_String == item->type)
        {
            cJSON_AddStringToObject(new_item, "value", item->valuestring);
        }
        else if ((cJSON_Array == item->type) || (cJSON_Object == item->type))
        {
            cJSON_AddItemToObject(new_item, "value", cJSON_Duplicate(item, 1));
        }
        else if (cJSON_Raw == item->type)
        {
            cJSON_AddRawToObject(new_item, "value", item->valuestring);
        }
        else
        {
            cJSON_Delete(new_item);
            continue;
        }
        cJSON_AddItemToObject(new_object, item->string, new_item);
    }
    cJSON_Delete(object);

    buff = cJSON_PrintUnformatted(new_object);
    ret  = _leda_send_signal(LEDA_PROPERTY_CHANGED, cloud_id, buff);
    cJSON_Delete(new_object);
    cJSON_free(buff);

    return ret;
}

/*
 * 上报属性, 设备具有的属性在设备能力描述在设备产品物模型tsl规定.
 *
 * 上报属性, 可以上报一个, 也可以多个一起上报.
 *
 * dev_handle:          设备在linkedge本地唯一标识.
 * properties:          @leda_device_data_t, 属性数组.
 * properties_count:    本次上报属性个数.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_report_properties(device_handle_t dev_handle, const leda_device_data_t properties[], int properties_count)
{
    int                 ret             = LE_SUCCESS;
    char*               buff            = NULL;
    leda_device_info_t  *device_info    = NULL;

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't register\n", dev_handle);
        return LEDA_ERROR_DEVICE_UNREGISTER;
    }
    else if (STATE_ONLINE != device_info->online)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't online\n", dev_handle);
        return LEDA_ERROR_DEVICE_OFFLINE;
    }

    if (NULL == properties || 0 == properties_count)
    {
        log_w(LEDA_TAG_NAME, "no properties need report\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    buff = leda_transform_data_struct_to_string(properties, properties_count);
    if (NULL == buff)
    {
        return LE_ERROR_ALLOCATING_MEM;
    }
    
    ret = _leda_add_property_timestamp(device_info->cloud_id, buff);
    free(buff);
    buff = NULL;

    return ret;
}

static int _leda_add_event_timestamp(const char* event_name, const char *cloud_id, char *output)
{
    cJSON       *object       = NULL; 
    cJSON       *new_object   = NULL;
    cJSON       *new_item     = NULL;
    char        *buff         = NULL;
    int         ret           = LE_SUCCESS;

    if (NULL != output)
    {
        object = cJSON_Parse(output);
        if (NULL == object)
        {
            log_w(LEDA_TAG_NAME, "output: %s parse failed\n", output);
            return LEDA_ERROR_INVALID_JSON;
        }
    }
    else
    {
        object = cJSON_Parse("{}");
    }

    new_object = cJSON_CreateObject();
    if (NULL == new_object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return LEDA_ERROR_INVALID_JSON;
    }

    new_item = cJSON_CreateObject();
    if (NULL == new_item)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(new_object);
        cJSON_Delete(object);
        return LEDA_ERROR_INVALID_JSON;
    }

    cJSON_AddNumberToObject(new_item, "time", _leda_get_current_time_ms());
    cJSON_AddItemToObject(new_item, "value", cJSON_Duplicate(object, 1));
    cJSON_AddItemToObject(new_object, "params", new_item);

    buff = cJSON_PrintUnformatted(new_object);
    ret = _leda_send_signal(event_name, cloud_id, buff);

    cJSON_Delete(object);
    cJSON_Delete(new_object);
    cJSON_free(buff);

    return ret;
}

/*
 * 上报事件, 设备具有的事件上报能力在设备产品物模型tsl有规定.
 *
 * 
 * dev_handle:  设备在linkedge本地唯一标识.
 * event_name:  事件名称.
 * data:        @leda_device_data_t, 事件参数数组.
 * data_count:  事件参数数组长度.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_report_event(device_handle_t dev_handle, const char *event_name, const leda_device_data_t data[], int data_count)
{
    int                 ret             = LE_SUCCESS;
    char*               buff            = NULL;
    leda_device_info_t  *device_info    = NULL;

    if (NULL == event_name)
    {
        log_w(LEDA_TAG_NAME, "event_name is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't register\n", dev_handle);
        return LEDA_ERROR_DEVICE_UNREGISTER;
    }
    else if (STATE_ONLINE != device_info->online)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't online\n", dev_handle);
        return LEDA_ERROR_DEVICE_OFFLINE;
    }

    if (NULL == data || 0 == data_count)
    {
        ret = _leda_add_event_timestamp(event_name, device_info->cloud_id, NULL);
    }
    else
    {
        buff = leda_transform_data_struct_to_string(data, data_count);
        if (NULL == buff)
        {
            return LE_ERROR_ALLOCATING_MEM;
        }

        ret = _leda_add_event_timestamp(event_name, device_info->cloud_id, buff);
        free(buff);
        buff = NULL;
    }

    return ret;
}

static device_handle_t _leda_register_and_online(const char *product_key, 
                                                 int is_local_name, 
                                                 const char *name, 
                                                 const leda_device_callback_t *device_cb, 
                                                 int is_local, 
                                                 void *usr_data)
{
    int                 ret;
    cJSON               *object                     = NULL;
    char                *info                       = NULL;
    char                *cloud_id                   = NULL;
    DBusMessage         *msg_call                   = NULL;
    leda_retinfo_t      retinfo;
    leda_device_info_t  *device_info                = NULL;    

    if (NULL == product_key
        || (LE_SUCCESS != leda_string_validate_utf8(product_key, strlen(product_key))))
    {
        log_w(LEDA_TAG_NAME, "product_key: %s is invalid!\n", product_key);
        return INVALID_DEVICE_HANDLE;
    }

    if ((NULL == name) 
        || (LE_SUCCESS != leda_string_validate_utf8(name, strlen(name))))
    {
        log_w(LEDA_TAG_NAME, "name: %s is invalid!\n", name);
        return INVALID_DEVICE_HANDLE;
    }

    if ((NULL == device_cb)
        || (NULL == device_cb->get_properties_cb)
        || (NULL == device_cb->set_properties_cb)
        || (NULL == device_cb->call_service_cb))
    {
        log_w(LEDA_TAG_NAME, "device_cb is invalid!\n");
        return INVALID_DEVICE_HANDLE;
    }

    device_info = leda_get_methodcb_by_dn_pk(product_key, name);
    if (NULL != device_info)
    {
        if (STATE_ONLINE == device_info->online)
        {   
            return device_info->dev_handle;
        }
    }

    object = cJSON_CreateObject();
    if (NULL == object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
    }

    cJSON_AddStringToObject(object, "productKey",       product_key);
    cJSON_AddStringToObject(object, "isLocal",          is_local ? "True" : "False");
    cJSON_AddStringToObject(object, "driverName",       g_module_name);
    if (0 == is_local_name)
    {
        cJSON_AddStringToObject(object, "deviceName",   name);
    }
    else
    {
        cJSON_AddStringToObject(object, "deviceLocalId", name);
    }

    info = cJSON_PrintUnformatted(object);
    if (NULL == info)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return INVALID_DEVICE_HANDLE;
    }

    msg_call = _leda_create_methodcall(DMP_DIMU_WELL_KNOWN_NAME, DMP_METHOD_CONNECT);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        cJSON_Delete(object);
        cJSON_free(info);
        return INVALID_DEVICE_HANDLE;
    }

    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
    log_d(LEDA_TAG_NAME, "device_identifier:%s\n", info);
    
    cJSON_Delete(object);
    cJSON_free(info);

    retinfo.message = NULL;
    retinfo.params = NULL;
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "call register and online deivce method failed, ret: %d code: %d, msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);

        return INVALID_DEVICE_HANDLE;
    }
    
    cloud_id = leda_params_parse(retinfo.params, "deviceCloudId");
    leda_retinfo_free(&retinfo);
    if (NULL == cloud_id)
    {
        log_w(LEDA_TAG_NAME, "parse deviceCloudId feild failed\n");
        return INVALID_DEVICE_HANDLE;
    }
    log_d(LEDA_TAG_NAME, "cloud_id:%s\n", cloud_id);

    device_info = leda_get_methodcb_by_cloud_id(cloud_id);
    if (NULL == device_info)
    {
        device_info = leda_insert_methodcb(cloud_id, 
                                           _get_unique_device_handle(), 
                                           product_key, 
                                           is_local_name, 
                                           name, 
                                           device_cb, 
                                           is_local, 
                                           usr_data);
        if (NULL == device_info)
        {
            log_w(LEDA_TAG_NAME, "device %s insert method failed\n", name);
            free(cloud_id);
            return INVALID_DEVICE_HANDLE;
        }
    }
    free(cloud_id);

    ret = _leda_request_wkn(LEDA_DEVICE_WKN, (char *)device_info->cloud_id);
    if (LE_SUCCESS != ret)
    {
        log_w(LEDA_TAG_NAME, "call request wkn method failed, ret: %d product_key: %s name: %s\n", ret, product_key, name);
        return LE_ERROR_UNKNOWN;
    }

    leda_set_methodcb_online(device_info->dev_handle, STATE_ONLINE);
    log_d(LEDA_TAG_NAME, "register_and_online: %d\n", device_info->dev_handle);

    return device_info->dev_handle;
}

/*
 * 下线设备, 假如设备工作在不正常的状态或设备退出前, 可以先下线设备, 这样LinkEdge就不会继续下发消息到设备侧.
 *
 * dev_handle:  设备在linkedge本地唯一标识.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_offline(device_handle_t dev_handle)
{
    int                 ret = LE_SUCCESS;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call       = NULL;
    cJSON               *object         = NULL;
    char                *info           = NULL;
    leda_device_info_t  *device_info    = NULL;

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "dev_handle:%d hasn't register\n", dev_handle);
        return LE_ERROR_INVAILD_PARAM;
    }   

    msg_call = _leda_create_methodcall(DMP_DIMU_WELL_KNOWN_NAME, DMP_METHOD_DISCONNECT);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    object = cJSON_CreateObject();
    if (NULL == object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    cJSON_AddStringToObject(object, "deviceCloudId", device_info->cloud_id);

    info = cJSON_PrintUnformatted(object);
    if (NULL == info)
    {
        cJSON_Delete(object);
        dbus_message_unref(msg_call);

        return LE_ERROR_ALLOCATING_MEM;
    }
    
    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
    cJSON_Delete(object);
    cJSON_free(info);

    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "call offline method failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        ret = LE_ERROR_UNKNOWN;
        goto END;
    }

    log_d(LEDA_TAG_NAME, "offline: %d\n", dev_handle);

END:
    leda_set_methodcb_online(dev_handle, STATE_OFFLINE);
    _leda_release_wkn(LEDA_DEVICE_WKN, device_info->cloud_id);
    leda_retinfo_free(&retinfo);

    return ret;
}

/*
 * 上线设备, 设备只有上线后, 才能被 LinkEdge 识别.
 *
 * dev_handle:  设备在linkedge本地唯一标识.
 *
 * 阻塞接口,成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_online(device_handle_t dev_handle)
{
    int                     ret             = LE_SUCCESS;
    leda_device_info_t      *device_info    = NULL;
    leda_device_callback_t  device_cb;

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't register\n", dev_handle);
        return LE_ERROR_INVAILD_PARAM;
    }
    else if (STATE_ONLINE == device_info->online)
    {
        return LE_SUCCESS;
    }

    device_cb.get_properties_cb = device_info->get_properties_cb;
    device_cb.set_properties_cb = device_info->set_properties_cb;
    device_cb.call_service_cb   = device_info->call_service_cb;

    _leda_register_and_online(device_info->product_key, 
                              device_info->is_local_name, 
                              device_info->dev_name, 
                              &device_cb, 
                              device_info->is_local, 
                              device_info->usr_data);

    log_d(LEDA_TAG_NAME, "online: %d\n", dev_handle);

    return ret;
}

/*
 * 通过已在云平台注册的device_name, 注册设备并上线设备, 申请设备唯一标识符.
 *
 * 设备默认注册后即上线
 *
 * product_key: 产品ProductKey
 * device_name: 由设备特征值组成的唯一描述信息, 必须保证每个待接入设备名称不同.
 * device_cb:   参考@leda_device_callback_t.
 * is_local:    设备是否需要上云, 1不上云, 0上云.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_inner_register_and_online_by_device_name(const char *product_key, const char *device_name, const leda_device_callback_t *device_cb, int is_local, void *usr_data)
{
    return _leda_register_and_online(product_key, 0, device_name, device_cb, is_local, usr_data);
}

/*
 * 通过已在阿里云物联网平台创建的设备device_name, 注册并上线设备, 申请设备唯一标识符.
 *
 * 若需要注册多个设备, 则多次调用该接口即可.
 *
 * product_key: 在阿里云物联网平台创建的产品ProductKey.
 * device_name: 在阿里云物联网平台创建的设备DeviceName.
 * device_cb:   请求调用设备回调函数结构体，详细描述见@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_register_and_online_by_device_name(const char *product_key, const char *device_name, leda_device_callback_t *device_cb, void *usr_data)
{
    return leda_inner_register_and_online_by_device_name(product_key, device_name, device_cb, 0, usr_data);
}

/*
 * 注册设备并上线设备, 申请设备唯一标识符, 若需要注册多个设备, 则多次调用该接口即可.
 *
 * 设备默认注册后即上线.
 *
 * product_key: 产品ProductKey.
 * local_name:  由设备特征值组成的唯一描述信息, 必须保证同一个product_key时，每个待接入设备名称不同.
 * device_cb:   设备回调函数结构体，详细描述@leda_device_callback.
 * is_local:    设备是否需要上云, 1不上云, 0上云.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_inner_register_and_online_by_local_name(const char *product_key, const char *local_name, const leda_device_callback_t *device_cb, int is_local, void *usr_data)
{
    return _leda_register_and_online(product_key, 1, local_name, device_cb, is_local, usr_data);
}

/*
 * 通过本地自定义设备名称, 注册并上线设备, 申请设备唯一标识符.
 *
 * 若需要注册多个设备, 则多次调用该接口即可.
 *
 * product_key: 在阿里云物联网平台创建的产品ProductKey.
 * local_name:  由设备特征值组成的唯一描述信息, 必须保证同一个product_key时，每个设备名称不同.
 * device_cb:   请求调用设备回调函数结构体，详细描述见@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 * 注: 在同一产品ProductKey条件设备注册, 不允许本接口和leda_register_and_online_by_device_name接口同时使用, 
 *     即每一个产品ProductKey设备注册必须使用同一接口, 否则设备注册会发生不可控行为.
 */
device_handle_t leda_register_and_online_by_local_name(const char *product_key, const char *local_name, leda_device_callback_t *device_cb, void *usr_data)
{
    return leda_inner_register_and_online_by_local_name(product_key, local_name, device_cb, 0, usr_data);
}

/*
 * 注销设备, 解除设备和云端关联.
 *
 * dev_handle:  设备在linkedge本地唯一标识.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_unregister(device_handle_t dev_handle)
{
    int                 ret             = LE_SUCCESS;
    cJSON               *object         = NULL;
    char                *info           = NULL;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call       = NULL;
    leda_device_info_t  *device_info    = NULL;

    device_info = leda_get_methodcb_by_device_handle(dev_handle);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "dev_handle: %d hasn't register\n", dev_handle);
        return LE_ERROR_INVAILD_PARAM;
    }
    else if (STATE_ONLINE == device_info->online)
    {
        leda_offline(device_info->dev_handle);
    }
    
    msg_call = _leda_create_methodcall(DMP_DIMU_WELL_KNOWN_NAME, DMP_METHOD_UNREGISTER_DEVICE);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    object = cJSON_CreateObject();
    if (NULL == object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    cJSON_AddStringToObject(object, "deviceCloudId", device_info->cloud_id);

    info = cJSON_PrintUnformatted(object);
    if (NULL == info)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return LE_ERROR_ALLOCATING_MEM;
    }

    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);
    cJSON_Delete(object);
    cJSON_free(info);

    retinfo.message = NULL;
    retinfo.params = NULL;
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "call unregister method failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        ret = LE_ERROR_UNKNOWN;
        goto END;
    }

    log_d(LEDA_TAG_NAME, "unregister: %d\n", dev_handle);

END:
    leda_retinfo_free(&retinfo);
    leda_remove_methodcb(dev_handle);

    return ret;
}

/*
 * 功能：驱动注销.
 *
 * module_name 驱动模块名称.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误错误码.
 */
static int _leda_unregister_driver(const char *module_name)
{
    int                 ret         = 0;
    cJSON               *object     = NULL;
    cJSON               *sub_object = NULL;
    char                *info       = NULL;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call   = NULL;
    
    msg_call = _leda_create_methodcall(DMP_DIMU_WELL_KNOWN_NAME, DMP_METHOD_UNREGISTER_DRIVER);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    sub_object = cJSON_CreateObject();
    if (NULL == sub_object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    cJSON_AddStringToObject(sub_object, "driverLocalId", module_name);

    object = cJSON_CreateObject();
    if (NULL == object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(sub_object);
        return LE_ERROR_ALLOCATING_MEM;
    }
    cJSON_AddItemToObject(object, "params", sub_object);

    info = cJSON_PrintUnformatted(object);
    if (NULL == info)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return LE_ERROR_ALLOCATING_MEM;
    }

    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);    
    cJSON_Delete(object);
    cJSON_free(info);

    retinfo.message = NULL;
    retinfo.params = NULL;
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "call unregister driver method failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);

        return LE_ERROR_UNKNOWN;
    }
    
    leda_retinfo_free(&retinfo);

    return LE_SUCCESS;
}

/*
 * 驱动注册.
 *
 * module_name 驱动模块名称.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误错误码.
 */
static int _leda_register_driver(const char *module_name)
{
    int                 ret         = 0;
    cJSON               *object     = NULL;
    cJSON               *sub_object = NULL;
    char                *info       = NULL;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call   = NULL;
    char                startup_time[128] = {0};
    
    msg_call = _leda_create_methodcall(DMP_DIMU_WELL_KNOWN_NAME, DMP_METHOD_REGISTER_DRIVER);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    sub_object = cJSON_CreateObject();
    if (NULL == sub_object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    sprintf(startup_time, "%lld", _leda_get_current_time_ms());
    cJSON_AddStringToObject(sub_object, "driverStartupTime", startup_time);
    cJSON_AddStringToObject(sub_object, "driverLocalId", module_name);

    object = cJSON_CreateObject();
    if (NULL == object)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(sub_object);
        return LE_ERROR_ALLOCATING_MEM;
    }
    cJSON_AddItemToObject(object, "params", sub_object);

    info = cJSON_PrintUnformatted(object);
    if (NULL == info)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        cJSON_Delete(object);
        return LE_ERROR_ALLOCATING_MEM;
    }

    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID);    
    cJSON_Delete(object);
    cJSON_free(info);

    retinfo.message = NULL;
    retinfo.params = NULL;
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "call register driver method failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);

        return LE_ERROR_UNKNOWN;
    }
    
    leda_retinfo_free(&retinfo);

    return LE_SUCCESS;
}

/*
 * 驱动模块初始化, 模块内部会创建工作线程池, 异步执行阿里云物联网平台下发的设备操作请求, 工作线程数目通过worker_thread_nums配置.
 *
 * module_id:           驱动模块ID.
 * module_name          驱动模块名称
 * worker_thread_nums : 线程池工作线程数, 该数值根据注册设备数量设置.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
static int _leda_init(const char *module_id, const char *module_name, int worker_thread_nums)
{
    DBusError           dbus_error;
    cJSON_Hooks         json_hooks;
    leda_connect_info_t *connect_info = NULL;

    log_d(LEDA_TAG_NAME, "driver info:     \n\
                          driver_id:   %s  \n\
                          driver_name: %s  \n", module_id, module_name);

    if ((NULL == module_id) 
        || (LE_SUCCESS != leda_string_validate_utf8(module_id, strlen(module_id))))
    {
        log_e(LEDA_TAG_NAME, "driver id %s is invalid, check deploy procedure please\n", module_id);
        return LE_ERROR_INVAILD_PARAM;
    }

    if (worker_thread_nums <= 0)
    {
        log_w(LEDA_TAG_NAME, "worker_thread_nums: %d is invalid\n", worker_thread_nums);
        return LE_ERROR_INVAILD_PARAM;
    }

    log_d(LEDA_TAG_NAME, "worker_thread_nums: %d\n", worker_thread_nums);

    dbus_error_init(&dbus_error);
    g_connection = dbus_connection_open(bus_address, &dbus_error);
    while ((dbus_error_is_set(&dbus_error)) || (NULL == g_connection))
    {
        log_w(LEDA_TAG_NAME, "dbus connection failed: %s, will retry again\n", dbus_error.message);
        sleep(5);
        dbus_error_free(&dbus_error);
        dbus_error_init(&dbus_error);
        g_connection = dbus_connection_open(bus_address, &dbus_error);
    }
    dbus_error_free(&dbus_error);

    dbus_error_init(&dbus_error);
    if ((TRUE != dbus_bus_register(g_connection, &dbus_error)) 
        || (dbus_error_is_set(&dbus_error)))
    {
        log_w(LEDA_TAG_NAME, "register dbus failed: %s\n", dbus_error.message);
        dbus_error_free(&dbus_error);
        return LE_ERROR_UNKNOWN;
    }
    dbus_error_free(&dbus_error);

    if (LE_SUCCESS != _leda_check_wkn(LEDA_DRIVER_WKN, module_id))
    {
        return LE_ERROR_UNKNOWN;
    }

    if (LE_SUCCESS != _leda_request_wkn(LEDA_DRIVER_WKN, module_id))
    {
        return LE_ERROR_UNKNOWN;
    }

    connect_info = (leda_connect_info_t *)malloc(sizeof(leda_connect_info_t));
    if (NULL == connect_info)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    connect_info->trpool_num = worker_thread_nums;
    connect_info->connection = g_connection;

    pthread_mutex_init(&g_methodcb_list_lock, NULL);
    pthread_mutex_init(&g_leda_reply_lock, NULL);
    pthread_mutex_init(&g_device_configcb_lock, NULL);
    
    if (0 != pthread_create(&g_methodcb_thread_id, NULL, leda_methodcb_thread, (void *)connect_info))
    {
        log_w(LEDA_TAG_NAME, "create thread failed\n");
        return LE_ERROR_UNKNOWN;
    }

    json_hooks.malloc_fn = malloc;
    json_hooks.free_fn = free;
    cJSON_InitHooks(&json_hooks);

    if (LE_SUCCESS != _leda_register_driver(module_name))
    {
        log_w(LEDA_TAG_NAME, "register driver: %s failed\n", module_name);

        free(connect_info);
        connect_info = NULL;

        return LE_ERROR_UNKNOWN;
    }

    g_module_id = (char *)malloc(strlen(module_id) + 1);
    if (NULL == g_module_id)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    strcpy(g_module_id, module_id);

    g_module_name = (char *)malloc(strlen(module_name) + 1);
    if (NULL == g_module_name)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        free(g_module_id);
        g_module_id = NULL;
        return LE_ERROR_ALLOCATING_MEM;
    }
    strcpy(g_module_name, module_name);

    return LE_SUCCESS;
}

/*
 * 驱动模块初始化, 模块内部会创建工作线程池, 异步执行阿里云物联网平台下发的设备操作请求, 工作线程数目通过worker_thread_nums配置.
 *
 * worker_thread_nums : 线程池工作线程数, 该数值根据注册设备数量进行设置.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_init(int worker_thread_nums)
{
    if (NULL == getenv("FUNCTION_ID") || NULL == getenv("FUNCTION_NAME"))
    {
        log_w(LEDA_TAG_NAME, "the driver is not deployed, check it please\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    return _leda_init(getenv("FUNCTION_ID"), getenv("FUNCTION_NAME"), worker_thread_nums);
}

/*
 * 模块退出.
 *
 * 模块退出前, 释放资源.
 *
 * 阻塞接口.
 */
void leda_exit(void)
{
    log_i(LEDA_TAG_NAME, "driver exit\n");

    _leda_unregister_driver(g_module_name);
    leda_set_runstate(RUN_STATE_EXIT);

    pthread_join(g_methodcb_thread_id, NULL);
    pthread_mutex_destroy(&g_methodcb_list_lock);
    pthread_mutex_destroy(&g_leda_reply_lock);
    pthread_mutex_destroy(&g_device_configcb_lock);

    if (NULL != g_module_id)
    {
        free(g_module_id);
        g_module_id = NULL;
    }

    if (NULL != g_module_name)
    {
        free(g_module_name);
        g_module_name = NULL;
    }

    return;
}

static int _leda_get_config_result(char **request_key, char **value)
{
    int                 ret = LE_SUCCESS;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call = NULL;

    if (NULL == request_key)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    msg_call = _leda_create_methodcall(DMP_CONFIGMANAGER_WELL_KNOW_NAME, DMP_CONFIGMANAGER_METHOD_GET);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, request_key, DBUS_TYPE_INVALID);
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);

    if (LE_SUCCESS != ret || ((LE_SUCCESS != retinfo.code) || (NULL == retinfo.params) || (!strcmp(retinfo.params, ""))))
    {
        log_w(LEDA_TAG_NAME, "call get_config method failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);
        return LE_ERROR_UNKNOWN;
    }

    *value = malloc(strlen(retinfo.params) + 1);
    if (NULL == *value)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        leda_retinfo_free(&retinfo);
        return LE_ERROR_UNKNOWN;
    }
    strcpy(*value, retinfo.params);
    leda_retinfo_free(&retinfo);

    return ret;
}

static int _leda_get_config_info(leda_data_info_e type, const char *key, char **value)
{
    int                 ret          = LE_SUCCESS;
    char                *request_key = NULL;

    if (INFO_TYPE_DEVICE_CONFIG == type)
    {
        request_key = malloc(strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1);
        if (NULL == request_key)
        {
            log_w(LEDA_TAG_NAME, "no memory can allocate\n");
            return LE_ERROR_ALLOCATING_MEM;
        }

        memset(request_key, 0, (strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1));
        snprintf(request_key, (strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1), "%s%s", CONFIGMANAGER_DEVICE_HEADER, key);

        ret = _leda_get_config_result(&request_key, value);
        free(request_key);

        /* 使用驱动id获取配置失败后，再次使用驱动名称获取配置 */
        while (LE_SUCCESS != ret)
        {
            key = NULL;
            request_key = NULL;

            key = getenv("FUNCTION_NAME");
            request_key = malloc(strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1);
            if (NULL == request_key)
            {
                log_w(LEDA_TAG_NAME, "no memory can allocate\n");
                ret = LE_ERROR_ALLOCATING_MEM;
                break;
            }

            memset(request_key, 0, (strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1));
            snprintf(request_key, (strlen(key) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1), "%s%s", CONFIGMANAGER_DEVICE_HEADER, key);

            ret = _leda_get_config_result(&request_key, value);
            free(request_key);
            break;
        }

        return ret;
    }
    else if (INFO_TYPE_PRODUCT_TSL == type)
    {
        request_key = malloc(strlen(key) + strlen(CONFIGMANAGER_TSL_HEADER) + 1);
        if (NULL == request_key)
        {
            log_w(LEDA_TAG_NAME, "no memory can allocate\n");
            return LE_ERROR_ALLOCATING_MEM;
        }

        memset(request_key, 0, (strlen(key) + strlen(CONFIGMANAGER_TSL_HEADER) + 1));
        snprintf(request_key, (strlen(key) + strlen(CONFIGMANAGER_TSL_HEADER) + 1), "%s%s", CONFIGMANAGER_TSL_HEADER, key);
    }
    else if (INFO_TYPE_PRODUCT_TSL_CONFIG == type)
    {
        request_key = malloc(strlen(key) + strlen(CONFIGMANAGER_TSL_CONFIG_HEADER) + 1);
        if (NULL == request_key)
        {
            log_w(LEDA_TAG_NAME, "no memory can allocate\n");
            return LE_ERROR_ALLOCATING_MEM;
        }

        memset(request_key, 0, (strlen(key) + strlen(CONFIGMANAGER_TSL_CONFIG_HEADER) + 1));
        snprintf(request_key, (strlen(key) + strlen(CONFIGMANAGER_TSL_CONFIG_HEADER) + 1), "%s%s", CONFIGMANAGER_TSL_CONFIG_HEADER, key);
    }    
    else
    {
        return LE_ERROR_UNKNOWN;
    }

    ret = _leda_get_config_result(&request_key, value);
    free(request_key);

    return ret;
}

static int _leda_get_config_size(const char *module_id)
{
    int     ret     = 0;
    char    *value  = NULL;
    
    if ((NULL == module_id) || 
        (LE_SUCCESS != leda_string_validate_utf8(module_id, strlen(module_id))))
    {
        log_w(LEDA_TAG_NAME, "the driver is not deployed, check it please\n");
        return 0;
    }

    ret = _leda_get_config_info(INFO_TYPE_DEVICE_CONFIG, module_id, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        ret = strlen(value) + 1;
        free(value);

        return ret;
    }

    return 0;
}

static int _leda_get_config(const char *module_id, char *config, int size)
{
    int     ret     = LE_SUCCESS;
    char    *value  = NULL;
    
    if ((NULL == module_id) || 
        (LE_SUCCESS != leda_string_validate_utf8(module_id, strlen(module_id))))
    {
        log_w(LEDA_TAG_NAME, "the driver is not deployed, check it please\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (NULL == config)
    {
        log_w(LEDA_TAG_NAME, "config is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }

    ret = _leda_get_config_info(INFO_TYPE_DEVICE_CONFIG, module_id, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        if (size < (strlen(value) + 1))
        {
            log_w(LEDA_TAG_NAME, "input param size: %d is less than real config size: %d\n", size, (strlen(value) + 1));
            free(value);
            return LE_ERROR_INVAILD_PARAM;
        }

        strncpy(config, value, size);
        free(value);
    }

    return ret;
}

static int _leda_get_driver_info(char **driver_info)
{
    int     ret     = LE_SUCCESS;
    char    *value  = NULL;

    cJSON   *root   = NULL;
    cJSON   *item   = NULL;

    char    *result = NULL;

    ret = _leda_get_config_info(INFO_TYPE_DEVICE_CONFIG, g_module_id, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        do
        {
            root = cJSON_Parse(value);
            if (NULL == root)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            item = cJSON_GetObjectItem(root, LEDA_DRIVER_CONFIG);
            if (NULL == item)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            result = cJSON_PrintUnformatted(item);
            if (NULL == result)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            *driver_info = result;
        } while (0);

        if (NULL != root)
        {
            cJSON_Delete(root);
        }
    }

    return ret;
}

/*
 * 获取驱动信息长度.
 *
 * 阻塞接口, 成功返回驱动信息长度, 失败返回0.
 */
int leda_get_driver_info_size(void)
{
    int     ret     = LE_SUCCESS;
    char    *result = NULL;

    ret = _leda_get_driver_info(&result);
    if ((LE_SUCCESS == ret) && (NULL != result))
    {
        ret = strlen(result) + 1;
        cJSON_free(result);
        return ret;
    }

    return 0;
}

/*
 * 获取驱动信息(在物联网平台配置的驱动配置).
 *
 * driver_info: 驱动信息, 需要提前申请好内存传入.
 * size:          驱动信息长度, leda_get_driver_info_size, 如果传入driver_info比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 * 
 */
int leda_get_driver_info(char *driver_info, int size)
{
    int     ret     = LE_SUCCESS;
    char    *result = NULL;

    if (NULL == driver_info)
    {
        log_w(LEDA_TAG_NAME, "driver_info is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }

    ret = _leda_get_driver_info(&result);
    if ((LE_SUCCESS == ret) && (NULL != result))
    {
        if (size < (strlen(result) + 1))
        {
            log_w(LEDA_TAG_NAME, "input param size: %d is less than real driver info size: %d\n", size, (strlen(result) + 1));
            cJSON_free(result);
            return LE_ERROR_INVAILD_PARAM;
        }
    }
    else
    {
        cJSON_free(result);
        return LE_ERROR_INVAILD_PARAM;
    }

    strncpy(driver_info, result, size);
    cJSON_free(result);

    return ret;
}

static int _leda_get_device_info(char **device_info)
{
    int     ret         = LE_SUCCESS;
    char    *value      = NULL;

    cJSON   *root       = NULL;
    cJSON   *list       = NULL;

    cJSON   *item       = NULL;

    cJSON   *custom     = NULL;
    cJSON   *custom_dup = NULL;

    char    *result     = NULL;

    ret = _leda_get_config_info(INFO_TYPE_DEVICE_CONFIG, g_module_id, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        do
        {
            root = cJSON_Parse(value);
            if (NULL == root)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            list = cJSON_GetObjectItem(root, LEDA_DEVICE_CONFIG_LIST);
            if (NULL == list)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            cJSON_ArrayForEach(item, list)
            {
                if (cJSON_Object == item->type)
                {
                    custom = cJSON_GetObjectItem(item, LEDA_DEVICE_CONFIG_CUSTOM);
                    if (NULL == custom)
                    {
                        continue;
                    }

                    custom_dup = cJSON_Duplicate(custom, cJSON_True);
                    cJSON_DeleteItemFromObject(item, LEDA_DEVICE_CONFIG_CUSTOM);
                    cJSON_AddRawToObject(item, LEDA_DEVICE_CONFIG_CUSTOM, custom_dup->valuestring);
                }
            }

            result = cJSON_PrintUnformatted(list);
            if (NULL == result)
            {
                ret = LE_ERROR_INVAILD_PARAM;
                break;
            }

            *device_info = result;
        } while (0);

        if (NULL != root)
        {
            cJSON_Delete(root);
        }
    }

    return ret;
}

/*
 * 获取设备信息长度.
 *
 * 阻塞接口, 成功返回设备信息长度, 失败返回0.
 */
int leda_get_device_info_size(void)
{
    int     ret         = LE_SUCCESS;
    char    *result     = NULL;

    ret = _leda_get_device_info(&result);
    if ((LE_SUCCESS == ret) && (NULL != result))
    {
        ret = strlen(result) + 1;
        cJSON_free(result);
        return ret;
    }

    return 0;
}

/*
 * 获取设备信息(在物联网平台配置的设备配置).
 *
 * device_info:  设备信息, 需要提前申请好内存传入.
 * size:         设备信息长度, 该长度通过leda_get_device_info_size接口获取, 如果传入device_info比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 * 
 */
int leda_get_device_info(char *device_info, int size)
{
    int     ret     = LE_SUCCESS;
    char    *result = NULL;

    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "device_info is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }

    ret = _leda_get_device_info(&result);
    if ((LE_SUCCESS == ret) && (NULL != result))
    {
        if (size < (strlen(result) + 1))
        {
            log_w(LEDA_TAG_NAME, "input param size: %d is less than real device info size: %d\n", size, (strlen(result) + 1));
            cJSON_free(result);
            return LE_ERROR_INVAILD_PARAM;
        }
    }
    else
    {
        cJSON_free(result);
        return LE_ERROR_INVAILD_PARAM;
    }

    strncpy(device_info, result, size);
    cJSON_free(result);

    return ret;
}

/*
 * 获取驱动所有配置（包括驱动信息和设备信息）长度.
 *
 * 阻塞接口, 成功返回素有驱动配置长度, 失败返回0.
 */
int leda_get_config_size(void)
{
    return _leda_get_config_size(g_module_id);
}

/*
 * 获取驱动所有配置（包括驱动信息和设备信息）内容.
 *
 * config:       驱动所有配置, 需要提前申请好内存传入.
 * size:         驱动所有配置长度, 该长度通过leda_get_config_size接口获取, 如果传入config比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_config(char *config, int size)
{
    return _leda_get_config(g_module_id, config, size);
}

/*
 * 订阅驱动配置变更监听回调.
 *
 * config_cb:      配置变更通知回调接口.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,失败返回错误码.
 */
int leda_register_config_changed_callback(config_changed_callback config_cb)
{
    int                 ret             = LE_SUCCESS;
    char                *driver_wkn     = NULL;
    char                *request_key    = NULL;
    int                 type            = 1;    /* 0:拥有者 1:观察者 */
    DBusMessage         *msg_call       = NULL;
    leda_retinfo_t      retinfo;

    if ((NULL == g_module_id) || 
        (LE_SUCCESS != leda_string_validate_utf8(g_module_id, strlen(g_module_id))))
    {
        log_w(LEDA_TAG_NAME, "the driver is not deployed, check it please\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (NULL == config_cb)
    {
        log_w(LEDA_TAG_NAME, "config_cb is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    request_key = malloc(strlen(g_module_id) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1);
    if (NULL == request_key)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }
    memset(request_key, 0, (strlen(g_module_id) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1));
    snprintf(request_key, (strlen(g_module_id) + strlen(CONFIGMANAGER_DEVICE_HEADER) + 1), "%s%s", CONFIGMANAGER_DEVICE_HEADER, g_module_id);
    
    driver_wkn = (char *)malloc(strlen(LEDA_DRIVER_WKN) + strlen(g_module_id) + 1);
    if (NULL == driver_wkn)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        free(request_key);
        return LE_ERROR_ALLOCATING_MEM;
    }
    memset(driver_wkn, 0, (strlen(LEDA_DRIVER_WKN) + strlen(g_module_id) + 1));
    snprintf(driver_wkn, strlen(LEDA_DRIVER_WKN) + strlen(g_module_id) + 1, "%s%s", LEDA_DRIVER_WKN, g_module_id);

    msg_call = _leda_create_methodcall(DMP_CONFIGMANAGER_WELL_KNOW_NAME, DMP_CONFIGMANAGER_METHOD_SUBSCRIBE);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        free(request_key);
        free(driver_wkn);
        return LE_ERROR_UNKNOWN;
    }

    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &driver_wkn, DBUS_TYPE_STRING, &request_key, DBUS_TYPE_INT32, &type, DBUS_TYPE_INVALID);
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    free(request_key);
    free(driver_wkn);
    if ((LE_SUCCESS != ret) || (LE_SUCCESS != retinfo.code))
    {
        log_w(LEDA_TAG_NAME, "register config change callback failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);

        return ret;
    }
    leda_retinfo_free(&retinfo);

    ret = leda_insert_device_configcb(g_module_id, config_cb);

    return ret;
}

/*
 * 获取指定产品ProductKey对应tsl(物模型)内容长度.
 *
 * product_key:   产品ProductKey.
 *
 * 阻塞接口, 成功返回product_key对应的tsl内容长度, 失败返回0.
 */
int leda_get_tsl_size(const char *product_key)
{
    int     ret     = 0;
    char    *value  = NULL;
    
    if ((NULL == product_key) || 
        (LE_SUCCESS != leda_string_validate_utf8(product_key, strlen(product_key))))
    {
        log_w(LEDA_TAG_NAME, "product_key: %s  is invalid\n", product_key);
        return 0;
    }

    ret = _leda_get_config_info(INFO_TYPE_PRODUCT_TSL, product_key, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        ret = strlen(value) + 1;
        free(value);

        return ret;
    }

    return 0;
}

/*
 * 获取指定产品ProductKey对应物模型内容.
 *
 * product_key:  产品ProductKey.
 * tsl:          物模型内容, 需要提前申请好内存传入.
 * size:         物模型内容长度, 该长度通过leda_get_tsl_size接口获取, 如果传入tsl比实际物模型内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_tsl(const char *product_key, char *tsl, int size)
{
    int     ret     = LE_SUCCESS;
    char    *value  = NULL;
    
    if ((NULL == product_key) || 
        (LE_SUCCESS != leda_string_validate_utf8(product_key, strlen(product_key))))
    {
        log_w(LEDA_TAG_NAME, "product_key: %s is invalid\n", product_key);
        return LE_ERROR_INVAILD_PARAM;
    }

    if (NULL == tsl)
    {
        log_w(LEDA_TAG_NAME, "tsl is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }

    ret = _leda_get_config_info(INFO_TYPE_PRODUCT_TSL, product_key, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        if (size < (strlen(value) + 1))
        {
            log_w(LEDA_TAG_NAME, "input param size: %d is less than real tsl size: %d\n", size, (strlen(value) + 1));
            free(value);
            return LE_ERROR_INVAILD_PARAM;
        }

        strncpy(tsl, value, size);
        free(value);
    }

    return ret;
}

/*
 * 获取指定产品ProductKey对应物模型扩展信息内容长度.
 *
 * product_key:   产品ProductKey.
 *
 * 阻塞接口, 成功返回product_key对应的物模型扩展信息内容长度, 失败返回0.
 */
int leda_get_tsl_ext_info_size(const char *product_key)
{
    int     ret     = 0;
    char    *value  = NULL;
    
    if ((NULL == product_key) || 
        (LE_SUCCESS != leda_string_validate_utf8(product_key, strlen(product_key))))
    {
        log_w(LEDA_TAG_NAME, "product_key: %s is invalid\n", product_key);
        return 0;
    }

    ret = _leda_get_config_info(INFO_TYPE_PRODUCT_TSL_CONFIG, product_key, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        ret = strlen(value) + 1;
        free(value);

        return ret;
    }

    return 0;
}

/*
 * 获取指定产品ProductKey对应物模型扩展信息内容.
 *
 * product_key:  产品ProductKey.
 * tsl_ext_info: 物模型配置内容, 需要提前申请好内存传入.
 * size:         物模型扩展信息长度, 该长度通过leda_get_tsl_ext_info_size接口获取, 如果传入tsl_ext_info比实际物模型扩展内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_tsl_ext_info(const char *product_key, char *tsl_ext_info, int size)
{
    int     ret     = LE_SUCCESS;
    char    *value  = NULL;
    
    if ((NULL == product_key) || 
        (LE_SUCCESS != leda_string_validate_utf8(product_key, strlen(product_key))))
    {
        log_w(LEDA_TAG_NAME, "product_key: %s is invalid\n", product_key);
        return LE_ERROR_INVAILD_PARAM;
    }

    if (NULL == tsl_ext_info)
    {
        log_w(LEDA_TAG_NAME, "tsl_ext_info is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }

    ret = _leda_get_config_info(INFO_TYPE_PRODUCT_TSL_CONFIG, product_key, &value);
    if ((LE_SUCCESS == ret) && (NULL != value))
    {
        if (size < (strlen(value) + 1))
        {
            log_w(LEDA_TAG_NAME, "input param size: %d is less than real tsl config size: %d\n", size, (strlen(value) + 1));
            free(value);
            return LE_ERROR_INVAILD_PARAM;
        }

        strncpy(tsl_ext_info, value, size);
        free(value);
    }

    return ret;
}

/*
 * 获取设备句柄.
 *
 * product_key: 产品ProductKey.
 * device_name: 设备名称.
 *
 * 阻塞接口, 成功返回device_handle_t, 失败返回小于0数值.
 */
device_handle_t leda_get_device_handle(const char *product_key, const char *device_name)
{
    leda_device_info_t *device_info = NULL;
    device_handle_t     dev_handle  = INVALID_DEVICE_HANDLE;

    if (NULL == product_key)
    {
        log_w(LEDA_TAG_NAME, "product_key is NULL\n");

        return INVALID_DEVICE_HANDLE;
    }

    if (NULL == device_name)
    {
        log_w(LEDA_TAG_NAME, "device_name is NULL\n");

        return INVALID_DEVICE_HANDLE;
    }

    device_info = leda_get_methodcb_by_dn_pk(product_key, device_name);
    if (NULL == device_info)
    {
        log_w(LEDA_TAG_NAME, "product_key: %s or device_name: %s is invalid\n", product_key, device_name);
        return INVALID_DEVICE_HANDLE;
    }

    dev_handle = device_info->dev_handle;

    return dev_handle;
}

int leda_inner_get_config_info_size(const char *key)
{
    int                 ret = LE_SUCCESS;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call = NULL;
    
    if ((NULL == key) || 
        (LE_SUCCESS != leda_string_validate_utf8(key, strlen(key))))
    {
        log_w(LEDA_TAG_NAME, "key: %s is invalid\n", key);
        return 0;
    }
    
    msg_call = _leda_create_methodcall(DMP_CONFIGMANAGER_WELL_KNOW_NAME, DMP_CONFIGMANAGER_METHOD_GET);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return 0;
    }
    
    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID);
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || ((LE_SUCCESS != retinfo.code) || (NULL == retinfo.params) || (!strcmp(retinfo.params, ""))))
    {
        log_w(LEDA_TAG_NAME, "get config failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);
        return 0;
    }

    if (LE_SUCCESS == ret)
    {
        ret = strlen(retinfo.params) + 1;
    }

    leda_retinfo_free(&retinfo);

    return ret;
}

int leda_inner_get_config_info(const char *key, char *info, int size)
{
    int                 ret = LE_SUCCESS;
    leda_retinfo_t      retinfo;
    DBusMessage         *msg_call = NULL;
    
    if ((NULL == key) || 
        (LE_SUCCESS != leda_string_validate_utf8(key, strlen(key))))
    {
        log_w(LEDA_TAG_NAME, "key: %s is invalid\n", key);
        return LE_ERROR_INVAILD_PARAM;
    }

    if (NULL == info)
    {
        log_w(LEDA_TAG_NAME, "info is NULL\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    if (0 >= size)
    {
        log_w(LEDA_TAG_NAME, "size: %d is invalid\n", size);
        return LE_ERROR_INVAILD_PARAM;
    }
    
    msg_call = _leda_create_methodcall(DMP_CONFIGMANAGER_WELL_KNOW_NAME, DMP_CONFIGMANAGER_METHOD_GET);
    if (NULL == msg_call)
    {
        log_w(LEDA_TAG_NAME, "create dbus method call failed\n");
        return LE_ERROR_UNKNOWN;
    }
    
    dbus_message_append_args(msg_call, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID);
    ret = _leda_send_with_replay(msg_call, 10000, &retinfo);
    dbus_message_unref(msg_call);
    if ((LE_SUCCESS != ret) || ((LE_SUCCESS != retinfo.code) || (NULL == retinfo.params) || (!strcmp(retinfo.params, ""))))
    {
        log_w(LEDA_TAG_NAME, "get config failed, ret: %d code: %d msg: %s\n", ret, retinfo.code, retinfo.message);
        leda_retinfo_free(&retinfo);
        return LE_ERROR_UNKNOWN;
    }

    if (size < (strlen(retinfo.params) + 1))
    {
        log_w(LEDA_TAG_NAME, "input param size: %d is less than real config size: %d\n", size, (strlen(retinfo.params) + 1));
        leda_retinfo_free(&retinfo);
        return LE_ERROR_INVAILD_PARAM;
    }

    strncpy(info, retinfo.params, size);
    leda_retinfo_free(&retinfo);

    return LE_SUCCESS;
}

/*
 * 喂看门狗.
 *
 * module_id:   驱动模块ID.
 * thread_name: 需要保活的线程名称.
 * count_down_seconds : 倒计时时间, -1表示停止保活, 单位:秒.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_feed_watchdog(const char *module_id, const char *thread_name, int count_down_seconds)
{
    DBusMessage *signal_msg = NULL;
    char        *path       = NULL; 
    char        *interface  = NULL;

    if ((NULL == module_id) 
        || (LE_SUCCESS != leda_string_validate_utf8(module_id, strlen(module_id))))
    {
        log_w(LEDA_TAG_NAME, "module_id: %s is invalid\n", module_id);
        return LE_ERROR_INVAILD_PARAM;
    }

    if ((NULL == thread_name)
        || (LE_SUCCESS != leda_string_validate_utf8(thread_name, strlen(thread_name))))
    {
        log_w(LEDA_TAG_NAME, "thread_name: %s is invalid\n", thread_name);
        return LE_ERROR_INVAILD_PARAM;
    }
    
    path = (char *)malloc(strlen(LEDA_DRIVER_WKN) + strlen(module_id) + 2);
    if (NULL == path)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    interface = (char *)malloc(strlen(LEDA_DRIVER_WKN) + strlen(module_id) + 1);
    if (NULL == interface)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        free(path);
        return LE_ERROR_ALLOCATING_MEM;
    }

    snprintf(interface, strlen(LEDA_DRIVER_WKN) + strlen(module_id) + 1, "%s%s", LEDA_DRIVER_WKN, module_id);
    leda_wkn_to_path(interface, path);
    signal_msg = dbus_message_new_signal(path, DMP_WATCHDOG_WELL_KNOWN_NAME, DMP_WATCHDOG_METHOD_FEEDDOG);
    if (NULL == signal_msg)
    {
        log_w(LEDA_TAG_NAME, "create dbus new signal failed\n");
        free(path);
        free(interface);
        return LE_ERROR_UNKNOWN;
    }

    dbus_message_append_args(signal_msg, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &thread_name, DBUS_TYPE_INT32, &count_down_seconds, DBUS_TYPE_INVALID);
    dbus_message_set_destination(signal_msg, DMP_WATCHDOG_WELL_KNOWN_NAME);
    if (TRUE != dbus_connection_send(g_connection, signal_msg, NULL))
    {
        log_w(LEDA_TAG_NAME, "dbus send failed\n");
        free(path);
        free(interface);
        dbus_message_unref(signal_msg);
        return LE_ERROR_UNKNOWN;
    }
    
    free(path);
    free(interface);
    dbus_message_unref(signal_msg);

    return LE_SUCCESS;
}

#ifdef __cplusplus  /* If this is a C++ compiler, use C linkage */
}
#endif
