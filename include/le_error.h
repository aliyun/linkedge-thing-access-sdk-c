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

#ifndef __LINKEDGE_ERROR__
#define __LINKEDGE_ERROR__

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#define LE_SUCCESS                              0                   /* 请求成功*/
#define LE_ERROR_UNKNOWN                        100000              /* 不能被识别的错误，用户不应该看到的错误*/
#define LE_ERROR_INVAILD_PARAM                  100001              /* 传入参数为NULL或无效*/
#define LE_ERROR_ALLOCATING_MEM                 100002              /* 指定的内存分配失败*/
#define LE_ERROR_CREATING_MUTEX                 100003              /* 创建mutex失败*/
#define LE_ERROR_WRITING_FILE                   100004              /* 写入文件失败*/
#define LE_ERROR_READING_FILE                   100005              /* 读取文件失败*/
#define LE_ERROR_TIMEOUT                        100006              /* 超时*/
#define LE_ERROR_PARAM_RANGE_OVERFLOW           100007              /* 参数范围越界*/
#define LE_ERROR_SERVICE_UNREACHABLE            100008              /* 服务不可达*/
#define LE_ERROR_FILE_NOT_EXIST                 100009              /* 文件不存在*/

#define LEDA_ERROR_DEVICE_UNREGISTER            109000              /* 设备未注册*/ 
#define LEDA_ERROR_DEVICE_OFFLINE               109001              /* 设备已下线*/
#define LEDA_ERROR_PROPERTY_NOT_EXIST           109002              /* 属性不存在*/
#define LEDA_ERROR_PROPERTY_READ_ONLY           109003              /* 属性只读*/
#define LEDA_ERROR_PROPERTY_WRITE_ONLY          109004              /* 属性只写*/
#define LEDA_ERROR_SERVICE_NOT_EXIST            109005              /* 服务不存在*/
#define LEDA_ERROR_SERVICE_INPUT_PARAM          109006              /* 服务的输入参数不正确错误码*/
#define LEDA_ERROR_INVALID_JSON                 109007              /* JSON格式错误*/
#define LEDA_ERROR_INVALID_TYPE                 109008              /* 参数类型错误*/

#ifdef __cplusplus  /* If this is a C++ compiler, use C linkage */
}
#endif
#endif