#include "sfmm.h"

void *start; 	/* address of the start of the heap space, should always point to this address */
void *head; 	/* address to the memory block at the start of the explicit list */
void *recent; 	/* address of the most recently added free block */
void *end; 		/* address of the end of the heap space, should always point to this address */

void sf_mem_init(void){
	start = sbrk(0); /* get the address at the start of the heap */
	head = start;
	recent = start;
	end = start;
}

void* sf_malloc(size_t size){
	if(size < 0x1 || size > 0x100000000) return NULL; /* invalid request size */

	if(start == end) init_heapSpace(); /* no call has been made to request heap space, do so here */

	uintptr_t *cursor = (uintptr_t *) NULL_POINTER;

restart_search: /* kind of like the cache if there's no space big enough(miss), get that space and tell you to ask me again */
	cursor = (uintptr_t*) head;

	uintptr_t allocate_size = 0;
	if(size < 16) 
		allocate_size = 16;
	else if (size % 8 == 0) 
		allocate_size = (uintptr_t) size;
	else 
		allocate_size =(uintptr_t) (size + (8 - (size % 8)));

	if((allocate_size / 8) % 2 != 0) allocate_size += 8;

	uintptr_t *result = (uintptr_t*) NULL_POINTER;

	if(cursor != (uintptr_t *)NULL_POINTER){
		while(get_loadSize(cursor) < allocate_size && (uintptr_t) *(cursor + 1) != NULL_POINTER)
		cursor = (uintptr_t *) *(cursor + 1); /* cursor = cursor.getNext(); */

		uintptr_t loadSize = get_loadSize(cursor);

		/* if the request fits and there is space to split */
		if(loadSize > (allocate_size + 48)){
			uintptr_t **ptr = &cursor;
			splitBlock(ptr, size, allocate_size, loadSize); 
			result = cursor + 1;
		}
		else if(loadSize >= allocate_size){
			uintptr_t **ptr = &cursor;
			allocateBlock(ptr, size, allocate_size);
			result = cursor + 1;
		}else if(loadSize < allocate_size){
			addHeapSpace(allocate_size);
			goto restart_search;
		}
	}
	/* could not find a space big enough to hold the requested size */
	else {
		addHeapSpace(allocate_size);
		goto restart_search;
	}

	return result;
}


void sf_free(void *ptr){
	/* ptr is not a valid address within dynamic memory space */
	if(ptr < start || ptr >= end) return;

	/* address provided is already a free region of memory */
	if((*(((uintptr_t*)ptr) - 1) & EXTRACT_MARKER_BIT) == MARK_FREE)
		return;

	uintptr_t *cursor = (uintptr_t*) (((uintptr_t*) ptr) - 1);
	
	uintptr_t ptr_as_int = (uintptr_t) cursor;
	ptr_as_int = ptr_as_int & 0xF;
	if(ptr_as_int != 8){	
		errno = ENOMEM;
		return;
	}

	uintptr_t cursorSize = get_loadSize(cursor);
	uintptr_t *cursorFooter = cursor + (cursorSize / 8) + 1;
	uintptr_t *newBlockHeader = (uintptr_t*) NULL_POINTER;
	uintptr_t *newBlockFooter = (uintptr_t*) NULL_POINTER;
	uintptr_t *previousFooter = cursor - 1;
	if((void*) previousFooter > start){
		uintptr_t previousSize = get_loadSize(previousFooter);
		uintptr_t perviousMarker = (uintptr_t) (*previousFooter & EXTRACT_MARKER_BIT);
		uintptr_t *previousHeader = previousFooter - (previousSize / 8) - 1;
		if((void*) previousHeader > start && perviousMarker == MARK_FREE){
			uintptr_t *previousNext = (uintptr_t *) *(previousHeader + 1);
			uintptr_t *previousPrevious = (uintptr_t*) *(previousHeader + 2);

			if((uintptr_t) previousNext != NULL_POINTER && (uintptr_t) previousPrevious != NULL_POINTER){
				*(previousNext + 2) = (uintptr_t) previousPrevious;
				*(previousPrevious + 1) = (uintptr_t) previousNext;

				newBlockHeader = previousHeader;

			}else{
				newBlockHeader = cursor;
			} 
		}else{
			newBlockHeader = cursor;
		}
	}else{
		newBlockHeader = cursor;
	} 

	uintptr_t *nextHeader = cursorFooter + 1;
	if((void*) nextHeader < end){
		uintptr_t nextSize = get_loadSize(nextHeader);
		uintptr_t nextMarker = (uintptr_t) (*nextHeader & EXTRACT_MARKER_BIT);
		uintptr_t *nextFooter = nextHeader + (nextSize / 8) + 1;
		if((void*) nextFooter < end - 1 && nextMarker == MARK_FREE){
			uintptr_t *nextNext = (uintptr_t*) *(nextHeader + 1);
			uintptr_t *nextPrevious = (uintptr_t*) *(nextHeader + 2);
	
			if((uintptr_t) nextNext != NULL_POINTER && (uintptr_t) nextPrevious != NULL_POINTER){
				*(nextNext + 2) = (uintptr_t) nextPrevious;
				*(nextPrevious + 1) = (uintptr_t) nextNext;

				newBlockFooter = nextFooter;
			}else{
				newBlockFooter = cursorFooter;
			}
		}else{
			newBlockFooter = cursorFooter;
		}
	}else{
		newBlockFooter = cursorFooter;
	}

	uintptr_t newBlockSize = get_Size(newBlockHeader, newBlockFooter) + 8;
	uintptr_t newBlockInfo = createHeader(0, newBlockSize, MARK_FREE);

	*newBlockHeader = newBlockInfo;
	*newBlockFooter = newBlockInfo;

	insertHead(newBlockHeader);
}

void* sf_realloc(void* ptr, size_t size){
	if((void *)ptr < start || (void *)ptr >= end){
		errno = ENOMEM;
		return NULL;
	}

	uintptr_t *cursor = (uintptr_t*) (((uintptr_t*) ptr) - 1);

	if((((uintptr_t) cursor) & 0xF) != 0x8){
		errno =ENOMEM;
		return ptr;
	}

	uintptr_t loadSize = get_loadSize(cursor);

	if(loadSize > size){
		uintptr_t blockInfo = createHeader(size, loadSize + 16, MARK_USED);
		*cursor = blockInfo;

		return ptr;
	}
	else{
		errno = ENOMEM; 
		return NULL;
	}

}


void* sf_calloc(size_t nmemb, size_t size){
	if(nmemb <=0 || size <= 0){ 
		errno = ENOMEM;
		return NULL;
	}

	uintptr_t* result = (void *) sf_malloc(nmemb * size);
	uintptr_t count = get_loadSize(result - 1) / 8;
	int i;
	for(i = 0; i < count; i++)
		*(result + i) = 0;

	return (void *) result;
} 

void sf_snapshot(void){
	uintptr_t *cursor = (uintptr_t *) head;
	int t = time(NULL);
	printf("Explicit 8 %lu\n", get_heapSize());
	printf("# %d\n", t);
	while((uintptr_t) cursor != NULL_POINTER){
		printf("%p %lu\n", cursor, (*cursor & EXTRACT_CAPACITY));
		cursor = (uintptr_t *)*(cursor + 1);
	}
}

uintptr_t ptr_as_uint(void *ptr){
	return (uintptr_t) ptr;
}

void inc_heapSpace(void){
	sbrk(STD_SBRK_REQ);
	end = (void*)(ptr_as_uint(end) + STD_SBRK_REQ);
}

void init_heapSpace(void){
	inc_heapSpace();

	uintptr_t *header_address = (uintptr_t *) (((uintptr_t *)start) + 1);
	uintptr_t *footer_address = (uintptr_t *) (((uintptr_t *)end) - 2);
	uintptr_t init_info = createHeader(STD_SBRK_REQ, STD_SBRK_REQ - 16, MARK_FREE);

	*header_address = init_info;
	*footer_address = init_info;

	*(header_address + 1) = NULL_POINTER; /* set next to NULL POINTER */
	*(header_address + 2) = NULL_POINTER; /* set previous to NULL POINTER */
	head = (void *) header_address;
}

uintptr_t get_Size(void *from, void *to){
	return ptr_as_uint(to) - ptr_as_uint(from);
}

uintptr_t get_loadSize(uintptr_t *cursor){
	uintptr_t loadSize = (*cursor & EXTRACT_CAPACITY);
	loadSize -= 16;
	return loadSize;
}

size_t get_heapSize(void){
	return ptr_as_uint(end) - ptr_as_uint(start);
}

uintptr_t createHeader(uintptr_t req_size, uintptr_t block_size, uintptr_t marker_bit){
	return ( (uintptr_t)  (req_size << 32) | (block_size) | marker_bit);
}

uintptr_t splitBlock(uintptr_t **cursor, size_t req_size, size_t allocate_size, size_t total_size){
	uintptr_t *header_address = *cursor;
	uintptr_t *footer_address = header_address + (allocate_size / 8) + 1;
	
	uintptr_t *freeHeader_address = footer_address + 1;
	uintptr_t *freeFooter_address = header_address + (total_size / 8) + 1;

	uintptr_t freeSize = total_size - allocate_size - 16;

	uintptr_t allocated_info = createHeader(req_size, allocate_size + 16, MARK_USED);
	uintptr_t free_info = createHeader(0,freeSize + 16 , MARK_FREE);

	*header_address = allocated_info;
	*footer_address = allocated_info;

	*freeHeader_address = free_info;
	*freeFooter_address = free_info;

	*(freeHeader_address + 1) = *(header_address + 1);
	*(freeHeader_address + 2) = *(header_address + 2);

	/* split block resides between two free blocks */
	if((uintptr_t) *(freeHeader_address + 1) != NULL_POINTER 
	&& (uintptr_t) *(freeHeader_address + 2) != NULL_POINTER ){
		uintptr_t *next = (uintptr_t*) *(freeHeader_address + 1);
		uintptr_t *previous = (uintptr_t*) *(freeHeader_address + 2);

		*(next + 2) = (uintptr_t) previous;
		*(previous + 1) = (uintptr_t) next;

		insertHead(freeHeader_address);
	}

	/* split block has a block infront but not behind */
	else if((uintptr_t) *(freeHeader_address + 1) != NULL_POINTER 
		 && (uintptr_t) *(freeHeader_address + 2) == NULL_POINTER ){
		uintptr_t *next = (uintptr_t*) *(freeHeader_address + 1);
		
		*(next + 2) = (uintptr_t) freeHeader_address;

		head = (void *) freeHeader_address;
	}

	/* split block has a block behind but not infront */
	else if((uintptr_t) *(freeHeader_address + 1) == NULL_POINTER 
		 && (uintptr_t) *(freeHeader_address + 2) != NULL_POINTER ){
		uintptr_t *previous = (uintptr_t*) *(freeHeader_address + 2);
		
		*(previous + 1) = (uintptr_t) NULL_POINTER;
		insertHead(freeHeader_address);

	}else{
		/* must of split the only free block in the list*/
		head = (void *) freeHeader_address;
	}

	return 1;
}

void allocateBlock(uintptr_t **cursor, size_t req_size, size_t allocate_size){
	uintptr_t *header_address = *cursor;
	uintptr_t *footer_address = header_address + (allocate_size / 8) + 1;

	uintptr_t allocated_info = createHeader(req_size, allocate_size + 16, MARK_USED);
	*header_address = allocated_info;
	*footer_address = allocated_info;

	/* allocated block resided between two free blocks */
	if((uintptr_t) *(header_address + 1) != NULL_POINTER 
	&& (uintptr_t) *(header_address + 2) != NULL_POINTER ){
		
		uintptr_t *next = (uintptr_t*) *(header_address + 1);
		uintptr_t *previous = (uintptr_t*) *(header_address + 2);

		*(next + 2) = (uintptr_t) previous;
		*(previous + 1) = (uintptr_t) next;

	}

	/* allocated block has a block infront but not behind */
	else if((uintptr_t) *(header_address + 1) != NULL_POINTER 
		 && (uintptr_t) *(header_address + 2) == NULL_POINTER ){
		
		uintptr_t *next = (uintptr_t*) *(header_address + 1);
		
		*(next + 2) = (uintptr_t) NULL_POINTER;

		head = (void *) next;
	}

	/* allocated block has a block behind but not infront */
	else if((uintptr_t) *(header_address + 1) == NULL_POINTER 
		 && (uintptr_t) *(header_address + 2) != NULL_POINTER ){
		
		uintptr_t *previous = (uintptr_t*) *(header_address + 2);
		
		*(previous + 1) = (uintptr_t) NULL_POINTER;

	}else{
		/* must of allocated the only free block in the list*/
		head = (uintptr_t *) NULL_POINTER;
	}	
}

void addHeapSpace(size_t allocate_size){
	uintptr_t *header_address = (((uintptr_t *)end) + 1);
	uintptr_t *temp_footer = header_address +( allocate_size / 8) + 1;

	int increments = 0;
	while((void *)temp_footer >= end){
		inc_heapSpace();
		increments ++;
	}
	uintptr_t *footer_address = (((uintptr_t *)end) - 2);

	uintptr_t init_info = createHeader(STD_SBRK_REQ, increments * STD_SBRK_REQ - 16, MARK_FREE);
	*header_address = init_info;
	*footer_address = init_info;

	if((uintptr_t) head != NULL_POINTER){
		insertHead(header_address);

	}else{
		*(header_address + 1) = (uintptr_t) NULL_POINTER;
		*(header_address + 2) = (uintptr_t) NULL_POINTER;
		head = (void *) header_address;
	}
}

void insertHead(void *ptr){
	uintptr_t *newHead = (uintptr_t*) ptr;
	uintptr_t *oldHead = (uintptr_t*) head;

	*(newHead + 1) = (uintptr_t) oldHead;
	*(newHead + 2) = (uintptr_t) NULL_POINTER;

	*(oldHead + 2) = (uintptr_t) newHead;

	head = (void *) newHead;
}
