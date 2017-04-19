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

#define PAGE_SIZE (1<<MIN_ORDER)	

/* total accessible memory space (in Bytes) */
#define MEMORY_SIZE (1<<MAX_ORDER)

/*number of pages*/
#define NUM_OF_PAGES (MEMORY_SIZE/PAGE_SIZE)


/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find the block order based on the page index. */
#define PAGE_TO_BLOCK_ORDER(page_idx) ( MAX_ORDER - (int)(log2f(page_idx + 1)) )

/* returns the index difference between two consecutive pages (say, a page and its buddy) in a given block order */
#define BUDDY_OFFSET(o) ( NUM_OF_PAGES/(1<<(MAX_ORDER-o)) )

/* find buddy address */  //Block order : o
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

/* checks if the given page index in the given block order is a buddy index (the L_R index)*/
#define CHECK_IF_BUDDY(page_idx, o)( (page_idx/BUDDY_OFFSET(o))%2 )


#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

#define TESTING 0	//Set to 1 to see the steps in the code printf'ed on to the console - for debugging

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int block_order;	//this field indicates in what block order the given page is allocated. If the page is free, this is set to -1
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[MEMORY_SIZE]; 

/* page structures */
page_t g_pages[(MEMORY_SIZE)/PAGE_SIZE]; 



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



/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	for (i = 0; i < NUM_OF_PAGES; i++) {
		INIT_LIST_HEAD(&g_pages[i].list);
		g_pages[i].block_order = -1;	//initialize all pages to "free"
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) { 
		INIT_LIST_HEAD(&free_area[i]);  
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]); 
	
}


//inquires the free area for the available block order that is closest (lowest) to the desired order (target block order)
/*
 * @param block_order the desired block order - guaranteed to be greater than or equal to MIN_ORDER (the page size)
 * @return the lowest available block order
*/
int request_closest_free_block_order(int block_order){

	while(block_order <= MAX_ORDER && list_empty(&free_area[block_order])){
		block_order++;
	}	

	if(block_order > MAX_ORDER){
		return -1;
	}
	
	return block_order;
}

//Allocates a block to memory by removing it from the free_area array. This is a helper function to buddy_alloc.
/* @param starting_block_order the lowest block order that is capable of allocating memory
 * @param target_block_order the block order to which we ultimately want to make an allocation
 * @param size size in bytes
 * @return memory block address
*/
void *_buddy_alloc(int starting_block_order, int target_block_order){ 
	
	struct list_head* page_node;
	int page_index;
	void* mem_addr = NULL;

	if(!(page_node = (&free_area[starting_block_order])->next)){	//get the first available list_head associated with this block order
		return NULL;
	}		
	page_index = (int)((page_t*)page_node - &g_pages[0]); //get the page index corresponding to this list_head
	
	list_del(page_node); //this block order will no longer be free upon allocation, therefore delete it from the free area
	
	//add all the required buddies, at each block order
	for(int block_order = starting_block_order-1; block_order >= target_block_order; --block_order){
		#if TESTING
			printf("	buddy created %p at index %d, at block order %d\n", &g_pages[page_index + BUDDY_OFFSET(block_order)].list, page_index + BUDDY_OFFSET(block_order), block_order);
		#endif	
		list_add_tail(&g_pages[page_index + BUDDY_OFFSET(block_order)].list, &free_area[block_order]); //add its buddy	
	}

	mem_addr = PAGE_TO_ADDR(page_index);	//the memory address of the allocated block
	g_pages[page_index].block_order = target_block_order;	//we just allocated memory to page_index at this block order, so set this field (used later by buddy_free())
	
	#if TESTING
		printf("	allocated to addr %p, at block order %d, at page index %d\n", (int*)mem_addr, target_block_order, page_index);	
	#endif
	
	return mem_addr;
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
	
	#if TESTING
		printf("REQUESTED: %.2fKB\n", (float)size/1024);
	#endif
	
	int alloc_bytes;
	
	//if the requested size is smaller than the page size then set it to the page size
	if(size <= PAGE_SIZE)
		alloc_bytes = PAGE_SIZE;
	else	
	 	alloc_bytes = roundup2(size);

	
	int target_block_order = log2f(alloc_bytes); //the (starting) free block order to search a free spot in
	
	
	void *mem_addr_allocd = NULL;
	
	//get the lowest block_order that supports allocation. -1 is returned if none is available.
	int starting_block_order = request_closest_free_block_order(target_block_order);
	
	//allocate memory if allowed
	if(starting_block_order != -1)
		mem_addr_allocd = _buddy_alloc(starting_block_order, target_block_order);

	#if TESTING
		printf("ALLOCATED: %dKB\n", (mem_addr_allocd ? alloc_bytes : 0)/1024 );
	#endif
	
	return mem_addr_allocd;
}

//iteratively frees block orders upwards, as long as the buddy of the freeable page index is also free. If not, it will stop, and no longer iterate. 
/*
 * @param block_order the block order than contains the freeable page
 * @return page_index the index of the freeable page
 */
void _buddy_free(int block_order, int page_index){
		
	int buddy_page_index;
	struct list_head* page_node = NULL;
	bool buddy_is_free = true;
	
	//if the buddy is free, remove it and redo at the next block level
	while(buddy_is_free && block_order <= MAX_ORDER){
		buddy_is_free = false;
		buddy_page_index = page_index + (CHECK_IF_BUDDY(page_index, block_order) ? -BUDDY_OFFSET(block_order) : BUDDY_OFFSET(block_order)); //get the page index of the buddy based on this page's index
		list_for_each( page_node, &free_area[block_order] ){
			if((page_t*)page_node == &g_pages[buddy_page_index]){	//if the buddy was found (in this order)	
				#if TESTING
					printf("	freeing buddy %p at block order %d, at page index %d, which is the buddy of page index %d\n", page_node, block_order, buddy_page_index, page_index);
				#endif
				list_del(page_node); 	//delete this page's buddy
				g_pages[buddy_page_index].block_order = -1;	//the buddy page is now free -> -1
				page_index = (buddy_page_index < page_index ? buddy_page_index : page_index); //set the appropriate page index in the next block order (up). used in the next iteration.
				buddy_is_free = true;
				block_order++;				
				break;
			}
		}
	}		

	//once no more buddies remain, add the freed block to the free area
	list_add_tail(&g_pages[page_index].list, &free_area[block_order]); //free this page
	g_pages[page_index].block_order = -1;	//this page is now free -> -1

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

	int page_index = ADDR_TO_PAGE(addr);	//page index of the freeable address
	int block_order = g_pages[page_index].block_order;	//block order of the freeable page
	
	#if TESTING

		if(!addr || g_pages[page_index].block_order == -1){
			fprintf(stderr, "Error: Attempted to free a NULL address.\n");
			exit(EXIT_FAILURE);
		}
		
		if( page_index < 0 || page_index >= NUM_OF_PAGES ){
			fprintf(stderr, "Error: Attempted to free an OUT-OF-BOUNDS Address (%p). The valid address range is from %p to %p.\n", (int*)addr, &g_memory[0], &g_memory[MEMORY_SIZE - 1]);
			exit(EXIT_FAILURE);
		} 
		
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
	
		//free the page and buddies iteratively
		_buddy_free(block_order, page_index);

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
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
