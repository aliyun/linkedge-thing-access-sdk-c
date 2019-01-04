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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <cJSON.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_TAG_NAME            "demo_led"
#define MAX_DEVICE_NUM          32

static int g_dev_handle_list[MAX_DEVICE_NUM] = 
{
    INVALID_DEVICE_HANDLE
};
static int  g_dev_handle_count = 0;

static int get_properties_callback_cb(device_handle_t dev_handle, 
                               leda_device_data_t properties[], 
                               int properties_count, 
                               void *usr_data)
{
    int i = 0;
    for (i = 0; i < properties_count; i++)
    {
        log_i(LED_TAG_NAME, "get_property %s: ", properties[i].key);

        if (!strcmp(properties[i].key, "temperature"))
        {
            /* 作为演示，填写获取属性数据为模拟数据 */
            properties[i].type = LEDA_TYPE_INT;
            sprintf(properties[i].value, "%d", 30);

            log_i(LED_TAG_NAME, "%s\r\n",  properties[i].value);
        }
    }

    return LE_SUCCESS;
}

static int set_properties_callback_cb(device_handle_t dev_handle, 
                               const leda_device_data_t properties[], 
                               int properties_count, 
                               void *usr_data)
{
    int i = 0;
    for (i = 0; i < properties_count; i++)
    {
        /* 作为演示，仅打印出设置属性信息 */
        log_i(LED_TAG_NAME, "set_property type:%d %s: %s\r\n", properties[i].type, properties[i].key, properties[i].value);
    }

    return LE_SUCCESS;
}

static int call_service_callback_cb(device_handle_t dev_handle, 
                               const char *service_name, 
                               const leda_device_data_t data[], 
                               int data_count, 
                               leda_device_data_t output_data[], 
                               void *usr_data)
{
    int i = 0;

    /* service_name为该驱动物模型自定义方法名称 */
    log_i(LED_TAG_NAME, "service_name: %s\r\n", service_name);
    
    /* 获取service_name方法的参数名称和值信息 */
    for (i = 0; i < data_count; i++)
    {
        log_i(LED_TAG_NAME, "input_data %s: %s\r\n", data[i].key, data[i].value);
    }

    /* 此处错位演示并没有执行真正的自定义方法 */

    return LE_SUCCESS;
}

static int get_and_parse_deviceconfig(const char* module_name)
{
    int                         ret             = LE_SUCCESS;

    int                         size            = 0;
    char                        *device_config  = NULL;

    char                        *productKey     = NULL;
    char                        *deviceName     = NULL;

    cJSON                       *root           = NULL;
    cJSON                       *object         = NULL;
    cJSON                       *item           = NULL;
    cJSON                       *result         = NULL;

    leda_device_callback_t      device_cb;
    device_handle_t             dev_handle      = -1;

    /* get device config */
    size = leda_get_config_size(module_name);
    if (size >0)
    {
        device_config = (char*)malloc(size);
        if (NULL == device_config)
        {
            log_e(LED_TAG_NAME, "allocate memory failed\r\n");
            return LE_ERROR_INVAILD_PARAM;
        }

        if (LE_SUCCESS != (ret = leda_get_config(module_name, device_config, size)))
        {
            log_e(LED_TAG_NAME, "get device config failed\r\n");
            return ret;
        }
    }

    root = cJSON_Parse(device_config);
    if (NULL == root)
    {
        log_e(LED_TAG_NAME, "device config parser failed\r\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    object = cJSON_GetObjectItem(root, "deviceList");
    cJSON_ArrayForEach(item, object)
    {
        if (cJSON_Object == item->type)
        {
            /* parse device config */
            result = cJSON_GetObjectItem(item, "productKey");
            productKey = result->valuestring;
            
            result = cJSON_GetObjectItem(item, "deviceName");
            deviceName = result->valuestring;

            /* register and online device */
            device_cb.get_properties_cb            = get_properties_callback_cb;
            device_cb.set_properties_cb            = set_properties_callback_cb;
            device_cb.call_service_cb              = call_service_callback_cb;
            device_cb.service_output_max_count     = 5;

            dev_handle = leda_register_and_online_by_device_name(productKey, deviceName, &device_cb, NULL);
            if (dev_handle < 0)
            {
                log_e(LED_TAG_NAME, "product:%s device:%s register failed\r\n", productKey, deviceName);
                continue;
            }
            g_dev_handle_list[g_dev_handle_count++] = dev_handle;
            log_i(LED_TAG_NAME, "product:%s device:%s register success\r\n", productKey, deviceName);
        }
    }

    if (NULL != root)
    {
        cJSON_Delete(root);
    }

    free(device_config);

    return LE_SUCCESS;
}

static void *thread_device_data(void *arg)
{
    int i = 0;

    prctl(PR_SET_NAME, "demo_led_data");

    while (1)
    {
        for (i = 0; i < g_dev_handle_count; i++)
        {
            /* report device properties */
            leda_device_data_t dev_proper_data[1] = 
            {
                {
                    .type  = LEDA_TYPE_INT,
                    .key   = {"temperature"},
                    .value = {"20"}
                }
            };
            leda_report_properties(g_dev_handle_list[i], dev_proper_data, 1);

            /* report device event */
            leda_device_data_t dev_event_data[1] = 
            {
                {
                    .type  = LEDA_TYPE_INT,
                    .key   = {"temperature"},
                    .value = {"50"}
                }
            };
            leda_report_event(g_dev_handle_list[i], "high_temperature", dev_event_data, 1);
        }

        sleep(5);
    }

    pthread_exit(NULL);

    return NULL;
}

int main(int argc, char** argv)
{
    int         ret         = LE_SUCCESS;
    char*       module_name = NULL;
    pthread_t   thread_id;

    log_init(LED_TAG_NAME, LOG_STDOUT, LOG_LEVEL_DEBUG, LOG_MOD_BRIEF);

    log_i(LED_TAG_NAME, "demo startup\r\n");

    /* init driver */
    module_name = leda_get_module_name();
    if (NULL == module_name)
    {
        log_e(LED_TAG_NAME, "the driver no deploy or deploy failed\r\n");
        return LE_ERROR_UNKNOWN;
    }

    if (LE_SUCCESS != (ret = leda_init(module_name, 5)))
    {
        log_e(LED_TAG_NAME, "leda_init failed\r\n");
        return ret;
    }

    if (LE_SUCCESS != (ret = get_and_parse_deviceconfig(module_name)))
    {
        log_e(LED_TAG_NAME, "parse device config failed\r\n");
        return ret;
    }

    if (0 != pthread_create(&thread_id, NULL, thread_device_data, NULL))
    {
        log_e(LED_TAG_NAME, "create thread failed\r\n");
        return LE_ERROR_UNKNOWN;
    }

    prctl(PR_SET_NAME, "demo_led_main");
    while (1)
    {
        sleep(5);
    }

    /* exit driver */
    leda_exit();

    log_i(LED_TAG_NAME, "demo exit\r\n");

	return LE_SUCCESS;
}

#ifdef __cplusplus
}
#endif
