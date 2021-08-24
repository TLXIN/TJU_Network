#include "debug.h"

void display_pkt(char* pkt){
    printf("==========================================\n");
    printf("源端口：%d          " , get_src(pkt));
    printf("目的端口：%d \n",get_dst(pkt));
    printf("seq：%d \n",get_seq(pkt));
    printf("ack：%d \n",get_ack(pkt));
    printf("hlen: %d        ",get_hlen(pkt));
    printf("plen: %d\n",get_plen(pkt));
    printf("FALG = %d\n",get_flags(pkt));
    printf("FLAG: FIN = %d , ACK = %d , SYN = %d\n",
            ((get_flags(pkt)&FIN_FLAG_MASK)>>1) , ((get_flags(pkt)&ACK_FLAG_MASK)>>2) , ((get_flags(pkt)&SYN_FLAG_MASK)>>3));
    printf("ADV_window：%d\n",get_advertised_window(pkt));
    printf("ext: %d \n",get_ext(pkt));
    printf("==========================================\n");
}

void print_SKB(SKB_HEAD* head){
    printf("==========================================\n");
    printf("HEAD NODE \n");
    printf("block_number：%d\n",head->block_number);
    printf("total_Size：%d\n",head->total_size);
    printf("\n");
    if(head -> next != NULL){
        SKB* node = head -> next;
        while (node != NULL)
        {
            printf("%d -> ",node->len);
            node = node -> next;
        }
    }
    printf("\n");
    printf("==========================================\n");
}
