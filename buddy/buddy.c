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

#define BLOCK_LEVEL_TO_PAGE(block_level) ( (1<<(MAX_ORDER - block_level)) - 1 )

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

static int largest_available_block_level_index = MAX_ORDER;

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

void test_alloc(){
	/*
	printf("to alloc 80k\n");
	buddy_alloc(80*1024);
	buddy_dump();
	printf("to alloc 60k\n");
	buddy_alloc(60*1024);
	buddy_dump();
	printf("to alloc 80k\n");
	buddy_alloc(80*1024);
	buddy_dump();
	*/
	buddy_alloc(4*1024);
	buddy_dump();
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
	test_alloc();
	exit(0);
}


//inquires the free area for the available block level that is closest to the desired level (target block level)
//returns an int (the block number)
int request_closest_free_block_level(int block_level){
	if(block_level > MAX_ORDER){
		return -1;
	}
	if(list_empty(&free_area[block_level])){
		return request_closest_free_block_level(block_level+1);
	}
	else{
		return block_level;
	}
}




void* recursive_alloc(int present_block_level, int target_block_level, int page_offset){ //page offset: the offset of the page in a given block level
	
	page_t *page;
	struct list_head* page_node;
	int page_index;
	void* mem_addr = NULL;

	list_for_each( page_node, &free_area[present_block_level] ){
		//traversal in the list happens for each page_list that does exist
		page_index = BLOCK_LEVEL_TO_PAGE(present_block_level) + page_offset;
		
		if(present_block_level == target_block_level){ //we've reached the target block leve
			mem_addr = PAGE_TO_ADDR(page_index);  //page index = block_level_init_index + page_offset
			//remove this page from free mem
			
		}		
		else{
			list_add(&g_pages[2*page_index+1].list, &free_area[present_block_level-1]); //correct index number later
			list_add(&g_pages[2*page_index+2].list, &free_area[present_block_level-1]); //add its buddy
			mem_addr = recursive_alloc(present_block_level - 1, target_block_level, page_offset);
		}

		if(mem_addr){
			list_del(page_node);			
		  	return mem_addr;
		}
		//if we made it here, we will be going into L_R
		//(move to the next block on this level and repeat)
		page_offset += (1<<present_block_level)/1024;
		
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
	
	int target_block_level = log2f(alloc_bytes); //the (starting) free block level to search a free spot in
	
	/*
	page_t *page;
	struct list_head* page_list;
	while(block_level <= MAX_ORDER){
		
		list_for_each( page_list, &free_area[block_level] ){


		}
		block_level++;
	}
	*/

	void *mem_addr_allocd = NULL;
	int initial_free_level = request_closest_free_block_level(target_block_level);
	if(initial_free_level != -1)
		mem_addr_allocd = recursive_alloc(initial_free_level, target_block_level, 0);
	
	/*
	page_t *page;
	while(level >= MIN_ORDER){
		page = list_entry((&free_area[level])->next, page_t, list);
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
		//printf("\nblock level: %d\n", o);
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
