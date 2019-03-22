# 设备接入SDK编译和驱动开发流程

## 准备操作系统

建议使用 Ubuntu 16.04 (x86_64) 或 CentOS 7.0 (x86_64) 进行开发。如果当前系统与这两个系统不一致，建议使用虚拟机安装这两个系统，虚拟机可以使用免费虚拟机软件[VirtualBox](https://www.virtualbox.org/)

`注`：不建议使用 DockerHub 中的相应镜像

### Ubuntu 16.04 (x86_64)

镜像可以从这里[下载](http://releases.ubuntu.com/xenial/ubuntu-16.04.5-desktop-amd64.iso)，关于不同版本的介绍见[链接](http://releases.ubuntu.com/xenial/)

### CentOS 7.0 (x86_64)

镜像可以从这里[下载](http://vault.centos.org/7.0.1406/isos/x86_64/CentOS-7.0-1406-x86_64-DVD.iso)，关于不同版本的介绍见[链接](https://wiki.centos.org/Download)


## 编译leda_sdk_c库

### 依赖环境
本工程需要的编译工具版本保证和表格中列举版本一致或更高版本，否则将编译可能会失败

Tool           | Version |
---------------|---------|
gcc            | 4.8.5+  |
make           | 3.82+   |
ld             | 2.17+   |
cmake          | 3.11.1+ |
autoconf       | 2.69+   |
libtool        | 2.4.6+  |
zip            | 3.0+    |

### 依赖组件
本工程依赖库的版本要保证和表格中列举版本一致或更高版本，否则将编译可能会失败

Componet       | Version |
---------------|---------|
cjson          | 1.5.5+  |
expat          | 2.2.0+  |
dbus           | 1.2.1+  |

### 编译步骤
``` sh
    $git clone git@github.com:aliyun/linkedge-thing-access-sdk-c.git
    
    $cd linkedge-thing-access-sdk-c
    
    $make prepare              #生成leda_sdk_c需要的依赖库

    $make && make install      #生成leda_sdk_c库和demo
```

### 交叉编译
如果需要交叉编译设备接入SDK，需要将Makefile脚本中注释部分打开，选择填写编译目标结构和编译工具链信息：

``` makefile
     # 编译目标Arch选择
     export HOST="x86_64"
     export ARCH="arm-linux-gnueabihf"
     export TARGET=${ARCH}

      # 编译工具链选择
     export CROSS_ROOT= #the root dir of cross compile tool chain 
     export CROSS_COMPILE=${CROSS_ROOT}/bin/arm-linux-gnueabihf-
     export CC=${CROSS_COMPILE}gcc
     export CXX=${CROSS_COMPILE}g++
     export LD=${CROSS_COMPILE}ld
     export AR=${CROSS_COMPILE}ar
     export RANLIB=${CROSS_COMPILE}ranlib
     export STRIP=${CROSS_COMPILE}strip
```

## 驱动开发流程
驱动开发流程见[驱动开发指南](https://help.aliyun.com/document_detail/104444.html?spm=a2c4g.11186623.6.562.7aaa8f08JPJf2d)
