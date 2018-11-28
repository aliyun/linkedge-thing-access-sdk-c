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

#ifndef __LINKEDGE_RUN_TIME__
#define __LINKEDGE_RUN_TIME__

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/*
 * FC触发入口函数.
 *
 * event:       触发事件.
 * context:     事件上下文.
 *
 * 阻塞接口, 返回执行结果.
 */
typedef char* (*fc_handler)(const char *event, const char *context);

/*
 * 运行时初始函数.
 *
 * handler:     入口函数.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int runtime_init(fc_handler handler);

/*
 * 运行时退出环境.
 *
 *
 * 阻塞接口.
 */
void runtime_exit(void);

#ifdef __cplusplus  /* If this is a C++ compiler, use C linkage */
}
#endif
#endif
