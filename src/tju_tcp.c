#include "tju_tcp.h"
#include "debug.h"
/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = NULL;
    sock->sending_len = 0;

    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = NULL;
    sock->received_len = 0;
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    sock->window->wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));
    sock->window->wnd_recv = (receiver_window_t)malloc(sizeof(receiver_window_t));

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
注册该socket到socks_queue哈希表
*/
int tju_listen(tju_tcp_t* sock){
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    tju_sock_queue* new_queue = (tju_sock_queue*)malloc(sizeof(tju_sock_queue));
    socks_queue[hashval] = new_queue;
    listen_socks[hashval] = sock;
    printf("监听socket的hashval是：%d \n",hashval);
    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
//我觉得这里应该用多线程,始终起一个线程来循环调用accept函数
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    int listen_hashval = cal_hash(listen_sock ->bind_addr.ip , listen_sock -> bind_addr.port
                                    ,0,0);
    printf("accept hashval is : %d \n",listen_hashval);
    while (1)
    {
    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 
    if(socks_queue[listen_hashval] -> accept_queue[0] != NULL){
            //拿出一个socket(已经建立好连接)
            tju_tcp_t* return_socket = socks_queue[listen_hashval] -> accept_queue[0];
            int return_sock_hashval = cal_hash( return_socket -> established_local_addr.ip , return_socket -> established_local_addr.port
                                            ,return_socket -> established_remote_addr.ip , return_socket -> established_remote_addr.port);
            printf("建立在服务器上的Socket的hash为： %d \n",return_sock_hashval);
            //把这个socket注册到EST里面,这里的hash为新socket的hash
            established_socks[return_sock_hashval] = return_socket;
            socks_queue[listen_hashval] -> accept_queue[0] = NULL;
            
            return return_socket;
        }
    }
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){

    sock->established_remote_addr = target_addr;

    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

    //这里要把刚刚建立的socket放入est——hash中，尽管现在还没有及建立连接
    //但是。只有这样才能让socket可以收到数据

    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;
    printf("客户端注册端口 %d \n",hashval);
    // 这里也不能直接建立连接 需要经过三次握手
    // 实际在linux中 connect调用后 会进入一个while循环
    // 循环跳出的条件是socket的状态变为ESTABLISHED 表面看上去就是 正在连接中 阻塞
    // 而状态的改变在别的地方进行 在我们这就是tju_handle_packet

    //使用UDP发送SYN报文 SYN = 1 ，ACK = 0 ， seq = 随机（取1000）
    //随后进入SYN_SENT
    char* shakehand1;
    shakehand1 = create_packet_buf(local_addr.port , target_addr.port 
                                    , get_ISN(1000) , 0 , DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN
                                    , SYN , 0 , 0
                                    , NULL ,0);
    sock->window->wnd_send.nextseq = get_ISN(1000) + 1 ;

    sendToLayer3( shakehand1 , 20 );
    sock->state = SYN_SENT;
    printf("send shakehand1 SYN \n");

    while(1){
        if(sock -> state == ESTABLISHED){
        printf("ESTABLISHED!!!\n");
        printf("建立在客户端上的Socket的hash为： %d \n",cal_hash(sock -> established_local_addr.ip , sock -> established_local_addr.port
                                                                ,sock -> established_remote_addr.ip , sock -> established_remote_addr.port));
        return 0;
        }
    }
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = 464;
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

    sendToLayer3(msg, plen);

    return 0;
}
int  tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len<=0){
        // 阻塞
    }
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0;
    if (sock->received_len >= len){ // 从中读取len长度的数据
        read_len = len;
    }else{
        read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len);
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
        free(sock->received_buf);
        sock->received_len -= read_len;
        sock->received_buf = new_buf;
    }else{ //清空接受缓冲区
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

    // 把收到的数据(pkt)放到接受(socket)缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL){
        sock->received_buf = malloc(data_len);
    }else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁
    int hashval = cal_hash(sock->established_local_addr.ip,sock->established_local_addr.port
                                    ,sock->established_remote_addr.ip , sock->established_remote_addr.port);

    int listen_hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    //这里要进行状态的转换
    //display_pkt(pkt);
    switch (sock -> state)
    {
    case LISTEN: //期望收到SYN = 1 ACK = 0
        if( is_SYN(pkt) && (!is_ACK(pkt)) ){ 
            //这里要建立一个新的socket，并放入半连接队列中
            printf("接受到s1\n");
            sock->state = SYN_RECV;
            
            //新建一个socket（半连接）
            tju_tcp_t* new_sock = tju_socket();
            new_sock -> established_remote_addr.ip   = inet_network("10.0.0.2");
            new_sock -> established_remote_addr.port = get_src(pkt);
            new_sock -> established_local_addr.ip    = inet_network("10.0.0.1");
            new_sock -> established_local_addr.port  = get_dst(pkt);

            new_sock -> state = SYN_RECV;
            printf("新socket创建完成~\n");

            //放入半连接队列(使用监听socket的hash)
            socks_queue[listen_hashval] -> syns_queue[0] = new_sock;

            //发送SYN = 1 ACK = 1 seq = 2000 ack = x+1
            int new_ack = get_seq(pkt) + 1;
            char* shakehand2;
            //源和目的反过来
            shakehand2 = create_packet_buf(  get_dst(pkt) , get_src(pkt)
                                            , get_ISN(2000) , new_ack , DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN
                                            ,SYN_ACK, 0 , 0
                                            ,NULL ,0);
            sendToLayer3( shakehand2 , 20);
            printf("发送shakehand2 \n");
        }
        break;

    case SYN_SENT: //这是客户端的状态
        if(is_SYN(pkt) && is_ACK(pkt) ){ //状态判断不完全
            printf("客户端收到S2\n");
            sock->state = ESTABLISHED;
            int new_ack = get_seq(pkt) + 1;
            char* shakehand3; 
            //上一个包的ACK就是这个的seq
            shakehand3 = create_packet_buf(  get_dst(pkt) , get_src(pkt)
                                            , get_ack(pkt) , new_ack , DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN
                                            , ACK , 0 , 0
                                            , NULL ,0);
            sendToLayer3(shakehand3 , 20);
            printf("发送shakehand3 \n");
        }
        break;

    case SYN_RECV:
        if(is_ACK(pkt)){//状态判断不完全
            //这里要把建立好的socket放入accept_queue里
            //监听socket应该还是回到 listen状态
            socks_queue[listen_hashval] -> accept_queue[0] = socks_queue[listen_hashval] -> syns_queue[0];
            socks_queue[listen_hashval] -> syns_queue[0] = NULL;
            socks_queue[listen_hashval] -> accept_queue[0] -> state = ESTABLISHED;
            printf("服务器socket建立完成\n");
        }
    default:
        break;
    }

    return 0;
}

int tju_close (tju_tcp_t* sock){
    char* close1;
    /*close1 = create_packet_buf(sock->established_local_addr.port , sock->established_remote_addr.port
                                ,2000 , )*/
    return 0;
}