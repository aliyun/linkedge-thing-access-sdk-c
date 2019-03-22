# Link IoT Edge设备接入SDK C语言版
本项目提供一个设备接入SDK C版本库，方便用户在[Link IoT Edge](https://help.aliyun.com/product/69083.html?spm=a2c4g.11186623.6.540.7c1b705eoBIMFA)上编写驱动以接入设备。

## 快速开始 - demo_led
`demo_led`示例演示将设备接入Link IoT Edge的全过程。

### demo编译演示准备
接入SDK编译条件和demo编译演示准备见[编译演示准备](docs/develop-guide.md)

### demo编译
``` sh
    $git clone git@github.com:aliyun/linkedge-thing-access-sdk-c.git
    
    $cd linkedge-thing-access-sdk-c

    $make prepare              #预编译生成外部依赖库

    $make && make install      #生成leda_sdk_c和demo
```

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
- **[leda_online](#leda_online)**
- **[leda_offline](#leda_offline)**
- **[leda_report_properties](#leda_report_properties)**
- **[leda_report_event](#leda_report_event)**
- **[leda_get_tsl_size](#leda_get_tsl_size)**
- **[leda_get_tsl](#leda_get_tsl)**
- **[leda_get_config_size](#leda_get_config_size)**
- **[leda_get_config](#leda_get_config)**
- **[leda_get_device_handle](#leda_get_device_handle)**
- **[leda_exit](#leda_exit)**
- **[leda_get_module_name](#leda_get_module_name)**

---
<a name="get_properties_callback"></a>
``` c
 /*
     * 获取属性（对应设备产品物模型tsl的属性定义）的回调函数, 需驱动开发者实现获取属性业务逻辑
     * 
     * LinkEdge 需要获取某个设备的属性时, SDK 会调用该接口间接获取到数据并封装成固定格式后回传给 LinkEdge.
     * 开发者需要根据设备id和属性名找到设备, 将获取到的属性值按照@device_data_t格式填充.
     *
     * @dev_handle:         LinkEdge 需要获取属性的具体某个设备.
     * @properties:         属性值键值结构, 驱动开发者需要将根据属性名称获取到的属性值更新到properties中.
     * @properties_count:   属性个数.
     * @usr_data:           注册设备时, 用户传递的私有数据.
     * 所有属性均获取成功则返回LE_SUCCESS, 其他则返回错误码(参考le_error.h错误码宏定义).
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
     * 设置属性（对应设备产品物模型tsl的属性定义）的回调函数, 需驱动开发者实现设置属性业务逻辑
     * 
     * LinkEdge 需要设置某个设备的属性时, SDK 会调用该接口将具体的属性值传递给应用程序, 开发者需要在本回调
     * 函数里将属性设置到设备.
     *
     * @dev_handle:         LinkEdge 需要设置属性的具体某个设备.
     * @properties:         LinkEdge 需要设置的设备的属性名称和值.
     * @properties_count:   属性个数.
     * @usr_data:           注册设备时, 用户传递的私有数据.
     * 
     * 若获取成功则返回LE_SUCCESS, 失败则返回错误码(参考le_error.h错误码宏定义).
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
     * 服务（对应设备产品物模型tsl的服务定义）调用的回调函数, 需要驱动开发者实现服务对应业务逻辑
     * 
     * LinkEdge 需要调用某个设备的服务时, SDK 会调用该接口将具体的服务参数传递给应用程序, 开发者需要在本回调
     * 函数里调用具体的服务, 并将服务返回值按照@device_data_t格式填充到output_data. 
     *
     * @dev_handle:   LinkEdge 需要调用服务的具体某个设备.
     * @service_name: LinkEdge 需要调用的设备的具体某个服务名, 名称与设备产品物模型tsl一致.
     * @data:         LinkEdge 需要调用的设备的具体某个服务参数, 参数与设备产品物模型tsl保持一致.
     * @data_count:   LinkEdge 需要调用的设备的具体某个服务参数个数.
     * @output_data:  开发者需要将服务调用的返回值, 按照设备产品物模型tsl规定的服务格式返回到output中.
     * @usr_data:     注册设备时, 用户传递的私有数据.
     * 
     * 若获取成功则返回LE_SUCCESS, 失败则返回错误码(参考le_error.h错误码宏定义).
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
 * 驱动模块初始化, 模块内部会创建工作线程池, 异步执行阿里云物联网平台下发的设备操作请求, 工作线程数目通过worker_thread_nums配置.
 *
 * module_name:         驱动模块名称.
 * worker_thread_nums : 线程池工作线程数, 该数值根据注册设备数量设置.
 *
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_init(const char *module_name, int worker_thread_nums);

```

---
<a name="config_changed_callback"></a>
``` c
/*
 * 驱动配置变更回调接口.
 *
 * module_name:   驱动模块名称.
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
 * 订阅驱动配置变更监听回调.
 *
 * module_name:    驱动模块名称.
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
 * 通过已在阿里云物联网平台创建的设备device_name, 注册并上线设备, 申请设备唯一标识符.
 *
 * 若需要注册多个设备, 则多次调用该接口即可.
 *
 * product_key: 在阿里云物联网平台创建的产品pk.
 * device_name: 在阿里云物联网平台创建的dn.
 * device_cb:   请求调用设备回调函数结构体，详细描述见@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 */
device_handle_t leda_register_and_online_by_device_name(const char *product_key, const char *device_name, leda_device_callback_t *device_cb, void *usr_data);

```

---
<a name="leda_register_and_online_by_local_name"></a>
``` c
/*
 * 通过本地自定义设备名称, 注册并上线设备, 申请设备唯一标识符.
 *
 * 若需要注册多个设备, 则多次调用该接口即可.
 *
 * product_key: 在阿里云物联网平台创建的产品pk.
 * local_name:  由设备特征值组成的唯一描述信息, 必须保证同一个product_key时，每个设备名称不同.
 * device_cb:   请求调用设备回调函数结构体，详细描述见@leda_device_callback.
 * usr_data:    设备注册时传入私有数据, 在回调中会传给设备.
 *
 * 阻塞接口, 返回值设备在linkedge本地唯一标识, >= 0表示有效, < 0 表示无效.
 *
 * 注: 在同一pk条件设备注册, 不允许本接口和leda_register_and_online_by_device_name接口同时使用, 
 *     即每一个pk设备注册必须使用同一接口, 否则设备注册会发生不可控行为.
 */
device_handle_t leda_register_and_online_by_local_name(const char *product_key, const char *local_name, leda_device_callback_t *device_cb, void *usr_data);

```

---
<a name="leda_online"></a>
``` c
/*
 * 上线设备, 设备只有上线后, 才能被 LinkEdge 识别.
 *
 * dev_handle:  设备在linkedge本地唯一标识.
 *
 * 阻塞接口,成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_online(device_handle_t dev_handle);

```

---
<a name="leda_offline"></a>
``` c
/*
 * 下线设备, 假如设备工作在不正常的状态或设备退出前, 可以先下线设备, 这样LinkEdge就不会继续下发消息到设备侧.
 *
 * dev_handle:  设备在linkedge本地唯一标识.
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
 * 上报属性, 设备具有的属性在设备能力描述在设备产品物模型tsl规定.
 *
 * 上报属性, 可以上报一个, 也可以多个一起上报.
 *
 * dev_handle:          设备在linkedge本地唯一标识.
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
 * 上报事件, 设备具有的事件上报能力在设备产品物模型tsl有规定.
 *
 * 
 * dev_handle:  设备在linkedge本地唯一标识.
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
 * 获取指定pk对应tsl(物模型)内容长度.
 *
 * product_key:   产品pk.
 *
 * 阻塞接口, 成功返回product_key对应的tsl内容长度, 失败返回0.
 */
int leda_get_tsl_size(const char *product_key);

```

---
<a name="leda_get_tsl"></a>
``` c
/*
 * 获取指定pk对应tsl(物模型)内容.
 *
 * product_key:  产品pk.
 * tsl:          物模型内容, 需要提前申请好内存传入.
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
 * 获取驱动配置信息长度.
 *
 * module_name:  驱动模块名称
 *
 * 阻塞接口, 成功返回config长度, 失败返回0.
 */
int leda_get_config_size(const char *module_name);

```

---
<a name="leda_get_config"></a>
``` c
/*
 * 获取驱动配置信息.
 *
 * module_name:  驱动模块名称
 * config:       配置信息, 需要提前申请好内存传入.
 * size:         config长度, 该长度通过leda_get_config_size接口获取, 如果传入config比实际配置内容长度短, 会返回LE_ERROR_INVAILD_PARAM.
 *  
 * 阻塞接口, 成功返回LE_SUCCESS, 失败返回错误码.
 */
int leda_get_config(const char *module_name, char *config, int size);

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
 * 驱动模块退出.
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
 * 该接口获取在阿里云物联网平台上传驱动时填写的驱动模块名称
 *
 * 阻塞接口, 成功返回驱动模块名称, 失败返回NULL.
 */
char* leda_get_module_name(void);
```
