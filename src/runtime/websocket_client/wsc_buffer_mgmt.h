#ifndef _BUF_MGMT_CLIENT_H_
#define _BUF_MGMT_CLIENT_H_

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define MAX_MSG_LEN_EACH    640
#define MAX_MSG_LEN_BUF     (MAX_MSG_LEN_EACH+LWS_PRE+REQUEST_DATA_PADDING_BYTES)
#define MAX_MSG_BUF_COUNT   8

typedef struct {
	pthread_mutex_t locker;
	int rd_index;
	int wr_index;
    int single_buf_size;
    int max_buf_cnt;
} msg_buf_status;

typedef struct {
	char *buf;
	int buf_len;
	int type;
} msg_buf_item;

typedef int (*cb_del)(char *buf, int buf_len, int type, void *usr);

int client_buf_mgmt_init(int single_buf_size, int max_buf_cnt);

int client_buf_mgmt_push(const char *buf, int len, int msg_type);

int client_buf_mgmt_pop(cb_del cb, void *usr);

void buf_mgmt_client_clear_msg();

int client_buf_mgmt_destroy(void);

#endif


