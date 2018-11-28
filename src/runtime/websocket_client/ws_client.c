#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "libwebsockets.h"
#include "ws_client.h"
#include "wsc_buffer_mgmt.h"

extern void *thread_wsc_network(void *arg);
extern volatile int force_exit;
extern struct lws_context *context;

p_wsc_param_cb g_cbs = NULL;

static void sighandler(int sig)
{
	force_exit = 1;
    if(context){
        lws_cancel_service(context);
        context = NULL; 
    }
    ws_client_destroy();
    exit(0);
}

int wsc_init(p_wsc_param_conn pc, p_wsc_param_cb cb)
{
    int ret = 0; 
    if(!pc || !cb || !pc->url){
        printf("wsc init failed, param should not be NULL.\n"); 
       return 1; 
    }
  
    printf("--------------->>>>>>>>:%s\n", pc->url);
    if(!strncmp(pc->url, "wss", 3) && !pc->ca_path)
        printf("ca path must not be NULL when use ssl\n");
    
    g_cbs = malloc(sizeof(wsc_param_cb)); 
    if(!g_cbs){
        printf("malloc error.\n");
        return 2;
    } 
    
    memset(g_cbs, 0, sizeof(wsc_param_cb));
    memcpy(g_cbs, cb, sizeof(wsc_param_cb));

    pthread_t id;
    ret = pthread_create(&id, NULL, thread_wsc_network, pc);
    if(ret == 0)
        printf("wsc init ok\n");
    else{
        free(g_cbs);
        g_cbs = NULL;
        printf("wsc init faild, %s.\n",strerror(errno));
        return 3;
    }

    ret = client_buf_mgmt_init(1024*2, 1024); 
    if(0 != ret){
        printf("failed to init wsc message mangement.\n"); 
        return 4;
    }

	signal(SIGINT, sighandler);
    return 0;
}

int wsc_add_msg(const char *msg, int len, int type)
{
    if(!msg || len <= 0 || type > 1 || type < 0)
        return -1;
    
    return client_buf_mgmt_push(msg, len, type);
}

int ws_client_destroy()
{
    if(g_cbs){
        free(g_cbs);
        g_cbs = NULL;
    }

    return client_buf_mgmt_destroy();
}

