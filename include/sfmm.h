#ifndef __SFMM_H
#define __SFMM_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#define MIN_ALLOC 0x1			/* 1 byte */
#define MAX_ALLOC 0x100000000	/* 4 gigabytes */
#define STD_SBRK_REQ 0x1000		/* 4 kilobytes */

#define MARK_FREE 0x0 			/* mark memory block as free */
#define MARK_USED 0x1 			/* mark memory block as used */

#define EXTRACT_CAPACITY	0x00000000FFFFFFF8 
#define EXTRACT_REQ_SIZE 	0xFFFFFFFF00000000
#define EXTRACT_MARKER_BIT 	0x0000000000000001

#define NULL_POINTER 0x89

/**
* initialize memory allocator
* '_start' function called before main
*/
void sf_mem_init(void);

/**
* implementation of malloc 
* creates dynamic memory which is aligned and padded properly
* for the underlying system. this memory is uninitialized
* @param size the number of bytes requested to be allocated
* @return if successful, the pointer to a valid region of memory
* to use is returned, else the value NULL is returned and the ERRNO
* is set accordingly. is syze if set to zero NULL is returned.
*/
void* sf_malloc(size_t size);

/**
* marks a dynamically allocated region of memory as no longer in use
* @param ptr address of memory returned by the functions, sf_malloc,
* sf_realloc, or sf_calloc
*/
void sf_free(void *ptr);

/**
* resizes the memory pointed to by ptr to be the size bytes
* @param ptr address pf the memory region to resize
* @param size the minimum size to resize the memory to
* @return if successful, the pointer to a valid memory region
* to use is returned, else the value NULL is returned and ERRNO
* is set accordingly
*
* a call with size zero returns NULL and sets ERRNO accordingly
*/
void* sf_realloc(void* ptr, size_t size);

/**
* allocates an array of nmemb elements each if size bytes
* the memory returned is additionally zeroed out
* @param nmemb number of elements in the array
* @param size the size of bytes of each element
* @return if successful, the pointer to a valid memory region
* to use is returned, else the value NULL is returned and ERRNO
* is set accordingly

* if nmemb or size is set to zero, then the value NULL is returned
*/
void* sf_calloc(size_t nmemb, size_t size);

/**
* outputs the state of the free-list to stdout
*/
void sf_snapshot(void);

/** PRIVATE HELPER FUNCTIONS: SECTION START ****************************************************/

/** 
* converts pointer in to an unsigned integer 
* @param ptr pointer to convert to an unsigned integer
* @return ptr value represented as an integer
*/
uintptr_t ptr_as_uint(void *ptr);

/**
* increments heap space by the standard request size (4096 bytes)
* as well moves the end pointer to the correct address
*/
void inc_heapSpace(void);

/**
* initializes the heap space and creates a header and footer
* to mark the initial state of free memory
*/
void init_heapSpace(void);

/**
* calculates the current size of heap in use
* @return the current size of available heap space (in bytes)
*/
size_t get_heapSize(void);

uintptr_t createHeader(uintptr_t req_size, uintptr_t block_size, uintptr_t marker_bit);

uintptr_t splitBlock(uintptr_t **cursor, size_t req_size, size_t allocate_size, size_t total_size);

uintptr_t get_Size(void *from, void *to);

uintptr_t get_loadSize(uintptr_t *cursor);

void allocateBlock(uintptr_t **cursor, size_t req_size, size_t allocate_size);

void addHeapSpace(size_t allocate_size);

void insertHead(void *ptr);

/** PRIVATE HELPER FUNCTIONS: SECTION END ******************************************************/

#endif