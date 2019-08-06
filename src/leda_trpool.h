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

#ifndef __LEDA_TRPOOL_H
#define __LEDA_TRPOOL_H

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/*
* 任务结构
* 注: 线程池里所有运行和等待的任务都是一个CThread_worker. 由于所有任务都在链表里, 所以是一个链表结构
*/
typedef struct worker
{
    void            *(*process) (void *arg);/* 回调函数 */
    void            *arg;                   /* 回调函数的参数 */
    struct worker   *next;
} CThread_worker;

/* 线程池结构 */
typedef struct
{
    pthread_mutex_t queue_lock;
    pthread_cond_t  queue_ready;
    CThread_worker  *queue_head;            /* 链表结构, 线程池中所有等待任务 */
    int             shutdown;               /* 是否销毁线程池 */
    pthread_t       *threadid;
    int             max_thread_num;         /* 线程池中允许的活动线程数目 */
    int             cur_queue_size;         /* 当前等待队列的任务数目 */
} CThread_pool;

void leda_pool_init(int max_thread_num);
int  leda_pool_add_worker(void *(*process)(void *arg), void *arg);
int  leda_pool_destroy(void);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
#endif
