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
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <cJSON.h>

#include "log.h"
#include "le_error.h"
#include "ws_client.h"
#include "runtime.h"

#define RUNTIME_TAG_NAME    "C_RUNTIME_LOG"

static  pthread_t           g_fcbase_thread_id;
static  int                 g_run_state = 0;
static  fc_handler          g_fc_handler = NULL;

static void _runtime_cb_recv(const char *msg, int len, void *user)
{
    (void)msg;
    (void)len;
    (void)user;
    ;
}

static void _runtime_cb_establish(void *user)
{
    (void)user;

    log_i(RUNTIME_TAG_NAME, "establish connection success.\r\n");
}

static void _runtime_cb_close(void *user)
{
    (void)user;

    log_i(RUNTIME_TAG_NAME, "the peer disconnect.\r\n");
}

static void *_runtime_connect_fcbase(void *arg)
{
    int ret = 0;

    wsc_param_conn conn_param;
    wsc_param_cb   cb_param;
    
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.url               = "ws://127.0.0.1:8089/";
    conn_param.ca_path           = NULL;
    conn_param.cert_path         = NULL;
    conn_param.key_path          = NULL;
    conn_param.protocol          = NULL;
    conn_param.timeout           = 50;

    memset(&cb_param, 0, sizeof(cb_param));
    cb_param.usr_cb_establish    = NULL;
    cb_param.p_cb_establish      = _runtime_cb_establish;
    cb_param.usr_cb_close        = NULL;
    cb_param.p_cb_close          = _runtime_cb_close;
    cb_param.usr_cb_recv         = NULL;
    cb_param.p_cb_recv           = _runtime_cb_recv;

    ret = wsc_init(&conn_param, &cb_param);
    while (!ret)
    {
        sleep(1);

        if (!g_run_state)
        {
            break;
        }
    }

    log_e(RUNTIME_TAG_NAME, "disconnet with fcbase.\r\n");
    ws_client_destroy();
    pthread_exit(NULL);

    return NULL;
}

/*
 * 运行时初始函数.
 *
 * handler:     入口函数.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int runtime_init(fc_handler handler)
{
    /* 和FC Base建立连接 */
    if (0 != pthread_create(&g_fcbase_thread_id, NULL, _runtime_connect_fcbase, NULL))
    {
        log_e(RUNTIME_TAG_NAME, "create thread failed\r\n");
        return LE_ERROR_UNKNOWN;
    }

    g_fc_handler = handler;
    g_run_state = 1;
    return LE_SUCCESS;
}

/*
 * 运行时退出环境.
 *
 *
 * 阻塞接口.
 */
void runtime_exit(void)
{
    g_run_state = 0;
    return;
}
