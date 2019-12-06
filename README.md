# owatch

一个RTP流服务器, 可从获取输入流(H264文件, 域套接字)并封装为RTP包发送

## Build

`make`

### Build Test

`make test`

测试程序的使用方法可查看相应程序的源码

## Usage

使用[VLC](https://www.videolan.org/index.zh.html)打开`play.sdp`文件播放

## Dependency

* [jrtplib](https://github.com/j0r1/JRTPLIB)
