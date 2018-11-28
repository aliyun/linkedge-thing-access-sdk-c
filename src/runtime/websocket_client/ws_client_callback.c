#include "libwebsockets.h"
#include "ws_client.h"
#include "wsc_buffer_mgmt.h"

extern p_wsc_param_cb g_cbs;
extern struct lws *g_wsi;
int cb_pop_msg(char *buf, int buf_len, int type, void *usr)
{
    struct lws *wsi = usr;
    
    if(!usr)
        return -1;
    return lws_write(wsi, (unsigned char*)buf, buf_len, type);;
}

static int recvframeAppend(struct lws *wsi, void **appendBuffer, void *in, size_t preLen, size_t len)
{
    char *tmp = NULL;
    
    if (*appendBuffer == NULL){
        tmp = malloc(preLen + len + 1);
    } else {
        tmp = realloc((void *)*appendBuffer,preLen + len + 1);
    } 

    if(!tmp)
    {
        lwsl_notice("recv malloc error:  %p\n", wsi);
        return 0;
    }

    memcpy(tmp + preLen, (void *)in, len);
    *(tmp + preLen +len) = 0;
    *appendBuffer = tmp;
    return len;
}


int callback_dumb_increment(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
    wsc_recv_tmpInfo* tmp = (wsc_recv_tmpInfo*)user;
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            if(g_cbs && g_cbs->p_cb_establish)
                g_cbs->p_cb_establish(g_cbs->usr_cb_establish);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            client_buf_mgmt_pop(cb_pop_msg, (void *)wsi);
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
           if (lws_is_final_fragment(wsi))
            {
                tmp->totalLen += recvframeAppend(wsi,(void **)&tmp->appendBuffer,in,tmp->totalLen,len);
                        
                if(g_cbs && g_cbs->p_cb_recv)
                {
                    g_cbs->p_cb_recv(tmp->appendBuffer, tmp->totalLen, g_cbs->usr_cb_recv);
                }
                if(tmp->appendBuffer != NULL){
                    free(tmp->appendBuffer);
                    tmp->appendBuffer = NULL;
                } 
                tmp->totalLen = 0;
            }
            else
            {
                tmp->totalLen += recvframeAppend(wsi,(void **)&tmp->appendBuffer,in,tmp->totalLen,len);
            }
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if(g_cbs && g_cbs->p_cb_close)
                g_cbs->p_cb_close(g_cbs->usr_cb_close);
            g_wsi = NULL;
            break;
        case LWS_CALLBACK_CLOSED:
            if(g_cbs && g_cbs->p_cb_close)
                g_cbs->p_cb_close(g_cbs->usr_cb_close);
            g_wsi = NULL; 
            buf_mgmt_client_clear_msg();
            return -1;
        default:
            break;
    }

	return 0;
}
