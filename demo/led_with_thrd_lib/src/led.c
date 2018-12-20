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
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <cJSON.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "third.h"
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

static int  g_LedRunState      = 0;

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
            sprintf(properties[i].value, "%d", 30); /* 作为演示，填写模拟数据 */
            properties[i].type = LEDA_TYPE_INT;
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
 
    log_i(LED_TAG_NAME, "service_name: %s\r\n", service_name);
 
    for (i = 0; i < data_count; i++)
    {
        log_i(LED_TAG_NAME, "    input_data %s: %s\r\n", data[i].key, data[i].value);
    }

    output_data[0].type = LEDA_TYPE_INT;
    sprintf(output_data[0].key, "%s", "temperature");
    sprintf(output_data[0].value, "%d", 50); /* 作为演示，填写模拟数据 */

    return LE_SUCCESS;
}

static int parse_driverconfig_and_onlinedevice(const char* driver_config)
{
    char                        *productKey     = NULL;
    char                        *deviceName     = NULL;
    cJSON                       *root           = NULL;
    cJSON                       *object         = NULL;
    cJSON                       *item           = NULL;
    cJSON                       *result         = NULL;
    leda_device_callback_t      device_cb;
    device_handle_t             dev_handle      = -1;

    root = cJSON_Parse(driver_config);
    if (NULL == root)
    {
        log_e(LED_TAG_NAME, "driver_config parser failed\r\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    object = cJSON_GetObjectItem(root, "deviceList");
    cJSON_ArrayForEach(item, object)
    {
        if (cJSON_Object == item->type)
        {
            result = cJSON_GetObjectItem(item, "productKey");
            productKey = result->valuestring;
            
            result = cJSON_GetObjectItem(item, "deviceName");
            deviceName = result->valuestring;

            /*  parse user custom config 
                因为本demo主要介绍驱动开发流程和SDK接口的使用，所以并没有完整的实现设备和驱动的连接。实际应用场景中这部分是
                必须要实现的，要实现驱动和设备的连接，开发者可以通过在设备关联驱动配置流程时通过自定义配置来配置设备连接信息，
                操作流程可以参考如下链接
                https://help.aliyun.com/document_detail/85236.html?spm=a2c4g.11186623.6.583.64bd3265NR9Ouo
                
                如果用户在自定义配置中填写内容为
                { 
                    "ip" : "192.168.1.199",
                    "port" : 9999
                }

                则实际生成结果为
                "custom" : { 
                    "ip" : "192.168.1.199",
                    "port" : 9999
                }

                下面给出解析自定义配置的示例代码，具体如下
                cJSON          *custom = NULL;
                char           *ip     = NULL;
                unsigned short port    = 65536;

                custom = cJSON_GetObjectItem(item, "custom");
                result = cJSON_GetObjectItem(custom, "ip");
                ip = result->valuestring;
                
                result = cJSON_GetObjectItem(custom, "port");
                port = (unsigned short)result->valueint;

                获取到设备ip和port后即可连接该设备，连接成功则注册上线设备，否则继续下一个配置的解析
            */

            /* register and online device */
            device_cb.get_properties_cb            = get_properties_callback_cb;
            device_cb.set_properties_cb            = set_properties_callback_cb;
            device_cb.call_service_cb              = call_service_callback_cb;
            device_cb.service_output_max_count     = 5;

            dev_handle = leda_register_and_online_by_local_name(productKey, deviceName, &device_cb, NULL);
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

    if (0 == g_dev_handle_count)
    {
        log_e(LED_TAG_NAME, "current driver no device online\r\n");
    }

    return LE_SUCCESS;
}

static void *thread_device_data(void *arg)
{
    int i = 0;

    prctl(PR_SET_NAME, "demo_led_data");
    while (0 == g_LedRunState)
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

static void ledSigInt(int sig)
{
    if (sig && (SIGINT == sig))
    {
        if (0 == g_LedRunState)
        {
            log_e(LED_TAG_NAME, "Caught signal: %s, exiting...\r\n", strsignal(sig));
        }

        g_LedRunState = -1;
    }

    return;
}

int main(int argc, char** argv)
{
    int         size            = 0;
    char        *driver_config  = NULL;
    pthread_t   thread_id;
	struct      sigaction sig_int;

    /* listen SIGINT singal */
	memset(&sig_int, 0, sizeof(struct sigaction));
    sigemptyset(&sig_int.sa_mask);
    sig_int.sa_handler = ledSigInt;
    sigaction(SIGINT, &sig_int, NULL);

    /* set log level */
    log_init(LED_TAG_NAME, LOG_FILE, LOG_LEVEL_DEBUG, LOG_MOD_VERBOSE);

    log_i(LED_TAG_NAME, "demo startup\r\n");

    log_i(LED_TAG_NAME, "the reference name of third lib is %s\r\n", get_third_lib_name());

    /* init sdk */
    if (LE_SUCCESS != leda_init(leda_get_module_name(), 5))
    {
        log_e(LED_TAG_NAME, "leda_init failed\r\n");
        return LE_ERROR_UNKNOWN;
    }

    /* take driver config from config center */
    size = leda_get_config_size(leda_get_module_name());
    if (size >0)
    {
        driver_config = (char*)malloc(size);
        if (NULL == driver_config)
        {
            return LE_ERROR_ALLOCATING_MEM;
        }

        if (LE_SUCCESS != leda_get_config(leda_get_module_name(), driver_config, size))
        {
            return LE_ERROR_INVAILD_PARAM;
        }
    }

    /* parse driver config and online device */
    if (LE_SUCCESS != parse_driverconfig_and_onlinedevice(driver_config))
    {
        log_e(LED_TAG_NAME, "parse driver config or online device failed\r\n");
        return LE_ERROR_UNKNOWN;
    }
    free(driver_config);

    /* report device data */
    if (0 != pthread_create(&thread_id, NULL, thread_device_data, NULL))
    {
        log_e(LED_TAG_NAME, "create device data thread failed\r\n");
        return LE_ERROR_UNKNOWN;
    }

    /* keep driver running */
    prctl(PR_SET_NAME, "demo_led_main");
    while (0 == g_LedRunState)
    {
        sleep(5);
    }

    /* exit sdk */
    leda_exit();
    log_i(LED_TAG_NAME, "demo exit\r\n");

	return LE_SUCCESS;
}

#ifdef __cplusplus
}
#endif
