# TJU_Network
天津大学网络实践
## 2021.8.19
写了一个TCP三次握手的框架，不能处理丢包，没有计时器。
listen socket的半连接（syns_queue）和已建立连接（accept_queue）没有使用队列实现
只用了定长数组作为简易实现
