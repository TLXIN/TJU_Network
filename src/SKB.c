#include "SKB.h"
#include "global.h"
#include "kernel.h"

int add_to_skb(tju_tcp_t* sock , const void* buffer , int len){
    char* data = malloc(len);
    memcpy(data, buffer, len);
    while (1){
        printf("attemp to add skb \n");
        if(sock -> send_buf_head -> total_size + len <= TCP_SENDWN_SIZE){
            SKB* new_skb = (SKB*)malloc(sizeof(SKB));
            new_skb -> next = NULL;
            new_skb -> len = len;
            new_skb -> data = data;
            if(sock -> send_buf_head -> next != NULL){
                SKB* tail =  sock -> send_buf_head -> next;
                while (tail -> next != NULL)
                {
                    tail = tail -> next;
                }
                tail -> next = new_skb;
            } 
            else sock -> send_buf_head -> next = new_skb;
            sock -> send_buf_head -> block_number += 1;
            sock -> send_buf_head -> total_size += len;
            return len;
        }
    }
}

void* send_thread_func(void* arg){
    #ifdef DEBUG_SEND_THREAD
        printf("发送线程建立完成\n");
        int sock_index = *((int*)arg);
        printf("绑定的socket的hash是：%d \n",sock_index);
    #endif

    tju_tcp_t* this_sock;
    if(listen_socks[sock_index] != NULL)
    {   
        #ifdef DEBUG_SEND_THREAD
            printf("绑定监听socket\n");
        #endif
        this_sock = listen_socks[sock_index];
    }
    else if (established_socks[sock_index] != NULL)
    {
        #ifdef DEBUG_SEND_THREAD
            printf("绑定建立socket\n");
        #endif
        this_sock = established_socks[sock_index];
    }
    else{
        printf("此发送线程的sokcet未注册\n");
        exit(-1);
    }
    while (1) //循环发送SKB的内容
    {
        if(this_sock -> send_buf_head -> block_number != 0 
                && this_sock -> send_buf_head -> next != NULL){
            //skb缓存不为空       
            SKB* send_block;
            send_block = this_sock -> send_buf_head -> next;
            char* send_pkt = create_packet_buf(this_sock -> established_local_addr.port , this_sock -> established_remote_addr.port
                                                , this_sock -> send_buf_head -> seq , 0 
                                                , DEFAULT_HEADER_LEN , DEFAULT_HEADER_LEN + send_block -> len 
                                                , 0 , 0 , 0 ); 
            sendToLayer3(send_pkt , DEFAULT_HEADER_LEN + send_block -> len);
            this_sock -> send_buf_head -> seq += send_block -> len;
            this_sock -> send_buf_head -> next = send_block -> next;
            this_sock -> send_buf_head -> block_number -= 1;
            this_sock -> send_buf_head -> total_size -= send_block -> len;

        }
        
    }
    
}

