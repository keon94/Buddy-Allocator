#include <stdio.h>
#include "buddy.h"

#define TEST1 0
#define TEST2 1


unsigned int *b_alloc(unsigned int kbytes){
    unsigned int *addr = buddy_alloc(1024*kbytes);
    buddy_dump();
    return addr;
}

void b_free(unsigned int *addr){
    buddy_free(addr);
    buddy_dump();
}



//given tests
void test1(){
	
    printf("******************************TEST 1 Part 1******************************\n");
    
    buddy_init();
	unsigned int *addr1, *addr2, *addr3, *addr4;
    addr1 = addr2 = addr3 = addr4 = NULL;
    
    addr1 = b_alloc(80);
    addr2 = b_alloc(60);
    addr3 = b_alloc(80);
    b_free(addr1);
    addr4 = b_alloc(32);
    b_free(addr2);
    b_free(addr4);
    b_free(addr3);

    printf("******************************TEST 1 Part 2******************************\n");
    
    addr1 = b_alloc(44);
    b_free(addr1);
    
	
}


//custom tests
void test2(){
	unsigned int *addr1, *addr2, *addr3, *addr4;
    addr1 = addr2 = addr3 = addr4 = NULL;
    buddy_init();
    #if 1
    addr1 = b_alloc(10);
    addr2 = b_alloc(1);
    addr3 = b_alloc(100);
    addr4 = b_alloc(1);
    
    b_free(addr1);
    b_free(addr2);
    b_free(addr3);
    b_free(addr4);
    #endif

    #if 1
        addr1 = b_alloc(32);
        b_free(addr1);
    #endif
    
}


int main(){
    
    #if TEST1
        test1();
    #endif
    #if TEST2
        test2();
    #endif

}