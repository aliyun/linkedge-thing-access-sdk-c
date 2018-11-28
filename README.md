# Link IoT Edge设备接入SDK C语言版
本项目提供一个设备接入SDK C版本库，方便用户在[Link IoT Edge](https://help.aliyun.com/product/69083.html?spm=a2c4g.11186623.6.540.7c1b705eoBIMFA)上编写驱动以接入设备。

## 快速开始 - demo_led
`demo_led`示例演示将设备接入Link IoT Edge的全过程。

### demo编译演示准备
demo编译演示准备见[编译演示准备](docs/develop-guide.md)

### demo编译
    $ git clone git@github.com:aliyun/link-iot-edge-access-sdk-c.git
    
    $ cd driver

    $ make prepare              #预编译生成外部依赖库

    $ make && make install      #生成leda_sdk_c和demo

### demo演示
1. 进入工程build/bin/demo/led目录。
2. 找到led_demo.zip压缩包。
3. 进入Link IoT Edge控制台，**分组管理**，**驱动管理**，**新建驱动**。
4. 语言类型选择*c*。
5. 驱动名称设置为`demo_led`，并上传前面准备好的zip包。
6. 创建一个产品。该产品包含一个`temperature`属性（int32类型）和一个`high_temperature`事件（int32类型和一个int32类型名为`temperature`的输入参数）。
7. 创建一个名为`demo_led`的上述产品的设备。
8. 创建一个新的分组，并将Link IoT Edge网关设备加入到分组。
9. 进入设备驱动页，将之前添加的驱动加入到分组。
10. 将`demo_led`设备添加到分组，并将`demo_led`驱动作为其驱动。
11. 使用如下配置添加*消息路由*：
  * 消息来源：`demo_led`设备
  * TopicFilter：属性
  * 消息目标：IoT Hub
12. 部署分组。`demo_led`设备将每隔5秒上报属性到云端，可在Link IoT Edge控制台设备运行状态页面查看。


## API参考手册

- **[get_properties_callback](#get_properties_callback)**
- **[set_properties_callback](#set_properties_callbackr)**
- **[call_service_callback](#call_service_callback)**
- **[leda_init](#leda_init)**
- **[config_changed_callback](#config_changed_callback)**
- **[leda_register_config_changed_callback](#leda_register_config_changed_callback)**
- **[leda_register_and_online_by_device_name](#leda_register_and_online_by_device_name)**
- **[leda_register_and_online_by_local_name](#leda_register_and_online_by_local_name)**
- **[leda_unregister](#leda_unregister)**
- **[leda_online](#leda_online)**
- **[leda_offline](#leda_offline)**
- **[leda_report_properties](#leda_report_properties)**
- **[leda_report_event](#leda_report_event)**
- **[leda_get_tsl_size](#leda_get_tsl_size)**
- **[leda_get_tsl](#leda_get_tsl)**
- **[leda_get_config_size](#leda_get_config_size)**
- **[leda_get_config](#leda_get_config)**
- **[leda_feed_watchdog](#leda_feed_watchdog)**
- **[leda_get_device_handle](#leda_get_device_handle)**
- **[leda_exit](#leda_exit)**
- **[leda_get_module_name](#leda_get_module_name)**

---
<a name="get_properties_callback"></a>
``` c
/*
     * 获取属性的回调函数, Link IoT Edge 需要获取某个设备的属性时, SDK 会调用该接口间接获取到数据并封装成固定格式后回传给 Link IoT Edge.
     * 开发者需要根据设备id和属性名找到设备, 将属性值获取并以@device_data_t格式返回.
     *
     * @dev_handle:         Link IoT Edge 需要获取属性的具体某个设备.
     * @properties:         开发者需要将属性值更新到properties中.
     * @properties_count:   属性个数.
     * @usr_data:           注册设备时, 用户传递的私有数据.
     * 所有属性均获取成功则返回LE_SUCCESS, 其他则返回错误码(参考错误码宏定义).
     * */
typedef int (*get_properties_callback)(device_handle_t dev_handle, 
                                       leda_device_data_t properties[], 
                                       int properties_count, 
                                       void *usr_data);

```

---
<a name="set_properties_callback"></a>
``` c
/*
     * 设置属性的回调函数, Link IoT Edge 需要设置某个设备的属性时, SDK 会调用该接口将具体的属性值传递给应用程序, 开发者需要在本回调
     * 函数里将属性设置到设备.
     *
     * @dev_handle:         Link IoT Edge 需要设置属性的具体某个设备.
     * @properties:         Link IoT Edge 需要设置的设备的属性名和值.
     * @properties_count:   属性个数.
     * @usr_data:           注册设备时, 用户传递的私有数据.
     * 
     * 若获取成功则返回LE_SUCCESS, 失败则返回错误码(参考错误码宏定义).
     * */
typedef int (*set_properties_callback)(device_handle_t dev_handle, 
                                       const leda_device_data_t properties[], 
                                       int properties_count, 
                                       void *usr_data);

```

---
<a name="call_service_callback"></a>
``` c
/*
     * 服务调用的回调函数, Link IoT Edge 需要调用某个设备的服务时, SDK 会调用该接口将具体的服务参数传递给应用程序, 开发者需要在本回调
     * 函数里调用具体的服务, 并将服务返回值按照设备 tsl 里指定的格式返回. 
     *
     * @dev_handle:   Link IoT Edge 需要调用服务的具体某个设备.
     * @service_name: Link IoT Edge 需要调用的设备的具体某个服务名.
     * @data:         Link IoT Edge 需要调用的设备的具体某个服务参数, 参数与设备 tsl 中保持一致.
     * @data_count:   Link IoT Edge 需要调用的设备的具体某个服务参数个数.
     * @output_data:  开发者需要将服务调用的返回值, 按照设备 tsl 中规定的服务格式返回到output中.
     * @usr_data:     注册设备时, 用户传递的私有数据.
     * 
     * 若获取成功则返回LE_SUCCESS, 失败则返回错误码(参考错误码宏定义).
     * */
typedef int (*call_service_callback)(device_handle_t dev_handle, 
                                     const char *service_name, 
                                     const leda_device_data_t data[], 
                                     int data_count, 
                                     leda_device_data_t output_data[], 
                                     void *usr_data);

```
---
<a name="leda_init"></a>
``` c
/*
 * 模块初始化, 模块内部会创建工作线程池, 异步执行云端下发的指令, 工作线程数目通过worker_thread_nums配置.
 *
 * module_name:         模块名称.
 * worker_thread_nums : 线程池工作线程数.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_init(const char *module_name, int worker_thread_nums);

```

---
<a name="config_changed_callback"></a>
``` c
/*
 * 设备配置变更回调.
 *
 * module_name:   模块名称.
 * config:        配置信息.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
typedef int (*config_changed_callback)(const char *module_name, const char *config);

```

---
<a name="leda_register_config_changed_callback"></a>
``` c
/*
 * 注册设备配置变更回调.
 *
 * module_name:    模块名称.
 * config_cb:      配置变更通知回调接口.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,失败返回错误码.
 */
int leda_register_config_changed_callback(const char *module_name, config_changed_callback config_cb);

```

---
<a name="leda_register_and_online_by_device_name"></a>
``` c
/*
 * 通过已在云平台注册的device_name, 注册设备并上线设备, 申请设备唯一标识符.
 *
 * 设备默认注册后即上线.
 *
 * product_key: 产品pk.
 * device_name: 云平台注册的device_name, 可以通过配置文件导入.
 * device_cb:   设备回调函数结构体，详细描述@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在Link IoT Edge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_register_and_online_by_device_name(const char *product_key, const char *device_name, leda_device_callback_t *device_cb, void *usr_data);

```

---
<a name="leda_register_and_online_by_local_name"></a>
``` c
/*
 * 注册设备并上线设备, 申请设备唯一标识符, 若需要注册多个设备, 则多次调用该接口即可.
 *
 * 设备默认注册后即上线.
 *
 * product_key: 产品pk.
 * local_name:  由设备特征值组成的唯一描述信息, 必须保证同一个product_key时，每个待接入设备名称不同.
 * device_cb:   设备回调函数结构体，详细描述@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在Link IoT Edge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_register_and_online_by_local_name(const char *product_key, const char *local_name, leda_device_callback_t *device_cb, void *usr_data);

```

---
<a name="leda_unregister"></a>
``` c
/*
 * 注销设备, 解除设备和Link IoT Edge关联.
 *
 * dev_handle:  设备在Link IoT Edge本地唯一标识.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_unregister(device_handle_t dev_handle);

```

---
<a name="leda_online"></a>
``` c
/*
 * 上线设备, 设备只有上线后, 才能被 Link IoT Edge 识别.
 *
 * dev_handle:  设备在Link IoT Edge本地唯一标识.
 *
 * 阻塞接口,成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_online(device_handle_t dev_handle);

```

---
<a name="leda_offline"></a>
``` c
/*
 * 下线设备, 假如设备工作在不正常的状态或设备退出前, 可以先下线设备, 这样Link IoT Edge就不会继续下发消息到设备侧.
 *
 * dev_handle:  设备在Link IoT Edge本地唯一标识.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_offline(device_handle_t dev_handle);

```

---
<a name="leda_report_properties"></a>
``` c
/*
 * 上报属性, 设备具有的属性在设备能力描述 tsl 里有规定.
 *
 * 上报属性, 可以上报一个, 也可以多个一起上报.
 *
 * dev_handle:          设备在Link IoT Edge本地唯一标识.
 * properties:          @leda_device_data_t, 属性数组.
 * properties_count:    本次上报属性个数.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_report_properties(device_handle_t dev_handle, const leda_device_data_t properties[], int properties_count);

```

---
<a name="leda_report_event"></a>
``` c
/*
 * 上报事件, 设备具有的事件上报能力在设备 tsl 里有约定.
 *
 * 
 * dev_handle:  设备在Link IoT Edge本地唯一标识.
 * event_name:  事件名称.
 * data:        @leda_device_data_t, 事件参数数组.
 * data_count:  事件参数数组长度.
 *
 * 阻塞接口, 成功返回LE_SUCCESS,  失败返回错误码.
 *
 */
int leda_report_event(device_handle_t dev_handle, const char *event_name, const leda_device_data_t data[], int data_count);

```

---
<a name="leda_get_tsl_size"></a>
``` c
/*
 * 获取配置相关信息长度.
 *
 * product_key:   产品pk.
 *
 * 阻塞接口, 成功返回product_key对应的tsl长度, 失败返回0.
 */
int leda_get_tsl_size(const char *product_key);

```

---
<a name="leda_get_tsl"></a>
``` c
/*
 * 获取配置相关信息.
 *
 * product_key:  产品pk.
 * tsl:          输出物模型, 需要提前申请好内存传入.
 * size:         tsl长度, 该长度通过leda_get_tsl_size接口获取, 如果传入tsl比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_tsl(const char *product_key, char *tsl, int size);

```

---
<a name="leda_get_config_size"></a>
``` c
/*
 * 获取配置信息长度.
 *
 * module_name:  模块名称
 *
 * 阻塞接口, 成功返回config长度, 失败返回0.
 */
int leda_get_config_size(const char *module_name);

```

---
<a name="leda_get_config"></a>
``` c
/*
 * 获取配置信息.
 *
 * module_name:  模块名称
 * config:       配置信息, 需要提前申请好内存传入.
 * size:         config长度, 该长度通过leda_get_config_size接口获取, 如果传入config比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_config(const char *module_name, char *config, int size);

```

---
<a name="leda_feed_watchdog"></a>
``` c
/*
 * 喂看门狗.
 *
 * module_name: 模块名称.
 * thread_name: 需要保活的线程名称.
 * count_down_seconds : 倒计时时间, -1表示停止保活, 单位:秒.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_feed_watchdog(const char *module_name, const char *thread_name, int count_down_seconds);

```

---
<a name="leda_get_device_handle"></a>
``` c
/*
 * 获取设备句柄.
 *
 * product_key: 产品pk.
 * device_name: 设备名称.
 *
 * 阻塞接口, 成功返回device_handle_t, 失败返回小于0数值.
 */
device_handle_t leda_get_device_handle(const char *product_key, const char *device_name);

```

---
<a name="leda_exit"></a>
``` c
/*
 * 模块退出.
 *
 * 模块退出前, 释放资源.
 *
 * 阻塞接口.
 */
void leda_exit(void);
```

---
<a name="leda_get_module_name"></a>
``` c
/*
 * 获取驱动模块名称.
 *
 * 阻塞接口, 成功返回驱动模块名称, 失败返回NULL.
 */
char* leda_get_module_name(void);
```
