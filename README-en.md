[中文](README.md)|[English](README-en.md)

# `Notes for thing access sdk version update:`
1. The thing access SDK was refactored in the [v1.0.0](https://github.com/aliyun/linkedge-thing-access-sdk-c/releases) version, and some interfaces are not compatible with the old version interface. Drivers developed for SDKs prior to v1.0.0 may not be able to be directly upgraded using v1.0.0;
2. The driver developed using v1.0.0 version SDK supports [Link IoT Edge version](https://help.aliyun.com/document_detail/120995.html?spm=a2c4g.11186623.6.553.1ffe4224dxnjxT) 1.8.1 to the latest version ;
3. Drivers developed before v1.0.0 support [Link IoT Edge version](https://help.aliyun.com/document_detail/120995.html?spm=a2c4g.11186623.6.553.1ffe4224dxnjxT) 1.8.1 to the latest version , But it is not recommended to continue to develop new drivers using this version of the SDK;

# Link IoT Edge thing access SDK C language version

This project provides a thing access SDK (C language version) for developers to write on [Link IoT Edge](https://help.aliyun.com/product/69083.html?spm=a2c4g.11186623.6.540.7c1b705eoBIMFA) thing access driver.

## Compiler Environment

We recommend using Ubuntu 16.04 (x86_64) or CentOS 7.0 (x86_64) for development. If the current system is inconsistent with these two systems, it is recommended to use a virtual machine to install the two systems. The virtual machine can use the free virtual machine software [VirtualBox](https://www.virtualbox.org/)

`Note`: It is not recommended to use the corresponding image in DockerHub

### Ubuntu 16.04 (x86_64)
The image can be downloaded from [here](http://releases.ubuntu.com/xenial/ubuntu-16.04.6-desktop-amd64.iso). For the introduction of different versions, see [link](http://releases.ubuntu.com/xenial/)

### CentOS 7.0 (x86_64)

The image can be downloaded from [here](http://vault.centos.org/7.0.1406/isos/x86_64/CentOS-7.0-1406-x86_64-DVD.iso). For the introduction of different versions, see [link](http://wiki.centos.org/Download)

### Cross compilation

If you need to cross-compile the thing to access the SDK, you need to fill in the compilation target structure and compilation tool chain information in the build-cross-compile.sh script:

```sh
      # Build target Arch selection
      export HOST = x86_64
      export ARCH = arm-linux-gnueabihf
      export TARGET = $ {ARCH}

      # Build toolchain selection
      export CROSS_ROOT = # the root dir path of cross compile tool-chain
      export CROSS_COMPILE = $ {CROSS_ROOT}/bin/arm-linux-gnueabihf-
      export CC = $ {CROSS_COMPILE} gcc
      export CXX = $ {CROSS_COMPILE} g ++
      export LD = $ {CROSS_COMPILE} ld
      export AR = $ {CROSS_COMPILE} ar
      export RANLIB = $ {CROSS_COMPILE} ranlib
      export STRIP = $ {CROSS_COMPILE} strip
```

## Compile dependencies

### Dependencies

The compilation tool version required for this project is guaranteed to be the same as or higher than the version listed in the table, otherwise the compilation may fail

Tool           | Version |
---------------|---------|
gcc            | 4.8.5+  |
make           | 3.82+   |
ld             | 2.17+   |
cmake          | 3.11.1+ |
autoconf       | 2.69+   |
libtool        | 2.4.6+  |
zip            | 3.0+    |

### Dependent Components

The version of the dependent libraries of this project must be consistent with the version listed in the table or higher, otherwise the compilation may fail, and the source code of the dependent components is under the project deps folder.

Componet       | Version |
---------------|---------|
cjson          | 1.5.5+  |
expat          | 2.2.0+  |
dbus           | 1.2.1+  |

## Quick demo

`demo_led` example demonstrates the process of connecting a device to the Link IoT Edge.

### demo compilation

```sh
     $ git clone git@github.com:aliyun/linkedge-thing-access-sdk-c.git

     $ cd linkedge-thing-access-sdk-c

     $ make prepare         # Pre-compile and generate external dependencies

     $ make && make install #Generate leda_sdk_c and demo
```

### demo

1. Go to the project directory build/bin/demo/demo_led.
2. Find the led_driver.zip compression package.
3. Enter the Link IoT Edge console, ** Edge Compute **, ** Driver Management **, ** New Driver **.
4. Select the language type * c *. The CPU architecture is selected according to the compilation environment.
5. Set the driver name to `led_driver` and upload the previously prepared zip package.
6. Create a product named `demo_led`. The product contains a `temperature` property (int32 type) and a` high_temperature` event (int32 type and an int32 input parameter named `temperature`).
7. Create a device for the above product named `demo_led`.
8. Create a new edge instance and add the Link IoT Edge gateway device to the edge instance.
9. Enter the device and driver configuration page and  assign the `led_driver` driver.
10. Assign the `demo_led` device to the` led_driver` driver.
11. Enter the message routing page and add * message routing * with the following configuration:
  * Source: `demo_led` device
  * TopicFilter: property
  * Message destination: IoT Hub
12. Deploy the edge instance. `demo_led` device will report attributes to the cloud plateform every 5 seconds, which can be viewed on the Link IoT Edge console device running status page.

## Development Documentation

See [Development Document](https://help.aliyun.com/document_detail/104444.html?spm=a2c4g.11186623.6.562.7aaa8f08JPJf2d) for the complete driver development process

