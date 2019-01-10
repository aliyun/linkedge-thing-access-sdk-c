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
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "log.h"

#define MAX_MSG_LEN     512*2

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#define PRINT_WITH_TIMESTAMP 0

#if PRINT_WITH_TIMESTAMP
//date [module] level <tag> file-func:line content
#define LOG_FMT_VRB  "%s %s <%s> %s-%s:%d "
#else
//[module] level <tag> file-func:line content
#define LOG_FMT_VRB  "%s <%s> %s-%s:%d "
#endif

static const char *g_log_desc[] = 
{ 
    "DEBUG", 
    "INFO",
    "WARNING", 
    "ERROR",
    "FATAL" 
};

static int g_log_lvl = LOG_LEVEL_ERR;

#if PRINT_WITH_TIMESTAMP
static char *get_timestamp(char *buf, int len, time_t cur_time)
{
    struct tm tm_time;

    localtime_r(&cur_time, &tm_time);

    snprintf(buf, len, "%d-%d-%d %d:%d:%d",
             1900 + tm_time.tm_year, 1 + tm_time.tm_mon,
             tm_time.tm_mday, tm_time.tm_hour,
             tm_time.tm_min, tm_time.tm_sec);
    return buf;
}
#endif

#define color_len_fin strlen(COL_DEF)
#define color_len_start strlen(COL_RED)
void log_print(unsigned char lvl, const char *color, const char *t, const char *f, const char *func, unsigned int l, const char *fmt, ...)
{
    if(lvl < g_log_lvl)
        return;

    va_list ap;
    va_start(ap, fmt);
    char *buf = NULL;

    char *tmp = NULL;
    int len = 0;

#if PRINT_WITH_TIMESTAMP
    char buf_date[20] = {0};
    time_t cur_time = time(NULL);
#endif

    t = !t ? "\b" : t;
    f = !f ? "\b" : f;
    func = !func ? "\b" : func;

    tmp = strrchr(f, '/');
    if(tmp)
        f = tmp + 1;

    buf = malloc(MAX_MSG_LEN + 1);
    if(NULL == buf){
        printf("failed to malloc memory .\n");
        return;
    }

    memset(buf, 0, MAX_MSG_LEN + 1);

    //add color support
    if (color) 
        strcat(buf, color);

#if PRINT_WITH_TIMESTAMP
    snprintf(buf + strlen(buf), MAX_MSG_LEN - strlen(buf), LOG_FMT_VRB,
                 get_timestamp(buf_date, 20, cur_time),
                 g_log_desc[lvl], t, f, func, l);
#else
    snprintf(buf + strlen(buf), MAX_MSG_LEN - strlen(buf), LOG_FMT_VRB,
                 g_log_desc[lvl], t, f, func, l);
#endif

    len = MAX_MSG_LEN - strlen(buf) - color_len_fin - 5;
    len = len <= 0 ? 0 : len;
    tmp = buf + strlen(buf);
    if (vsnprintf(tmp, len, fmt, ap) > len) 
        strcat(buf, "...\n");

    if (color) 
        strcat(buf, COL_DEF);

    fprintf(stdout, "%s", buf);
    if (color) 
        buf[strlen(buf) - color_len_fin] = '\0';

    va_end(ap);

    free(buf);
}

void log_set_level(LOG_LEVEL lvl)
{
    if(lvl < LOG_LEVEL_DEBUG || lvl > LOG_LEVEL_FATAL)
        g_log_lvl = LOG_LEVEL_ERR; 
    else 
        g_log_lvl = lvl;
    printf("set log level :  %d\n", lvl);
}

int log_init(const char *name, LOG_STORE_TYPE type, LOG_LEVEL lvl, LOG_MODE mode)
{ 
    if(lvl < LOG_LEVEL_DEBUG || lvl > LOG_LEVEL_FATAL)
        g_log_lvl = LOG_LEVEL_ERR; 
    else 
        g_log_lvl = lvl;

    if (setvbuf(stdout, NULL, _IOLBF, 0))
    {
        printf("setbuf error: %s\n", strerror(errno));
    }

    return 0;
}

void log_destroy()
{
    return;   
}

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif
