#ifndef _SKB_H_
#define _SKB_H_

#include "global.h"

typedef struct 
{
    SKB* next;
    uint32_t len;
    uint32_t seq;
    char* data;
} SKB;

typedef struct 
{
    SKB* next;
} SKB_HEAD ;


 



#endif