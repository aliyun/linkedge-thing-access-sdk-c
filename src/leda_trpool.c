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
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>

#include "log.h"
#include "le_error.h"
#include "leda.h"
#include "linux-list.h"
#include "leda_trpool.h"

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

void *leda_thread_routine(void *arg);

static CThread_pool *pool = NULL;

void leda_pool_init (int max_thread_num)
{
    int i = 0;

    pool = (CThread_pool *)malloc(sizeof(CThread_pool));
    if (NULL == pool)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return;
    }

    (void)memset(pool, 0, sizeof(CThread_pool));
    pthread_mutex_init(&(pool->queue_lock), NULL);
    pthread_cond_init(&(pool->queue_ready), NULL);
    pool->queue_head = NULL;
    pool->threadid   = (pthread_t *)malloc(max_thread_num * sizeof(pthread_t));
    if (NULL == pool->threadid)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        free(pool);
        return;
    }

    for (i = 0; i < max_thread_num; i++)
    { 
        (void)pthread_create(&(pool->threadid[i]), NULL, leda_thread_routine, NULL);
    }

    pool->max_thread_num = max_thread_num;
    pool->cur_queue_size = 0;
    pool->shutdown       = 0;

    return;
}

int leda_pool_add_worker (void *(*process)(void *arg), void *arg)
{
    CThread_worker *newworker = (CThread_worker *)malloc(sizeof(CThread_worker));
    if (NULL == newworker)
    {
        log_w(LEDA_TAG_NAME, "no memory can allocate\n");
        return LE_ERROR_ALLOCATING_MEM;
    }

    (void)memset(newworker, 0, sizeof(CThread_worker));
    newworker->process  = process;
    newworker->arg      = arg;
    newworker->next     = NULL;

    pthread_mutex_lock(&(pool->queue_lock));
    CThread_worker *member = pool->queue_head;
    if (member != NULL)
    {
        while (member->next != NULL)
        {
            member = member->next;
        }
        member->next = newworker;
    }
    else
    {
        pool->queue_head = newworker;
    }

    assert(pool->queue_head != NULL);
    pool->cur_queue_size++;
    pthread_mutex_unlock(&(pool->queue_lock));

    pthread_cond_signal (&(pool->queue_ready));

    return LE_SUCCESS;
}

int leda_pool_destroy(void)
{
    int i;
    CThread_worker *head = NULL;

    if (pool->shutdown)
    {
        return LE_ERROR_UNKNOWN;
    }
    pool->shutdown = 1;

    /* 唤醒所有等待线程，线程池要销毁了 */
    pthread_cond_broadcast(&(pool->queue_ready));

    /* 阻塞等待线程退出，否则就成僵尸了 */
    for (i = 0; i < pool->max_thread_num; i++)
    {
        pthread_join(pool->threadid[i], NULL);
    }
    free(pool->threadid);

    /* 销毁等待队列 */
    while (pool->queue_head != NULL)
    {
        head = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free(head);
    }

    /* 条件变量和互斥量也别忘了销毁 */
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));
    
    free(pool);
    pool=NULL;

    return LE_SUCCESS;
}

void *leda_thread_routine(void *arg)
{
    struct timespec tout;

    log_i(LEDA_TAG_NAME, "starting thread 0x%x\n", pthread_self());

    while (1)
    {
        clock_gettime(CLOCK_REALTIME, &tout);
        tout.tv_sec += 1;
        if(0 != pthread_mutex_timedlock(&(pool->queue_lock), &tout))
        {
            continue;
        }

        /*  如果等待队列为0并且不销毁线程池，则处于阻塞状态; 
            注意pthread_cond_wait是一个原子操作，等待前会解锁，唤醒后会加锁 */
        while (pool->cur_queue_size == 0 && !pool->shutdown)
        {
            log_d(LEDA_TAG_NAME, "thread 0x%x is waiting\n", pthread_self());
            pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
        }

        /* 线程池要销毁了 */
        if (pool->shutdown)
        {
            pthread_mutex_unlock (&(pool->queue_lock));
            log_w(LEDA_TAG_NAME, "thread 0x%x will exit\n", pthread_self());
            pthread_exit (NULL);
        }

        log_d(LEDA_TAG_NAME, "thread 0x%x is starting to work\n", pthread_self());
        assert (pool->cur_queue_size != 0);
        assert (pool->queue_head != NULL);
        
        /* 等待队列长度减去1，并取出链表中的头元素 */
        CThread_worker *worker = pool->queue_head;
        pool->queue_head = worker->next;
        pool->cur_queue_size--;

        /* 调用回调函数，执行任务 */
        pthread_mutex_unlock(&(pool->queue_lock));
        (*(worker->process))(worker->arg);
        free(worker);
        worker = NULL;
    }

    pthread_exit (NULL);

    return NULL;
}

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
