/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"
#include <stdbool.h>

#include <math.h>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)	//2^12 = 4096 = 4k
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

#define BLOCK_ORDER_TO_FIRST_PAGE(block_order) ( (1<<(MAX_ORDER - block_order)) - 1 )

#define PAGE_TO_BLOCK_ORDER(page_idx) ( MAX_ORDER - (int)(log2f(page_idx + 1)) )

/* find buddy address */  //Block order : o
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int max_alloc;
	bool available;
	int index;
	/* TODO: DECLARE NECESSARY MEMBER VARIABLES */
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[1<<MAX_ORDER]; //2^20 B -- 2^12*2^8 = 256 blocks of 4K total

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE]; //2^8 = 256

//static int largest_available_block_order_index = MAX_ORDER;

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

 //rounds up x (in bytes) to the next power of 2, if not already a power of 2
int roundup2(int x){
	float log2x = log2f((float)x);
	if(log2x != (int)log2x)
		return 1<<(int)ceil(log2x);
	return x;
}

void test(){
	
	int *addr1, *addr2, *addr3;
	printf("\n");
	printf("to alloc 80k\n");
	addr1 = buddy_alloc(80*1024);
	buddy_dump();
	printf("to alloc 60k\n");
	addr2 = buddy_alloc(60*1024);
	buddy_dump();
	printf("to alloc 80k\n");
	addr3 = buddy_alloc(80*1024);
	buddy_dump();
	printf("\n");
	buddy_free(addr1);
	buddy_free(addr2);
	buddy_free(addr3);

	
}

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;//2^20/2^12 = 256
	//printf("hi\n");
	int div = 1, max_allocable_bytes = 1<<MAX_ORDER;
	for (i = 0; i < n_pages; i++) {
		/* TODO: INITIALIZE PAGE STRUCTURES */
		if(i / div > 0)
			div = div*2;
		INIT_LIST_HEAD(&g_pages[i].list);
		g_pages[i].max_alloc = max_allocable_bytes/div;  // alot of these fields are redundant as it seems, might remove later
		g_pages[i].available = true;
		g_pages[i].index = i;
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) { //12 to 20
		INIT_LIST_HEAD(&free_area[i]);  //each free area's head is initialised 
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]); 
	
	//for test only, remove later
	test();
	exit(0);
}


//inquires the free area for the available block order that is closest to the desired order (target block order)
//returns an int (the block number)
int request_closest_free_block_order(int block_order){
	if(block_order > MAX_ORDER){
		return -1;
	}
	if(list_empty(&free_area[block_order])){
		return request_closest_free_block_order(block_order+1);
	}
	else{
		return block_order;
	}
}


//order <-> ORDER

void* recursive_alloc(int present_block_order, int target_block_order){ //page offset: the offset of the page in a given block order
	
	struct list_head* page_node;
	int page_index;
	void* mem_addr = NULL;

	list_for_each( page_node, &free_area[present_block_order] ){
		//traversal in the list happens for each page_list that does exist
		//page_index = BLOCK_ORDER_TO_FIRST_PAGE(present_block_order) + page_offset;
		page_index = (int)((page_t*)page_node - &g_pages[0]);
		
		if(present_block_order == target_block_order){ //we've reached the target block order
			mem_addr = PAGE_TO_ADDR(page_index);  //page index = block_order_init_index + page_offset
			printf("allocd to addr %p, page index %d in block %d, \n", (int*)mem_addr, page_index, present_block_order);			
		}		
		else{
			list_add_tail(&g_pages[2*page_index+1].list, &free_area[present_block_order-1]); //correct index number later
			list_add_tail(&g_pages[2*page_index+2].list, &free_area[present_block_order-1]); //add its buddy
			mem_addr = recursive_alloc(present_block_order-1, target_block_order);
		}

		if(mem_addr){
			list_del(page_node);			
		  	return mem_addr;
		}
		//if we made it here, we will be going into L_R
		//(move to the next block on this order and repeat)
		
		//page = list_entry( page_list , page_t , list );
	}
	return NULL; //if couldn't find any free blocks
}


/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	
	int alloc_bytes = roundup2(size);
	printf("REQUESTED: %dB; ALLOCATED: %dB\n", size , alloc_bytes);
	
	int target_block_order = log2f(alloc_bytes); //the (starting) free block order to search a free spot in

	void *mem_addr_allocd = NULL;
	int initial_free_order = request_closest_free_block_order(target_block_order);
	if(initial_free_order != -1)
		mem_addr_allocd = recursive_alloc(initial_free_order, target_block_order);
	
	/*
	page_t *page;
	while(order >= MIN_ORDER){
		page = list_entry((&free_area[order])->next, page_t, list);
		printf("%d\n",page->max_alloc);
		if(
	}
	*/
	
	return mem_addr_allocd;
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	printf("FREEING...(to be implemented. as of now subsequent results ARE erroneous).\n");
	int page_index = ADDR_TO_PAGE(addr);	
	int block_order = PAGE_TO_BLOCK_ORDER(page_index);
	printf("   freeing addr %p, page index %d and block %d\n", (int*)addr, page_index, block_order);


}

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		//printf("\nblock order: %d\n", o);
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
