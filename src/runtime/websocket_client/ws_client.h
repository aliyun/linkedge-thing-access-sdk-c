#ifndef __WS_CLIENT_H__
#define __WS_CLIENT_H__

typedef struct{
    char                *url;           //wss://127.0.0.1:5432/
    int                 timeout;        //timeout to close current connection.
    char                *ca_path;       //path of the ca. 
    char                *cert_path;     //path of the cert. 
    char                *key_path;       //path of the private key.
    char                *protocol;      //"sec-websocket-protocol" filed in websocket handshark protocol, NULL will  be the default "alibaba-iot-linkedge-protocol"
}wsc_param_conn, *p_wsc_param_conn;

typedef struct {
    char *appendBuffer;
    size_t totalLen;
}wsc_recv_tmpInfo;



/*msg recv callback
 *
 * len:     the length of @msg
 * user:    the user data delivered by wss_register_recv_cb
 *
 * */
typedef void (*wsc_callback_recv)(const char *msg, int len, void *user);

/*connection established/close callback
 *
 * conn:    current connection hander.
 * user:    the user data delivered by @wss_param_cb
 *
 * */
typedef void (*wsc_callback_conn_changed)(void *user);

typedef struct{
    wsc_callback_conn_changed p_cb_establish;
    void                      *usr_cb_establish;
    wsc_callback_conn_changed p_cb_close;
    void                      *usr_cb_close;
    wsc_callback_recv         p_cb_recv;
    void                      *usr_cb_recv;
}wsc_param_cb, *p_wsc_param_cb;

/*module init
 *  
 *  pc:     @wss_param_conn paramaters 
 *  cb:     @wss_param_cb callbacks to handle data and connection.
 *  return value: 0 on success , error code on failed. 
 * */
int wsc_init(p_wsc_param_conn pc, p_wsc_param_cb cb);

/*add message to current connection.
 *
 *  msg:        pointer to the data you want to send. 
 *  len:        length of the data you want to send.
 *  type:       0: text , 1: binary
 *
 *  return value: 0 on success , error code on failed.
 * */
int wsc_add_msg(const char *msg, int len, int type);

/*module destroy. 
 *
 *  return value: 0 on success , error code on failed.
 * */
int ws_client_destroy(); 

#endif

