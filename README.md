# owatch

一个RTP流服务器, 可从获取输入流(H264文件, 域套接字)并封装为RTP包发送

## Build

`make`

### Build Test

`make test`

测试程序的使用方法可查看相应程序的源码

### Build All

`make all` 编译主程序和所有测试程序

## Usage

`play.sdp`:

```plain
m=video 9000/2 RTP/AVP 96
a=rtpmap:96 H264/90000
```

使用[VLC](https://www.videolan.org/index.zh.html)打开`play.sdp`文件播放

## Note

### timestampinc的计算

假设采样率为90000Hz, 帧率为24FPS, 则timestamp为90000/24=3750

## Dependency

* [jrtplib](https://github.com/j0r1/JRTPLIB)
