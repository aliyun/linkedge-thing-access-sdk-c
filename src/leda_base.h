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

#ifndef _LEDA_BASE_H_
#define _LEDA_BASE_H_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/* 下行执行结果解释 */
#define LE_SUCCESS_MSG                      "Ok"                                /* 请求成功*/
#define LE_ERROR_UNKNOWN_MSG                "Unknow error"                      /* 不能被识别的错误，用户不应该看到的错误*/
#define LE_ERROR_INVAILD_PARAM_MSG          "Invalid params"                    /* 传入参数为NULL或无效*/
#define LE_ERROR_ALLOCATING_MEM_MSG         "Alloc memery failed"               /* 指定的内存分配失败*/
#define LE_ERROR_CREATING_MUTEX_MSG         "Create mutex failed"               /* 创建mutex失败*/
#define LE_ERROR_WRITING_FILE_MSG           "Write file failed"                 /* 写入文件失败*/
#define LE_ERROR_READING_FILE_MSG           "Read file failed"                  /* 读取文件失败*/
#define LE_ERROR_TIMEOUT_MSG                "Tiemout"                           /* 超时*/
#define LE_ERROR_PARAM_RANGE_OVERFLOW_MSG   "Param range overflow"              /* 参数范围越界*/
#define LE_ERROR_SERVICE_UNREACHABLE_MSG    "Service unreachable"               /* 服务不可达*/
#define LE_ERROR_FILE_NOT_EXIST_MSG         "No file exist"                     /* 文件不存在*/

#define LEDA_ERROR_DEVICE_UNREGISTER_MSG    "Device not register"               /* 设备未注册*/ 
#define LEDA_ERROR_DEVICE_OFFLINE_MSG       "Device offline"                    /* 设备已下线*/
#define LEDA_ERROR_PROPERTY_NOT_EXIST_MSG   "Property no exist"                 /* 属性不存在*/
#define LEDA_ERROR_PROPERTY_READ_ONLY_MSG   "Property only support read"        /* 属性只读*/
#define LEDA_ERROR_PROPERTY_WRITE_ONLY_MSG  "Property only support write"       /* 属性只写*/
#define LEDA_ERROR_SERVICE_NOT_EXIST_MSG    "Service no exist"                  /* 服务不存在*/
#define LEDA_ERROR_SERVICE_INPUT_PARAM_MSG  "Service param invalid"             /* 服务的输入参数不正确错误码*/
#define LEDA_ERROR_INVALID_JSON_MSG         "Json formate invalid"              /* JSON格式错误*/
#define LEDA_ERROR_INVALID_TYPE_MSG         "Param type invalid"                /* 参数类型错误*/

/* 服务协议和接口 */
#define DMP_DIMU_WELL_KNOWN_NAME            "iot.dmp.dimu"
#define DMP_METHOD_REGISTER_DEVICE          "registerDevice"
#define DMP_METHOD_UNREGISTER_DEVICE        "unregisterDevice"
#define DMP_METHOD_DEVICE_OFFLINE           "shutdownDevice"
#define DMP_METHOD_DEVICE_ONLINE            "startupDevice"
#define DMP_METHOD_REGISTER_DRIVER          "registerDriver"
#define DMP_METHOD_UNREGISTER_DRIVER        "unregisterDriver"
#define DMP_METHOD_CONNECT                  "connect"
#define DMP_METHOD_DISCONNECT               "disconnect"

#define DMP_SUB_WELL_KNOWN_NAME             "iot.dmp.subscribe"
#define DMP_WATCHDOG_WELL_KNOWN_NAME        "iot.gateway.watchdog"
#define DMP_WATCHDOG_METHOD_FEEDDOG         "feedDog"

#define LEDA_DEVICE_WKN                     "iot.device.id"
#define LEDA_PATH_NAME                      "/iot/device/id"
#define LEDA_PROPERTY_CHANGED               "propertiesChanged"
#define DMP_METHOD_CALLMETHOD               "callServices"
#define DMP_METHOD_INTROSPECT               "Introspect"
#define LEDA_DEV_METHOD_SET_PROPERTIES      "set"
#define LEDA_DEV_METHOD_GET_PROPERTIES      "get"
#define DMP_METHOD_RESULT_NOTIFY            "connectResultNotify"

#define LEDA_DRV_METHOD_GET_DEV_LIST        "getDeviceList"
#define LEDA_DRIVER_WKN                     "iot.driver.id"
#define LEDA_DRIVER_PATH                    "/iot/driver/id"

#define DMP_CONFIGMANAGER_WELL_KNOW_NAME    "iot.dmp.configmanager"
#define DMP_CONFIGMANAGER_PATH              "iot/dmp/configmanager"
#define DMP_CONFIGMANAGER_METHOD_GET        "get_config"
#define DMP_CONFIGMANAGER_METHOD_SUBSCRIBE  "subscribe_config"
#define DMP_CONFIGMANAGER_METHOD_NOTIFY     "notify_config"

#define CONFIGMANAGER_DEVICE_HEADER         "gw_driverconfig_"
#define CONFIGMANAGER_TSL_HEADER            "gw_TSL_"
#define CONFIGMANAGER_TSL_CONFIG_HEADER     "gw_TSL_config_"

#define LEDA_DRIVER_CONFIG                  "config"
#define LEDA_DEVICE_CONFIG_LIST             "deviceList"
#define LEDA_DEVICE_CONFIG_CUSTOM           "custom"

/* 物模型信息链表 */
typedef struct leda_tsl
{
    struct list_head list_node;
    char *product_key;  /* 产品pk */
    char *tsl;          /* 物模型 */
}leda_tsl_t;

/* 方法调用返回数据 */
typedef struct leda_retinfo
{
    int code;           /* 消息编码和msg对应 */
    char *message;      /* 返回的执行状态 */
    char *params;       /* 返回的输出数据 */
}leda_retinfo_t;

int  leda_string_validate_utf8(const char *str, int len);
void leda_wkn_to_path(const char *wkn, char *path);
int  leda_interface_is_vaild(const char *bus_if);
int  leda_path_is_vaild(const char *path);

char *leda_retmsg_create(int ret, char *params);
void leda_retmsg_init(leda_retinfo_t *retinfo);
int  leda_retmsg_parse(DBusMessage *reply, const char *method_name, leda_retinfo_t *retinfo);
void leda_retmsg_free(char *msg);
void leda_retinfo_free(leda_retinfo_t *retinfo);

char *leda_mothedret_create(int code, const leda_device_data_t data[], int count);
char *leda_params_parse(const char *params, char *key);

char *leda_transform_data_struct_to_string(const leda_device_data_t data[], int count);
int  leda_transform_data_json_to_struct(const char* product_key,
                                        const char* service_name, 
                                        cJSON *object, 
                                        leda_device_data_t **dev_data);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
#endif
