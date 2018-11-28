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

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#define THIRD_LIB_NAME  "third_lib"

/*
 * 获取第三方库名字.
 *
 *
 * 阻塞接口, 成功返回第三方库名字, 失败返回空指针.
 */
const char* get_third_lib_name(void)
{
    return THIRD_LIB_NAME;
}

#ifdef __cplusplus  /* If this is a C++ compiler, use C linkage */
}
#endif