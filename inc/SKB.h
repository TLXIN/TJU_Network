#ifndef _SKB_H_
#define _SKB_H_
#include <stdint.h>

typedef struct SKB
{
    struct SKB* next;
    uint32_t len;
    char* data;
} SKB;

typedef struct 
{
    SKB* next;
    //这里指示下一个要发送的包的序列号
    //初始化的时候，该值为 1，默认已经建立连接 
    /*客户端：   发送SYN 后 设置seq = 1；
                接收SYN+ACK后，发送 seq = 1
                然后就可以正常累加
                发送长为 len 的包 后 
                seq = seq + len ，表示下一次从 新的seq开始发送

     服务器端：  初始置为0
                接收SYN后，发送seq = 0
                然后进入正常传输阶段*/
    
    uint32_t seq; 
    int block_number;
    int total_size;

} SKB_HEAD ;



#endif