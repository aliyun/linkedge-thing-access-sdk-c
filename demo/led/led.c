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
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <cJSON.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG_DEMO_LED            "demo_led"

static int g_dev_handle         = INVALID_DEVICE_HANDLE;
static int g_dev_temperature    = 0;

static int get_properties_callback_cb(device_handle_t dev_handle, 
                               leda_device_data_t properties[], 
                               int properties_count, 
                               void *usr_data)
{
    int i = 0;
    for (i = 0; i < properties_count; i++)
    {
        log_i(TAG_DEMO_LED, "get_property %s: ", properties[i].key);

        if (!strcmp(properties[i].key, "temperature"))
        {
            /* as a demonstration, fill in simulated data to get properities call */
            properties[i].type = LEDA_TYPE_INT;
            sprintf(properties[i].value, "%d", g_dev_temperature);

            log_i(TAG_DEMO_LED, "%s\n",  properties[i].value);
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
        /* as a demonstration, only print requst conetent to set properties call */
        log_i(TAG_DEMO_LED, "set_property type:%d %s: %s\n", properties[i].type, properties[i].key, properties[i].value);

        if (!strcmp(properties[i].key, "temperature"))
        {
            log_i(TAG_DEMO_LED, "set temperature from current %d to new %d\n", g_dev_temperature, atoi(properties[i].value));
            g_dev_temperature = atoi(properties[i].value);
            
        } 
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

    /* as a demonstration, only print requst conetent to custom call */
    log_i(TAG_DEMO_LED, "service_name: %s\n", service_name);
    
    for (i = 0; i < data_count; i++)
    {
        log_i(TAG_DEMO_LED, "service input_data %s: %s\n", data[i].key, data[i].value);
    }

    return LE_SUCCESS;
}

static int online_devices()
{
    int                         ret             = LE_SUCCESS;

    int                         size            = 0;
    char                        *device_config  = NULL;

    char                        *productKey     = NULL;
    char                        *deviceName     = NULL;

    cJSON                       *devices           = NULL;
    cJSON                       *item           = NULL;
    cJSON                       *result         = NULL;

    leda_device_callback_t      device_cb;
    device_handle_t             dev_handle      = -1;

    /* get device config */
    size = leda_get_device_info_size();
    if (size >0)
    {
        device_config = (char*)malloc(size);
        if (NULL == device_config)
        {
            log_e(TAG_DEMO_LED, "allocate memory failed\n");
            return LE_ERROR_INVAILD_PARAM;
        }

        if (LE_SUCCESS != (ret = leda_get_device_info(device_config, size)))
        {
            log_e(TAG_DEMO_LED, "get device config failed\n");
            return ret;
        }
    }

    /* parse device config */
    devices = cJSON_Parse(device_config);
    if (NULL == devices)
    {
        log_e(TAG_DEMO_LED, "device config parser failed\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    cJSON_ArrayForEach(item, devices)
    {
        if (cJSON_Object == item->type)
        {
            /* parse config element */
            result      = cJSON_GetObjectItem(item, "productKey");
            productKey  = result->valuestring;
            
            result      = cJSON_GetObjectItem(item, "deviceName");
            deviceName  = result->valuestring;

            result      = cJSON_GetObjectItem(item, "custom");
            if (NULL != result)
            {
                log_i(TAG_DEMO_LED, "custom content: %s\n", cJSON_PrintUnformatted(result));
            }

            /* online device */
            device_cb.get_properties_cb            = get_properties_callback_cb;
            device_cb.set_properties_cb            = set_properties_callback_cb;
            device_cb.call_service_cb              = call_service_callback_cb;
            device_cb.service_output_max_count     = 5;

            dev_handle = leda_register_and_online_by_device_name(productKey, deviceName, &device_cb, NULL);
            if (dev_handle < 0)
            {
                log_e(TAG_DEMO_LED, "product:%s device:%s register failed\n", productKey, deviceName);
                continue;
            }

            g_dev_handle = dev_handle;
            log_i(TAG_DEMO_LED, "product:%s device:%s register success\n", productKey, deviceName);
        }
    }

    if (NULL != devices)
    {
        cJSON_Delete(devices);
    }

    free(device_config);

    return LE_SUCCESS;
}

int main(int argc, char** argv)
{
    int    ret = LE_SUCCESS;

    log_init(TAG_DEMO_LED, LOG_STDOUT, LOG_LEVEL_DEBUG, LOG_MOD_BRIEF);

    /* init leda*/
    if (LE_SUCCESS != (ret = leda_init(5)))
    {
        log_e(TAG_DEMO_LED, "leda_init failed\n");
        return ret;
    }

    /* online devices */
    if (LE_SUCCESS != (ret = online_devices()))
    {
        log_e(TAG_DEMO_LED, "online devices failed\n");
        return ret;
    }

    /* report device data */
    while (1)
    {
        /* report device properties */
        leda_device_data_t dev_proper_data[1] = 
        {
            {
                .type  = LEDA_TYPE_INT,
                .key   = {"temperature"},
                .value = {0}
            }
        };
        sprintf(dev_proper_data[0].value, "%d", g_dev_temperature);
        leda_report_properties(g_dev_handle, dev_proper_data, 1);

        /* report device events */
        if (g_dev_temperature > 50)
        {
            leda_device_data_t dev_event_data[1] = 
            {
                {
                    .type  = LEDA_TYPE_INT,
                    .key   = {"temperature"},
                    .value = {0}
                }
            };
            sprintf(dev_event_data[0].value, "%d", g_dev_temperature);
            leda_report_event(g_dev_handle, "high_temperature", dev_event_data, 1);
        }

        sleep(5);
    }

    /* exit leda */
    leda_exit();

    return LE_SUCCESS;
}

#ifdef __cplusplus
}
#endif
