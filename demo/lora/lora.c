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

# include <stdio.h>
# include <fcntl.h>
# include <errno.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <termios.h>
# include <sys/epoll.h>
# include <time.h>
# include "cjson/cJSON.h"
# include "log.h"
# include "le_error.h"
# include "leda.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG_DEMO_LED            "LoRa"
#define MAXEPOLL 1024
#define ONLINE_TIMEOUT          (3 * 3600 * 1000)   // 3小时无数据上报，下线

struct device_info
{
    char productkey[16];    // 子设备 PK
    char devicename[40];    // 子设备 DN
    uint64_t last_time;     // 子设备 最后数据上报时间
    device_handle_t handle; // 子设备 句柄
    int serial_fd;          // lora串口句柄
};

static struct device_info *g_device_array = NULL;
static int g_device_count = 0;

// 获取系统开机时间（毫秒）
static uint64_t get_time_ms()
{
	struct timespec tp;
	int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
	if(0 == ret)
	{
		return (uint64_t)(tp.tv_sec) * 1000L + tp.tv_nsec / 1000000L;
	}
	return 0;
}

// 将 FD 加入 EPOLL 队列
static int epoll_add(int epfd, int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP; // 可读 | 错误 | 关闭
    ev.data.fd = fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        log_e(TAG_DEMO_LED, "epoll_add, epoll_ctl EPOLL_CTL_ADD fail error:%s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// 将 FD 从 EPOLL 队列删除
int epoll_del(int epfd, int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP; // 可读 | 错误 | 关闭
    ev.data.fd = fd;
    if(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev) < 0)
    {
        log_e(TAG_DEMO_LED, "epoll_ctl EPOLL_CTL_DEL fd: %d, error: %s", fd, strerror(errno));
        return -1;
    }
    return 0;
}

// 打开串口操作
static int open_serial(const char *dev, int baud, uint8_t data_bit, uint8_t stop_bit, char parity)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL);
    if(fd < 0)
    {
        log_e(TAG_DEMO_LED, "open_serial, open fail error:%s\n", strerror(errno));
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, NULL);
    if(flags < 0)
    {
        log_e(TAG_DEMO_LED, "open_serial, fcntl F_GETFL fail error:%s\n", strerror(errno));
        return -1;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        log_e(TAG_DEMO_LED, "open_serial, fcntl F_SETFL fail error:%s\n", strerror(errno));
        return -1;
    }

    struct termios tios;
    memset(&tios, 0, sizeof(struct termios));

    speed_t speed;
    switch (baud) {
    case 9600:
        speed = B9600;
        break;
    case 115200:
        speed = B115200;
        break;
    default:
        log_e(TAG_DEMO_LED, "open_serial, unsuport baud speed:%d\n", speed);
        return -1;
    }

    if ((cfsetispeed(&tios, speed) < 0) || (cfsetospeed(&tios, speed) < 0))
    {
        close(fd);
        log_e(TAG_DEMO_LED, "open_serial, cfsetspeed fail error:%s\n", strerror(errno));
        return -1;
    }

    tios.c_cflag |= (CREAD | CLOCAL);
    tios.c_cflag &= ~CSIZE;
    switch (data_bit) {
    case 5:
        tios.c_cflag |= CS5;
        break;
    case 6:
        tios.c_cflag |= CS6;
        break;
    case 7:
        tios.c_cflag |= CS7;
        break;
    case 8:
    default:
        tios.c_cflag |= CS8;
        break;
    }

    if (stop_bit == 1)
        tios.c_cflag &=~ CSTOPB;
    else
        tios.c_cflag |= CSTOPB;

    if (parity == 'N') {
        tios.c_cflag &=~ PARENB;
    } else if (parity == 'E') {
        tios.c_cflag |= PARENB;
        tios.c_cflag &=~ PARODD;
    } else {
        tios.c_cflag |= PARENB;
        tios.c_cflag |= PARODD;
    }

    tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    if (parity == 'N') {
        tios.c_iflag &= ~INPCK;
    } else {
        tios.c_iflag |= INPCK;
    }

    tios.c_iflag &= ~(IXON | IXOFF | IXANY);
    tios.c_oflag &=~ OPOST;
    tios.c_cc[VMIN] = 0;
    tios.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tios) < 0) {
        close(fd);
        log_e(TAG_DEMO_LED, "open_serial, tcsetattr fail error:%s\n", strerror(errno));
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

static int get_properties_callback_cb(device_handle_t dev_handle, 
                               leda_device_data_t properties[], 
                               int properties_count, 
                               void *usr_data)
{
    log_e(TAG_DEMO_LED, "get_property not support\n");
    return LE_ERROR_SERVICE_UNREACHABLE;
}

static int set_properties_callback_cb(device_handle_t dev_handle, 
                               const leda_device_data_t properties[], 
                               int properties_count, 
                               void *usr_data)
{
    log_e(TAG_DEMO_LED, "set_property not support\n");
    return LE_ERROR_SERVICE_UNREACHABLE;
}

static int call_service_callback_cb(device_handle_t dev_handle, 
                               const char *service_name, 
                               const leda_device_data_t data[], 
                               int data_count, 
                               leda_device_data_t output_data[], 
                               void *usr_data)
{
    log_e(TAG_DEMO_LED, "call_service not support\n");
    return LE_ERROR_SERVICE_UNREACHABLE;
}

// 加载LORA节点设备
static int load_devices(int epoll_fd, struct device_info **device_array, int *device_count)
{
    /* 获取驱动设备配置 */
    int config_size = leda_get_config_size();
    if (config_size < 0)
    {
        log_i(TAG_DEMO_LED, "get config size fail\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    char *config_buffer = (char*)malloc(config_size);
    if (NULL == config_buffer)
    {
        log_e(TAG_DEMO_LED, "allocate memory failed\n");
        return LE_ERROR_INVAILD_PARAM;
    }

    int ret = leda_get_config(config_buffer, config_size);
    if (LE_SUCCESS != ret)
    {
        log_e(TAG_DEMO_LED, "get config failed\n");
        free(config_buffer);
        return ret;
    }

    /* 解析驱动设备配置 */
    cJSON *root = cJSON_Parse(config_buffer);
    if (NULL == root)
    {
        log_e(TAG_DEMO_LED, "root parser failed\n");
        free(config_buffer);
        return LE_ERROR_INVAILD_PARAM;
    }

    cJSON *config = cJSON_GetObjectItem(root, "config");
    if(NULL == config)
    {
        log_e(TAG_DEMO_LED, "config parser failed\n");
        cJSON_Delete(root);
        free(config_buffer);
        return LE_ERROR_INVAILD_PARAM;
    }

    cJSON *server_list = cJSON_GetObjectItem(config, "serverList");
    if(NULL == server_list)
    {
        log_e(TAG_DEMO_LED, "serverList parser failed\n");
        cJSON_Delete(root);
        free(config_buffer);
        return LE_ERROR_INVAILD_PARAM;
    }

    cJSON *device_list = cJSON_GetObjectItem(root, "deviceList");
    if(NULL == device_list)
    {
        log_e(TAG_DEMO_LED, "deviceList parser failed\n");
        cJSON_Delete(root);
        free(config_buffer);
        return LE_ERROR_INVAILD_PARAM;
    }

    int count = cJSON_GetArraySize(device_list);
    struct device_info *devices = (struct device_info *)malloc(sizeof(struct device_info) * count);
    if(devices == NULL)
    {
        log_e(TAG_DEMO_LED, "malloc %d devices failed\n", count);
        cJSON_Delete(root);
        free(config_buffer);
        return LE_ERROR_INVAILD_PARAM;
    }
    memset(devices, 0, sizeof(struct device_info) * count);

    int index = 0;
    cJSON *server = NULL;
    cJSON_ArrayForEach(server, server_list)
    {
        if (cJSON_Object == server->type)
        {
            char *serverId = NULL;
            {
                cJSON *result = cJSON_GetObjectItem(server, "serverId");
                serverId = result->valuestring;
            }

            char *serialPort = NULL;
            {
                cJSON *result = cJSON_GetObjectItem(server, "serialPort");
                serialPort = result->valuestring;
            }

            cJSON *device = NULL;
            cJSON_ArrayForEach(device, device_list)
            {
                if (cJSON_Object == device->type)
                {
                    cJSON *deviceId = cJSON_GetObjectItem(device, "serverId");
                    if(NULL == deviceId)
                    {
                        log_e(TAG_DEMO_LED, "serverId parser failed\n");
                        continue;
                    }

                    if(0 != strcmp(deviceId->valuestring, serverId))
                    {
                        continue;
                    }

                    int fd = open_serial(serialPort, 9600, 8, 1, 'N');
                    if(fd < 0)
                    {
                        log_e(TAG_DEMO_LED, "main, open_serial %s fail error:%s\n", serialPort, strerror(errno));
                        exit(-1);
                    }

                    if(epoll_add(epoll_fd, fd) != 0)
                    {
                        log_e(TAG_DEMO_LED, "epoll_add %d %d failed\n", epoll_fd, fd);
                        close(fd);
                        continue;
                    }

                    char *pk = NULL;
                    {
                        cJSON *result = cJSON_GetObjectItem(device, "productKey");
                        pk = result->valuestring;
                    }

                    char *dn = NULL;
                    {
                        cJSON *result = cJSON_GetObjectItem(device, "deviceName");
                        dn = result->valuestring;
                    }

                    log_i(TAG_DEMO_LED, "device[%d]: serialport: %s, productkey: %s, devicename: %s\n", index, serialPort, pk, dn);
                    strcpy(devices[index].productkey, pk);
                    strcpy(devices[index].devicename, dn);
                    devices[index].serial_fd = fd;
                    devices[index].last_time = 0;
                    devices[index].handle = -1;
                    index++;
                }
            }
        }
    }

    cJSON_Delete(root);
    free(config_buffer);

    *device_array = devices;
    *device_count = count;
    return LE_SUCCESS;
}

int main(int argc, char** argv)
{
    if(setvbuf(stdout, NULL, _IOLBF, 0))
    {
        exit(-100);
    }
    log_init(TAG_DEMO_LED, LOG_STDOUT, LOG_LEVEL_DEBUG, LOG_MOD_BRIEF);

    int ret = LE_SUCCESS;
    if (LE_SUCCESS != (ret = leda_init(2)))
    {
        log_e(TAG_DEMO_LED, "leda_init failed\n");
        return ret;
    }

    int epoll_fd = epoll_create(MAXEPOLL);
    if(epoll_fd < 0)
    {
        log_e(TAG_DEMO_LED, "epoll_create failed\n");
        return epoll_fd;
    }

    ret = load_devices(epoll_fd, &g_device_array, &g_device_count);
    if (LE_SUCCESS != ret)
    {
        log_e(TAG_DEMO_LED, "load devices failed\n");
        return ret;
    }

    int loopping = 1;
    while(loopping)
    {
        struct epoll_event evs[MAXEPOLL];
        int wait_fds = epoll_wait(epoll_fd, evs, MAXEPOLL, 1000);
        if(wait_fds < 0)
        {
            break;
        }

        uint64_t now_time = get_time_ms();
        if(wait_fds == 0)
        {
            int i = 0;
            for(; i < g_device_count; i++)
            {
                struct device_info *device = g_device_array +i;
                if(device->handle >= 0 && now_time - device->last_time > ONLINE_TIMEOUT)
                {
                    log_i(TAG_DEMO_LED, "product:%s device:%s offline because of timeout\n", device->productkey, device->devicename);
                    leda_offline(device->handle);
                    device->handle = -1;
                }
            }
            continue;
        }

        int i = 0;
        for(; i < wait_fds; i++)
        {
            int fd = evs[i].data.fd;
            if(evs[i].events & EPOLLIN)
            {
                uint8_t recv_buff[128] = {0};
                int nread = read(fd, recv_buff, sizeof(recv_buff));
                if(nread <= 0)
                {
                    log_e(TAG_DEMO_LED, "loop, read error nread:%d size:%zu error:%s\n", nread, sizeof(recv_buff), strerror(errno));
                    loopping = 0;
                    break;
                }

                struct device_info *device = NULL;
                int index = 0;
                for(; index < g_device_count; index++)
                {
                    if(fd == g_device_array[index].serial_fd)
                    {
                        device = g_device_array + index;
                        break;
                    }
                }

                if(device == NULL)
                {
                    log_e(TAG_DEMO_LED, "loop, can't find device handle\n");
                    continue;
                }

                /********** 数据解析开始 各款传感器不一致 ***********/
                leda_device_data_t dev_proper_data[4];
                memset(&dev_proper_data, 0, sizeof(dev_proper_data));
                int count = 0;

                index = 0;
                while(index < nread - 1 && index < 100)
                {
                    uint8_t channel = recv_buff[index];
                    uint8_t type = recv_buff[index + 1];
                    if(channel == 0x01)
                    {
                        if(type == 0x67)
                        {
                            float CurrentTemperature = ((float)recv_buff[index + 2] + ((float)recv_buff[index + 3]) * 255) / 10;
                            dev_proper_data[count].type = LEDA_TYPE_FLOAT;
                            strcpy(dev_proper_data[count].key, "CurrentTemperature");
                            sprintf(dev_proper_data[count].value, "%f", CurrentTemperature);
                            count++;
                            index += 4;
                        }
                        else
                        {
                            log_e(TAG_DEMO_LED, "loop, unknow type: %u in channel: %d\n", type, channel);
                        }
                    }
                    else if(channel == 0x02)
                    {
                        if(type == 0x68)
                        {
                            float RelativeHumidity = ((float)recv_buff[index + 2]) / 2;
                            dev_proper_data[count].type = LEDA_TYPE_FLOAT;
                            strcpy(dev_proper_data[count].key, "RelativeHumidity");
                            sprintf(dev_proper_data[count].value, "%f", RelativeHumidity);
                            count++;
                            index += 3;
                        }
                        else
                        {
                            log_e(TAG_DEMO_LED, "loop, unknow type: %u in channel: %d\n", type, channel);
                        }
                    }
                    else if(channel == 0x03)
                    {
                        if(type == 0x75)
                        {
                            float BatteryPercentage = (float)recv_buff[index + 2];
                            dev_proper_data[count].type = LEDA_TYPE_FLOAT;
                            strcpy(dev_proper_data[count].key, "BatteryPercentage");
                            sprintf(dev_proper_data[count].value, "%f", BatteryPercentage);
                            count++;
                            index += 2;
                        }
                        else
                        {
                            log_e(TAG_DEMO_LED, "loop, unknow type: %u in channel: %d\n", type, channel);
                        }
                    }
                    else if(channel == 0xff)
                    {
                        if(type == 0x0b || type == 0x01)
                        {
                            index += 3;
                        }
                        else if(type == 0x08 || type == 0x09)
                        {
                            index += 8;
                        }
                        else if(type == 0x0d)
                        {
                            float CurrentTemperature = ((float)recv_buff[index + 7] + ((float)recv_buff[index + 8]) * 255) / 10;
                            dev_proper_data[count].type = LEDA_TYPE_FLOAT;
                            strcpy(dev_proper_data[count].key, "CurrentTemperature");
                            sprintf(dev_proper_data[count].value, "%f", CurrentTemperature);
                            count++;
                            index += 9;
                        }
                        else
                        {
                            log_e(TAG_DEMO_LED, "loop, unknow type: %u in channel: %d\n", type, channel);
                        }
                    }
                    else
                    {
                        log_e(TAG_DEMO_LED, "loop, unknow channel: %u\n", channel);
                        break;
                    }
                }
                /********** 数据解析结束 ***********/

                if(count < 0)
                {
                    continue;
                }

                if(device->handle < 0)
                {
                    leda_device_callback_t device_cb;
                    device_cb.get_properties_cb            = get_properties_callback_cb;
                    device_cb.set_properties_cb            = set_properties_callback_cb;
                    device_cb.call_service_cb              = call_service_callback_cb;
                    device_cb.service_output_max_count     = 5;

                    device_handle_t handle = leda_register_and_online_by_device_name(device->productkey, device->devicename, &device_cb, NULL);
                    if(handle < 0)
                    {
                        log_e(TAG_DEMO_LED, "product:%s device:%s register failed\n", device->productkey, device->devicename);
                        continue;
                    }
                    device->handle = handle;
                }

                device->last_time = now_time;
                leda_report_properties(device->handle, dev_proper_data, count);
            }
            else
            {
                log_e(TAG_DEMO_LED, "loop, fd:%d, events:%d\n", evs[i].data.fd, evs[i].events);
                loopping = 0;
                break;
            }
        }
    }

    // 关闭所有句柄
    int i = 0;
    for(; i < g_device_count; i++)
    {
        struct device_info *device = g_device_array +i;
        if(device->handle >= 0)
        {
            leda_offline(device->handle);
            device->handle = -1;
        }
    }

    free(g_device_array);
    g_device_array = NULL;
    g_device_count = 0;

    /* 退出驱动 */
    leda_exit();
    return LE_SUCCESS;
}

#ifdef __cplusplus
}
#endif
