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
#include <stdint.h>
#include <cJSON.h>
#include <dbus/dbus.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"
#include "linux-list.h"
#include "leda_base.h"

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

static int _leda_get_itemtype_from_tsl_serviecs(const char* product_key, const char* service_name, const char* obj_name)
{
    int     ret             = LEDA_TYPE_BUTT;
    cJSON*  object          = NULL;
    cJSON*  services        = NULL;
    cJSON*  service_item    = NULL;
    cJSON*  input_item      = NULL;
    cJSON*  sub_item        = NULL;
    cJSON*  type_obj        = NULL;

    int     tsl_size        = 0;
    char*   tsl             = NULL;

    tsl_size = leda_get_tsl_size(product_key);
    if (tsl_size <=0)
    {
        goto END;
    }

    tsl = (char*)malloc(tsl_size);
    if (NULL == tsl)
    {
        goto END;
    }

    if (LE_SUCCESS != leda_get_tsl(product_key, tsl, tsl_size))
    {
        goto END;
    }

    object = cJSON_Parse(tsl);
    if (NULL == object)
    {
        goto END;
    }

    services = cJSON_GetObjectItem(object, "services");
    if (NULL == services)
    {
        goto END;
    }

    cJSON_ArrayForEach(service_item, services)
    {
        if (cJSON_Object == service_item->type)
        {
            if (strcmp(service_name, cJSON_GetObjectItem(service_item, "identifier")->valuestring))
            {
                continue;
            }

            input_item = cJSON_GetObjectItem(service_item, "inputData");
            cJSON_ArrayForEach(sub_item, input_item)
            {
                if (strcmp(obj_name, cJSON_GetObjectItem(sub_item, "identifier")->valuestring)) 
                {
                    continue;
                }

                type_obj = cJSON_GetObjectItem(cJSON_GetObjectItem(sub_item, "dataType"), "type");
                if (!strcmp(type_obj->valuestring, "int"))
                {
                    ret = LEDA_TYPE_INT;
                }
                else if (!strcmp(type_obj->valuestring, "bool"))
                {
                    ret = LEDA_TYPE_BOOL;
                }
                else if (!strcmp(type_obj->valuestring, "float"))
                {
                    ret = LEDA_TYPE_FLOAT;
                }
                else if (!strcmp(type_obj->valuestring, "date"))
                {
                    ret = LEDA_TYPE_DATE;
                }
                else if (!strcmp(type_obj->valuestring, "enum"))
                {
                    ret = LEDA_TYPE_ENUM;
                }
                else if (!strcmp(type_obj->valuestring, "double"))
                {
                    ret = LEDA_TYPE_DOUBLE;
                }
                else if (!strcmp(type_obj->valuestring, "text"))
                {
                    ret = LEDA_TYPE_TEXT;
                }
                
                goto END;
            }
        }
    }

END:

    if (NULL != object)
    {
        cJSON_Delete(object);
    }

    if (NULL != tsl)
    {
        free(tsl);
    }

    return ret;
}

#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
     (((Char) & 0xFFFFF800) != 0xD800))
#define UTF8_GET(Result, Chars, Count, Mask, Len)   \
    (Result) = (Chars)[0] & (Mask);                 \
    for((Count) = 1; (Count) < (Len); ++(Count))    \
    {									            \
        if (((Chars)[(Count)] & 0xc0) != 0x80)     \
	    {                                           \
	        (Result) = -1;                          \
	        break;							        \
	    }                                           \
        (Result) <<= 6;                             \
        (Result) |= ((Chars)[(Count)] & 0x3f);      \
    }
#define UTF8_COMPUTE(Char, Mask, Len)         \
    if (Char < 128)                           \
    {                                         \
      Len = 1;                                \
      Mask = 0x7f;                            \
    }                                         \
    else if ((Char & 0xe0) == 0xc0)           \
    {                                         \
      Len = 2;                                \
      Mask = 0x1f;                            \
    }                                         \
    else if ((Char & 0xf0) == 0xe0)           \
    {                                         \
      Len = 3;                                \
      Mask = 0x0f;                            \
    }          	                              \
    else if ((Char & 0xf8) == 0xf0)           \
    {									      \
      Len = 4;                                \
      Mask = 0x07;                            \
    }									      \
    else if ((Char & 0xfc) == 0xf8)           \
    {									      \
      Len = 5;                                \
      Mask = 0x03;                            \
    }									      \
    else if ((Char & 0xfe) == 0xfc)           \
    {									      \
      Len = 6;                                \
      Mask = 0x01;                            \
    }									      \
    else                                      \
    {                                         \
      Len = 0;                                \
      Mask = 0;                               \
    }
#define UTF8_LENGTH(Char)              \
  ((Char) < 0x80 ? 1 :                 \
   ((Char) < 0x800 ? 2 :               \
    ((Char) < 0x10000 ? 3 :            \
     ((Char) < 0x200000 ? 4 :          \
      ((Char) < 0x4000000 ? 5 : 6)))))

int leda_string_validate_utf8(const char *str, int len)
{
    unsigned char *p;
    unsigned char *end;

    p = (unsigned char*)str;
    end = p + len;

    while(p < end)
    {
        int i, mask, char_len;
        uint32_t result;

        /* nul bytes considered invalid */
        if (*p == '\0')
        {
            break;
        }
        if(*p < 128)
        {
            ++p;
            continue;
        }

        UTF8_COMPUTE (*p, mask, char_len);

        if(char_len == 0)  /* ASCII: char_len == 1 */
        {
            break;
        }

        if((end - p) < char_len)
        {
            break;
        }
        UTF8_GET (result, p, i, mask, char_len);

        if(UTF8_LENGTH (result) != char_len) /* ASCII: UTF8_LENGTH == 1 */
        {
            break;
        }
        if(!UNICODE_VALID (result)) /* ASCII: always valid */
        {
            break;
        }

        p += char_len;
    }
    if(p != end)
    {
        return LE_ERROR_INVAILD_PARAM;
    }
    else
    {
        return LE_SUCCESS;
    }
}

void leda_wkn_to_path(const char *wkn, char *path)
{
    int i;
    int len = strlen(wkn);

    *path = '/';
    for(i=0; i<len; i++)
    {
        if('.' == *(wkn+i))
        {
            *(path+i+1) = '/';
        }
        else
        {
            *(path+i+1) = *(wkn+i);
        }
    }
    *(path+i+1) = '\0';
}

int leda_interface_is_vaild(const char *bus_if)
{
    char *leda_wkn = LEDA_DEVICE_WKN;

    if(NULL == bus_if)
    {
        return LE_ERROR_INVAILD_PARAM;
    }
    
    if(strncmp(leda_wkn, bus_if, strlen(leda_wkn)))
    {
        return LE_ERROR_INVAILD_PARAM;
    }

    return LE_SUCCESS;
}

int leda_path_is_vaild(const char *path)
{
    char *leda_path = LEDA_PATH_NAME;

    if(NULL == path)
    {
        return LE_ERROR_INVAILD_PARAM;
    }
    
    if(strncmp(leda_path, path, strlen(leda_path)))
    {
        return LE_ERROR_INVAILD_PARAM;
    }

    return LE_SUCCESS;
}

char *leda_retmsg_create(int ret, char *params)
{
    cJSON *object, *item;
    char *info;

    object = cJSON_CreateObject();
    if(NULL == object)
    {
        return NULL;
    }

    switch (ret)
    {
    case LE_SUCCESS:
        {
            cJSON_AddNumberToObject(object, "code", LE_SUCCESS);
            cJSON_AddStringToObject(object, "message", LE_SUCCESS_MSG);
            break;
        }
    case LE_ERROR_INVAILD_PARAM:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_INVAILD_PARAM);
            cJSON_AddStringToObject(object, "message", LE_ERROR_INVAILD_PARAM_MSG);
            break;
        }
    case LE_ERROR_ALLOCATING_MEM:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_ALLOCATING_MEM);
            cJSON_AddStringToObject(object, "message", LE_ERROR_ALLOCATING_MEM_MSG);
            break;
        }
    case LE_ERROR_CREATING_MUTEX:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_CREATING_MUTEX);
            cJSON_AddStringToObject(object, "message", LE_ERROR_CREATING_MUTEX_MSG);
            break;
        }
    case LE_ERROR_WRITING_FILE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_WRITING_FILE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_WRITING_FILE_MSG);
            break;
        }
    case LE_ERROR_READING_FILE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_READING_FILE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_READING_FILE_MSG);
            break;
        }
    case LE_ERROR_TIMEOUT:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_TIMEOUT);
            cJSON_AddStringToObject(object, "message", LE_ERROR_TIMEOUT_MSG);
            break;
        }
    case LE_ERROR_PARAM_RANGE_OVERFLOW:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_PARAM_RANGE_OVERFLOW);
            cJSON_AddStringToObject(object, "message", LE_ERROR_PARAM_RANGE_OVERFLOW_MSG);
            break;
        }
    case LE_ERROR_SERVICE_UNREACHABLE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_SERVICE_UNREACHABLE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_SERVICE_UNREACHABLE_MSG);
            break;
        }
    case LE_ERROR_FILE_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_FILE_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LE_ERROR_FILE_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_DEVICE_UNREGISTER:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_DEVICE_UNREGISTER);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_DEVICE_UNREGISTER_MSG);
            break;
        }
    case LEDA_ERROR_DEVICE_OFFLINE:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_DEVICE_OFFLINE);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_DEVICE_OFFLINE_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_READ_ONLY:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_READ_ONLY);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_READ_ONLY_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_WRITE_ONLY:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_WRITE_ONLY);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_WRITE_ONLY_MSG);
            break;
        }
    case LEDA_ERROR_SERVICE_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_SERVICE_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_SERVICE_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_SERVICE_INPUT_PARAM:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_SERVICE_INPUT_PARAM);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_SERVICE_INPUT_PARAM_MSG);
            break;
        }
    case LEDA_ERROR_INVALID_JSON:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_INVALID_JSON);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_INVALID_JSON_MSG);
            break;
        }
    case LEDA_ERROR_INVALID_TYPE:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_INVALID_TYPE);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_INVALID_TYPE_MSG);
            break;
        }
    default:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_UNKNOWN);
            cJSON_AddStringToObject(object, "message", LE_ERROR_UNKNOWN_MSG);
            break;
        }
    }

    if((NULL != params) && (LE_SUCCESS == leda_string_validate_utf8(params, strlen(params))))
    {
        item = cJSON_Parse(params);
        if(NULL != item)
        {
            cJSON_AddItemToObject(object, "params", item);
        }
        else
        {
            log_e(LEDA_TAG_NAME, "the downstream data %s is not object\r\n", params);
            cJSON_AddItemToObject(object, "params", cJSON_CreateObject());
        }
    }
    else
    {
        cJSON_AddItemToObject(object, "params", cJSON_CreateObject());
    }

    info = cJSON_Print(object);
    cJSON_Delete(object);
    return info;
}

void leda_retmsg_init(leda_retinfo_t *retinfo)
{
    retinfo->code = 0;
    retinfo->message = NULL;
    retinfo->params = NULL;
}

int leda_retmsg_parse(DBusMessage *reply, const char *method_name, leda_retinfo_t *retinfo)
{
    cJSON *root, *item;
    DBusError dbus_error;
    char *info = NULL;
    char *buff = NULL;
    int code = 0;
    
    dbus_error_init(&dbus_error);
    if(!strcmp(method_name, DMP_CONFIGMANAGER_METHOD_GET))
    {
        if(!dbus_message_get_args(reply, &dbus_error, DBUS_TYPE_INT32, &code, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID))
        {
            log_e(LEDA_TAG_NAME, "get_args err:%s\n", info);
            return LE_ERROR_INVAILD_PARAM;
        }
        retinfo->code = code;
        retinfo->message = NULL;
        retinfo->params = (char *)malloc(strlen(info)+1);
        if(NULL == retinfo->params)
        {
            log_e(LEDA_TAG_NAME, "no memory\r\n");
            return LE_ERROR_INVAILD_PARAM;
        }
        snprintf(retinfo->params, (strlen(info)+1), "%s", info);
    }
    else if(!strcmp(method_name, DMP_CONFIGMANAGER_METHOD_SUBSCRIBE))
    {
        if(!dbus_message_get_args(reply, &dbus_error, DBUS_TYPE_INT32, &code, DBUS_TYPE_INVALID))
        {
            log_e(LEDA_TAG_NAME, "get_args err:%s\r\n", info);
            return LE_ERROR_INVAILD_PARAM;
        }
        retinfo->code = code;
        retinfo->message = NULL;
        retinfo->params = NULL;        
    }
    else
    {
        if(!dbus_message_get_args(reply, &dbus_error, DBUS_TYPE_STRING, &info, DBUS_TYPE_INVALID))
        {
            log_e(LEDA_TAG_NAME, "get_args err:%s\r\n", info);
            return LE_ERROR_INVAILD_PARAM;
        }
        root = cJSON_Parse(info);
        if(NULL == root)
        {
            log_e(LEDA_TAG_NAME, "result err:%s\r\n", info);
            return LEDA_ERROR_INVALID_JSON;
        }
        item = cJSON_GetObjectItem(root, "code");
        if(NULL == item)
        {
            log_e(LEDA_TAG_NAME, "result err:%s\r\n", info);
            cJSON_Delete(root);
            return LEDA_ERROR_INVALID_JSON;
        }
        retinfo->code = item->valueint;
        
        item = cJSON_GetObjectItem(root, "message");
        if((NULL == item) || (NULL == item->valuestring))
        {
            log_e(LEDA_TAG_NAME, "result err:%s\r\n", info);
            cJSON_Delete(root);
            return LEDA_ERROR_INVALID_JSON;
        }
        retinfo->message = (char *)malloc(strlen(item->valuestring)+1);
        if(NULL == retinfo->message)
        {
            log_e(LEDA_TAG_NAME, "no memory\r\n");
            cJSON_Delete(root);
            return LE_ERROR_ALLOCATING_MEM;
        }
        snprintf(retinfo->message, (strlen(item->valuestring)+1), "%s", item->valuestring);

        item = cJSON_GetObjectItem(root, "params");
        if(NULL != item)
        {
            if(cJSON_String == item->type)
            {
                buff = item->valuestring;
                retinfo->params = (char *)malloc(strlen(buff)+1);
                if(NULL == retinfo->params)
                {
                    log_e(LEDA_TAG_NAME, "no memory\r\n");
                    free(retinfo->message);
                    cJSON_Delete(root);
                    return LE_ERROR_ALLOCATING_MEM;
                }
                snprintf(retinfo->params, (strlen(buff)+1), "%s", buff);
            }
            else
            {
                buff = cJSON_Print(item);
                if(NULL == buff)
                {
                    log_e(LEDA_TAG_NAME, "no memory\r\n");
                    free(retinfo->message);
                    cJSON_Delete(root);
                    return LE_ERROR_ALLOCATING_MEM;
                }
                retinfo->params = (char *)malloc(strlen(buff)+1);
                if(NULL == retinfo->params)
                {
                    log_e(LEDA_TAG_NAME, "no memory\r\n");
                    free(retinfo->message);
                    cJSON_free(buff);
                    cJSON_Delete(root);
                    return LE_ERROR_ALLOCATING_MEM;
                }
                snprintf(retinfo->params, (strlen(buff)+1), "%s", buff);
                cJSON_free(buff);
            }
        } 
        else
        {
            retinfo->params = NULL;
        }
        cJSON_Delete(root);
    }
    dbus_error_free(&dbus_error);

    return LE_SUCCESS;
}

void leda_retmsg_free(char *msg)
{
    if(msg)
    {
        cJSON_free(msg);
    }
}

void leda_retinfo_free(leda_retinfo_t *retinfo)
{
    if(retinfo->message)
    {
        free(retinfo->message);
    }
    if(retinfo->params)
    {
        free(retinfo->params);
    }
}

char *leda_mothedret_create(int code, const leda_device_data_t data[], int count)
{
    cJSON *object, *item;
    char *params = NULL;
    char *info;
    int i = 0;
    int num = 0;

    object = cJSON_CreateObject();
    if(NULL == object)
    {
        free(params);
        return NULL;
    }

    switch (code)
    {
    case LE_SUCCESS:
        {
            cJSON_AddNumberToObject(object, "code", LE_SUCCESS);
            cJSON_AddStringToObject(object, "message", LE_SUCCESS_MSG);
            break;
        }
    case LE_ERROR_INVAILD_PARAM:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_INVAILD_PARAM);
            cJSON_AddStringToObject(object, "message", LE_ERROR_INVAILD_PARAM_MSG);
            break;
        }
    case LE_ERROR_ALLOCATING_MEM:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_ALLOCATING_MEM);
            cJSON_AddStringToObject(object, "message", LE_ERROR_ALLOCATING_MEM_MSG);
            break;
        }
    case LE_ERROR_CREATING_MUTEX:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_CREATING_MUTEX);
            cJSON_AddStringToObject(object, "message", LE_ERROR_CREATING_MUTEX_MSG);
            break;
        }
    case LE_ERROR_WRITING_FILE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_WRITING_FILE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_WRITING_FILE_MSG);
            break;
        }
    case LE_ERROR_READING_FILE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_READING_FILE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_READING_FILE_MSG);
            break;
        }
    case LE_ERROR_TIMEOUT:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_TIMEOUT);
            cJSON_AddStringToObject(object, "message", LE_ERROR_TIMEOUT_MSG);
            break;
        }
    case LE_ERROR_PARAM_RANGE_OVERFLOW:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_PARAM_RANGE_OVERFLOW);
            cJSON_AddStringToObject(object, "message", LE_ERROR_PARAM_RANGE_OVERFLOW_MSG);
            break;
        }
    case LE_ERROR_SERVICE_UNREACHABLE:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_SERVICE_UNREACHABLE);
            cJSON_AddStringToObject(object, "message", LE_ERROR_SERVICE_UNREACHABLE_MSG);
            break;
        }
    case LE_ERROR_FILE_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_FILE_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LE_ERROR_FILE_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_DEVICE_UNREGISTER:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_DEVICE_UNREGISTER);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_DEVICE_UNREGISTER_MSG);
            break;
        }
    case LEDA_ERROR_DEVICE_OFFLINE:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_DEVICE_OFFLINE);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_DEVICE_OFFLINE_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_READ_ONLY:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_READ_ONLY);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_READ_ONLY_MSG);
            break;
        }
    case LEDA_ERROR_PROPERTY_WRITE_ONLY:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_PROPERTY_WRITE_ONLY);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_PROPERTY_WRITE_ONLY_MSG);
            break;
        }
    case LEDA_ERROR_SERVICE_NOT_EXIST:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_SERVICE_NOT_EXIST);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_SERVICE_NOT_EXIST_MSG);
            break;
        }
    case LEDA_ERROR_SERVICE_INPUT_PARAM:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_SERVICE_INPUT_PARAM);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_SERVICE_INPUT_PARAM_MSG);
            break;
        }
    case LEDA_ERROR_INVALID_JSON:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_INVALID_JSON);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_INVALID_JSON_MSG);
            break;
        }
    case LEDA_ERROR_INVALID_TYPE:
        {
            cJSON_AddNumberToObject(object, "code", LEDA_ERROR_INVALID_TYPE);
            cJSON_AddStringToObject(object, "message", LEDA_ERROR_INVALID_TYPE_MSG);
            break;
        }
    default:
        {
            cJSON_AddNumberToObject(object, "code", LE_ERROR_UNKNOWN);
            cJSON_AddStringToObject(object, "message", LE_ERROR_UNKNOWN_MSG);
            break;
        }
    }

    if ((NULL != data) && (0 != count))
    {
        for (i = 0; i < count; i++)
        {
            if (LEDA_TYPE_BUTT == data[i].type)
            {
                break;
            }

            num++;
        }

        if (0 != num)
        {
            params = leda_transform_data_struct_to_string(data, num);
        }
    }

    if((NULL != params) 
        && (LE_SUCCESS == leda_string_validate_utf8(params, strlen(params))))
    {
        item = cJSON_Parse(params);
        if(NULL != item)
        {
            cJSON_AddItemToObject(object, "data", item);
        }
        else
        {
            log_e(LEDA_TAG_NAME, "the method downstream data %s is not object\r\n", params);
            cJSON_AddItemToObject(object, "data", cJSON_CreateObject());
        }
    }
    else
    {
        cJSON_AddItemToObject(object, "data", cJSON_CreateObject());
    }

    info = cJSON_Print(object);
    cJSON_Delete(object);
    if(NULL != params)
    {
        free(params);
    }
    return info;
}

char *leda_params_parse(const char *params, char *key)
{
    char *value;
    cJSON *root, *item;

    root = cJSON_Parse(params);    
    item = cJSON_GetObjectItem(root, key);
    if(NULL == item)
    {
        log_e(LEDA_TAG_NAME, "result err:%s\r\n", params);
        cJSON_Delete(root);
        return NULL;
    }
    value = (char *)malloc(strlen(item->valuestring)+1);
    if(NULL == value)
    {
        log_e(LEDA_TAG_NAME, "no memory\r\n");
        cJSON_Delete(root);
        return NULL;
    }
    snprintf(value, (strlen(item->valuestring)+1), "%s", item->valuestring);
    cJSON_Delete(root);
    return value;
}

char *leda_transform_data_struct_to_string(const leda_device_data_t data[], int count)
{
    int     i       = 0;
    char*   output  = NULL;
    cJSON*  object  = NULL;
    
    object = cJSON_CreateObject();
    if (NULL == object)
    {
        goto END;
    }

    for (i = 0; i < count; i++)
    {
        if ((data[i].type == LEDA_TYPE_TEXT)
            || (data[i].type == LEDA_TYPE_DATE))
        {
            cJSON_AddStringToObject(object, data[i].key, data[i].value);
        }
        else if (data[i].type == LEDA_TYPE_FLOAT)
        {
            cJSON_AddNumberToObject(object, data[i].key, strtof(data[i].value, NULL));
        }
        else if (data[i].type == LEDA_TYPE_DOUBLE)
        {
            cJSON_AddNumberToObject(object, data[i].key, strtod(data[i].value, NULL));
        }
        else if ((data[i].type == LEDA_TYPE_INT)
                 ||(data[i].type == LEDA_TYPE_BOOL)
                 || (data[i].type == LEDA_TYPE_ENUM))
        {
            cJSON_AddNumberToObject(object, data[i].key, atoi(data[i].value));
        }
        else if((data[i].type == LEDA_TYPE_STRUCT)||(data[i].type == LEDA_TYPE_ARRAY))
        {
            cJSON_AddItemToObject(object, data[i].key, cJSON_Parse(data[i].value));
        }
        else
        {
            goto END;
        }
    }

    output = cJSON_Print(object);

END:

    if (NULL != object)
    {
        cJSON_Delete(object);
    }

    return output;
}

int leda_transform_data_json_to_struct(const char* product_key, 
                                       const char* service_name, 
                                       cJSON *object, 
                                       leda_device_data_t **dev_data)
{
    int     i       = 0;
    cJSON*  item    = NULL;
    char*   buff    = NULL;

    if ((NULL == object) || (NULL == dev_data))
    {
        return 0;
    }

    cJSON_ArrayForEach(item, object)
    {
        i++;
    }

    *dev_data = (leda_device_data_t *)malloc(sizeof(leda_device_data_t) * i);
    if (NULL == *dev_data)
    {
        return 0;
    }

    memset(*dev_data, 0, sizeof(leda_device_data_t) * i);

    i = 0;
    cJSON_ArrayForEach(item, object)
    {
        (*dev_data)[i].type = LEDA_TYPE_BUTT;
        switch (item->type)
        {
        case cJSON_Number:
            {
                if (NULL != item->string)
                {
                    (*dev_data)[i].type = _leda_get_itemtype_from_tsl_serviecs(product_key, service_name, item->string);
                    snprintf((*dev_data)[i].key, MAX_PARAM_NAME_LENGTH, "%s", item->string);
                    if (item->valuedouble != item->valueint)
                    {
                        snprintf((*dev_data)[i].value, MAX_PARAM_VALUE_LENGTH, "%f", item->valuedouble);
                    }
                    else
                    {
                        snprintf((*dev_data)[i].value, MAX_PARAM_VALUE_LENGTH, "%d", item->valueint);
                    }
                    
                    ++i;
                }
                break;
            }
        case cJSON_Raw:
        case cJSON_String:
            {
                if (NULL != item->string)
                {
                    if (NULL != item->valuestring)
                    {
                        (*dev_data)[i].type = _leda_get_itemtype_from_tsl_serviecs(product_key, service_name, item->string);
                        snprintf((*dev_data)[i].key, MAX_PARAM_NAME_LENGTH, "%s", item->string);
                        snprintf((*dev_data)[i].value, MAX_PARAM_VALUE_LENGTH, "%s", item->valuestring);
                        ++i;
                    }
                }
                else if (NULL != item->valuestring)
                {
                    (*dev_data)[i].type = LEDA_TYPE_BUTT;
                    strcpy((*dev_data)[i].key, item->valuestring);
                    ++i;
                }
                break;
            }
        case cJSON_Array:
        case cJSON_Object:
            {
                if (NULL != item->string && (cJSON_Array == item->type || cJSON_Object == item->type))
                {
                    if (cJSON_Array == item->type)
                    {
                        (*dev_data)[i].type = LEDA_TYPE_ARRAY;
                    }
                    else if (cJSON_Object == item->type)
                    {
                        (*dev_data)[i].type = LEDA_TYPE_STRUCT;
                    }

                    snprintf((*dev_data)[i].key, MAX_PARAM_NAME_LENGTH, "%s", item->string);
                    buff = cJSON_Print(item);
                    if (NULL != buff)
                    {
                        snprintf((*dev_data)[i].value, MAX_PARAM_VALUE_LENGTH, "%s", buff);
                        cJSON_free(buff);
                    }

                    ++i;
                }
                break;
            }
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
        case cJSON_Invalid:
        default:
            break;
        }
    }

    return i;
}

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
