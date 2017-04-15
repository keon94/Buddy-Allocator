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

/* find the block order based on the page index. */
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

#define TESTING 0

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
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

void test1(){
	
	int *addr1, *addr2, *addr3, *addr4;
	printf("\n");
	addr1 = buddy_alloc(80*1024);
	buddy_dump();
	printf("\n");
	addr2 = buddy_alloc(60*1024);
	buddy_dump();
	printf("\n");
	addr3 = buddy_alloc(80*1024);	
	buddy_dump();
	
	printf("\n");
	buddy_free(addr1);
	buddy_dump();

	printf("\n");
	addr4 = buddy_alloc(32*1024);	
	buddy_dump();
	
	printf("\n");
	buddy_free(addr2);
	buddy_dump();
	printf("\n");
	buddy_free(addr4);
	buddy_dump();
	printf("\n");
	buddy_free(addr3);
	buddy_dump();
	printf("\n");

	
}

void test2(){
	int *addr1;
	printf("\n");
	addr1 = buddy_alloc(44*1024);
	buddy_dump();
	printf("\n");
	buddy_free(addr1);
	buddy_dump();
	printf("\n");
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
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE; //2^20/2^12 = 256
	for (i = 0; i < n_pages; i++) {
		INIT_LIST_HEAD(&g_pages[i].list);
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) { //12 to 20
		INIT_LIST_HEAD(&free_area[i]);  //each free area's head is initialised 
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]); 
	
	//for test only
	#if TESTING
		printf("\n*************************Test 1**************************\n");
		test1();
		printf("*************************Test 2**************************\n");
		test2();
		exit(EXIT_SUCCESS);
	#endif
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

//Allocates a page to memory by removing it from the free_area array. The page is found recursively, for a given target_block_order (based on the demanded page size)
void *recursive_alloc(int present_block_order, int target_block_order){ //page offset: the offset of the page in a given block order
	
	struct list_head* page_node;
	int page_index;
	void* mem_addr = NULL;

	list_for_each( page_node, &free_area[present_block_order] ){
		//traversal in the list happens for each page_list that does exist in the present block

		page_index = (int)((page_t*)page_node - &g_pages[0]);
		
		if(present_block_order == target_block_order){ //we've reached the target block order
			mem_addr = PAGE_TO_ADDR(page_index); 
			#if TESTING
				printf("	allocated to addr %p, at block order %d, at page index %d\n", (int*)mem_addr, present_block_order, page_index);	
			#endif		
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
		//(move to the next page in this block order and repeat)
		
	}
	return NULL; //if couldn't find any free pages
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
	
	int alloc_bytes = roundup2(size);
	#if TESTING
		printf("REQUESTED: %dB; ALLOCATED: %dB\n", size , alloc_bytes);
	#endif
	
	int target_block_order = log2f(alloc_bytes); //the (starting) free block order to search a free spot in

	void *mem_addr_allocd = NULL;
	int initial_free_order = request_closest_free_block_order(target_block_order);
	if(initial_free_order != -1)
		mem_addr_allocd = recursive_alloc(initial_free_order, target_block_order);
	
	return mem_addr_allocd;
}

//recursively frees block orders upwards, as long as the buddy of the freeable page index is also free. If not, it will stop, and no longer recurse. 
//NOTE: This is a tail recursive function that can be trivially converted to an iterative one (with a loop). Will probably do that later, since iteration is a lot better on memory and performance.
void recursive_free(int block_order, int page_index){
		
	int buddy_page_index = page_index + ( page_index % 2 == 1 ? 1 : -1) ;

	//check the free area at this block to see if the buddy is available
	struct list_head* page_node = NULL;
	bool buddy_is_free = false;
	list_for_each( page_node, &free_area[block_order] ){
		if((page_t*)page_node == &g_pages[buddy_page_index]){		
			list_del(page_node); 	//delete this page's buddy
			buddy_is_free = true;
			break;
		}
	}
	
	#if TESTING
		printf("	at block order %d, at page index %d\n", block_order, page_index);
	#endif

	//will always go to the 'left' page in the parent block order(L_L)
	int parent_page_index = (page_index-1)/2;

	if(buddy_is_free){
		recursive_free( block_order+1 , parent_page_index ); //free the parent block since the buddy of this page was also free
	}
	else{
		list_add_tail(&g_pages[page_index].list, &free_area[block_order]); //buddy page wasn't free, so just free this page (by adding it to the free area)
	}

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

	int page_index = ADDR_TO_PAGE(addr);	
	int block_order = PAGE_TO_BLOCK_ORDER(page_index);
	
	#if TESTING
		printf("FREEING addr %p\n",  (int*)addr);	
		/* Make sure we're not double freeing. For Testing Purposes (Although, probably a good thing to have in general, just like the real free() function does) */
		struct list_head* page_node = NULL;
		list_for_each( page_node, &free_area[block_order] ){
			if((page_t*)page_node == &g_pages[page_index]){
				fprintf(stderr, "Error: Attempted a double free at addr %p, block order %d, page index %d\n", (int*)addr, block_order, page_index);
				exit(EXIT_FAILURE);
			}
		}
	#endif
	
	recursive_free(block_order, page_index);

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
