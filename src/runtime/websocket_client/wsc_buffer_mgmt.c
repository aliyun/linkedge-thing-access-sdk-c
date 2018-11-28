#include <string.h>
#include <errno.h>
#include "libwebsockets.h"
#include "wsc_buffer_mgmt.h"

static msg_buf_status g_ws_buf_state;
static msg_buf_item *g_ws_buf = NULL;

int client_buf_mgmt_init(int single_buf_size, int max_buf_cnt)
{
	int i = 0;
    int ret = 0;

    if(g_ws_buf || single_buf_size <= 0 || max_buf_cnt <= 0)
        return -1;

    memset(&g_ws_buf_state, 0, sizeof(msg_buf_status));
    ret = pthread_mutex_init(&g_ws_buf_state.locker, NULL);
    if(ret != 0){
        printf("failed to init buffer locker.\n");
        return -2; 
    }
    g_ws_buf_state.rd_index = 0;
	g_ws_buf_state.wr_index = 0;
    g_ws_buf_state.single_buf_size = single_buf_size;
    g_ws_buf_state.max_buf_cnt = max_buf_cnt;

    g_ws_buf = malloc(max_buf_cnt * sizeof(msg_buf_item));
    if(!g_ws_buf){
        printf("failed to malloc memory for client buffer mgmt\n");
        pthread_mutex_destroy(&g_ws_buf_state.locker);
        return -3;
    }
    memset(g_ws_buf, 0, sizeof(msg_buf_item) * max_buf_cnt);
	for (i = 0; i < max_buf_cnt; i++) {
        g_ws_buf[i].buf = malloc(single_buf_size);
        if(!g_ws_buf[i].buf)
            goto _failed;
		memset(g_ws_buf[i].buf, 0, single_buf_size);
	}

    return 0;
_failed:
    for(i = 0; i < max_buf_cnt; i++){
        if(g_ws_buf[i].buf){
            free(g_ws_buf[i].buf);
            g_ws_buf[i].buf = NULL;
        }else
            break;
    }
    
    free(g_ws_buf);
    g_ws_buf = NULL;

	return -5;
}

extern void notify_network();
int client_buf_mgmt_push(const char *buf, int len, int msg_type)
{
    char         *tmp = NULL;
    int           windex = 0;

    if(!buf || !g_ws_buf)
        return -1;

	pthread_mutex_lock(&g_ws_buf_state.locker);
    
    windex = g_ws_buf_state.wr_index;
	if (windex >= g_ws_buf_state.max_buf_cnt){
		g_ws_buf_state.wr_index = 0;
        windex = 0;
    }
    if(len > g_ws_buf_state.single_buf_size - LWS_PRE){
        tmp = realloc(g_ws_buf[windex].buf, len + LWS_PRE);    
        if(!tmp){
            printf("failed to alloc more memory to store msg\n");
            pthread_mutex_unlock(&g_ws_buf_state.locker);
            return -2;
        }
        memset(tmp, 0, len + LWS_PRE);
        g_ws_buf[windex].buf = tmp;
    }

	memset(g_ws_buf[windex].buf, 0, g_ws_buf[windex].buf_len);
	memcpy(g_ws_buf[windex].buf + LWS_PRE, buf, len);
	g_ws_buf[windex].buf_len = len;
	g_ws_buf[windex].type = msg_type;

	g_ws_buf_state.wr_index++;

	pthread_mutex_unlock(&g_ws_buf_state.locker);

    notify_network();
	return 0;
}

int client_buf_mgmt_pop(cb_del cb, void *usr)
{
	int ret = 0;
    int rindex = 0;

	pthread_mutex_lock(&g_ws_buf_state.locker);
    
    rindex = g_ws_buf_state.rd_index;
	if (rindex >= g_ws_buf_state.max_buf_cnt){
		g_ws_buf_state.rd_index = 0;
        rindex = 0; 
    }

	if (g_ws_buf[rindex].buf_len == 0) {
		pthread_mutex_unlock(&g_ws_buf_state.locker);
		return 0;
	}
    
    ret = cb(g_ws_buf[rindex].buf + LWS_PRE, 
                        g_ws_buf[rindex].buf_len, g_ws_buf[rindex].type, usr);
    
    if(ret < g_ws_buf[rindex].buf_len){
        pthread_mutex_unlock(&g_ws_buf_state.locker);
        printf("failed to post msg to server.\n"); 
        return -1;
    }
    g_ws_buf[rindex].buf_len = 0;
	g_ws_buf_state.rd_index++;
	pthread_mutex_unlock(&g_ws_buf_state.locker);
	return 0;
}

void buf_mgmt_client_clear_msg()
{
	int i = 0;
	pthread_mutex_lock(&g_ws_buf_state.locker);

	g_ws_buf_state.wr_index = 0;
	g_ws_buf_state.rd_index = 0;

	for (i = 0; i < MAX_MSG_BUF_COUNT; i++) {
		g_ws_buf[i].buf_len = 0;
	}
	pthread_mutex_unlock(&g_ws_buf_state.locker);
}

int client_buf_mgmt_destroy(void)
{
	int i = 0;
    
    if(!g_ws_buf)
        return 0;

	pthread_mutex_lock(&g_ws_buf_state.locker);
	for (i = 0; i < g_ws_buf_state.max_buf_cnt; i++) {
		if (NULL != g_ws_buf[i].buf)
			free(g_ws_buf[i].buf);
	}
    free(g_ws_buf);
    g_ws_buf = NULL;
	pthread_mutex_unlock(&g_ws_buf_state.locker);

	pthread_mutex_destroy(&g_ws_buf_state.locker);
    memset(&g_ws_buf_state, 0, sizeof(msg_buf_status));

    return 0;
}

