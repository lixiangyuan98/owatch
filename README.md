# owatch

一个RTP流服务器, 可从获取输入流(H264文件, 域套接字)并封装为RTP包发送

## Dependency

* [jrtplib](https://github.com/j0r1/JRTPLIB)

## Build

`make`

### Build Test

`make test`

测试程序的使用方法可查看相应程序的源码

### Build All

`make all` 编译主程序和所有测试程序

### Build for ARM

`make CROSS_COMPILE={your-cross-compile-toolchain-prefix}` 指定交叉编译工具链前缀

## Usage

`play.sdp`:

```plain
m=video 9000/2 RTP/AVP 96
a=rtpmap:96 H264/90000
```

使用[VLC](https://www.videolan.org/index.zh.html)打开`play.sdp`文件播放

## Note

### JRTPLIB的交叉编译

在`JRTPLIB`的源码目录下新建文件`toolchain.cmake`:

```plain
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER $(your-cross-compiler-for-C))
set(CMAKE_CXX_COMPILER $(your-cross-compiler-for-C++))

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

`CMakeLists.txt`中添加:

```plain
set(CMAKE_INSTALL_PREFIX $(path-to-your-cross-compile-toolchain-root)
set(CMAKE_FIND_ROOT_PATH $(path-to-your-cross-compile-toolchain-root))
```

添加后的`CMakeLists.txt`如下:

```plain
...

project(jrtplib)
set(VERSION_MAJOR 3)
set(VERSION_MINOR 11)
set(VERSION_DEBUG 1)
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_DEBUG}")

// 添加下面两行
set(CMAKE_INSTALL_PREFIX $(path-to-your-cross-compile-toolchain-root)
set(CMAKE_FIND_ROOT_PATH $(path-to-your-cross-compile-toolchain-root))

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
if (CMAKE_INSTALL_PREFIX AND NOT CMAKE_PREFIX_PATH)
    #message("Setting CMAKE_PREFIX_PATH to ${CMAKE_INSTALL_PREFIX}")
    file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" CMAKE_PREFIX_PATH)
endif()

...
```

JRTPLIB交叉编译默认目标平台是大端序，可改为小端序: `-DJRTPLIB_USE_BIGENDIAN=OFF`

执行`cmake . -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DJRTPLIB_USE_BIGENDIAN=OFF`

然后`make && sudo make install`

### timestampinc的计算

假设采样率为90000Hz, 帧率为24FPS, 则timestamp为90000/24=3750

### Linux网络栈参数修改

使用`Unix Domain Socket`时，不需要考虑网络延迟、丢包等问题，可以发送较大的包，这时候Linux设置的Buffer Size可能会导致发送包失败(EMSGSIZE)，可以调整Linux网络参数使较大的包能够成功发送:

查看默认和最大的发送数据包大小:

```shell
$ cat /proc/sys/net/core/wmem_default
$ cat /proc/sys/net/core/wmem_max
```

查看默认和最大的接收数据包大小

```shell
$ cat /proc/sys/net/core/rmem_default
$ cat /proc/sys/net/core/rmem_max
```

可在`/etc/sysctl.conf`中修改以上参数:

```plain
net.core.wmem_default=1048576
net.core.wmem_max=1048576
net.core.rmem_default=1048576
net.core.rmem_max=1048576
```

使修改生效:

```shell
$ sysctl -p
```