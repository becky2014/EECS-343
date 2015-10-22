/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 
 ************************************************************************/

/************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/

#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

/************Function Prototypes******************************************/

/************External Declaration*****************************************/

/**************Implementation***********************************************/

typedef struct block_t {
    void *base;
    kma_size_t size;
    bool occupied;
    struct block_t *child;
    struct block_t *parent;
} block_t;

typedef struct page_wrapper_t {
    block_t *block_head;
    kma_page_t *page;
    struct page_wrapper_t *child;
    struct page_wrapper_t *parent;
} page_wrapper_t;

page_wrapper_t *page_head = NULL;



void *kma_malloc(kma_size_t size) {
    assert(size <= PAGESIZE);
    if (!page_head) {
        page_head = (page_wrapper_t *)malloc(sizeof(page_wrapper_t));
        page_head->block_head = (block_t *)malloc(sizeof(block_t));
        page_head->page = get_page();
        page_head->child = page_head;
        page_head->parent = page_head;
        block_t *block = page_head->block_head;
        block->base = page_head->page->ptr;
        block->size = page_head->page->size;
        block->occupied = FALSE;
        block->child = block;
        block->parent = block;
    }
    page_wrapper_t *page_looper = page_head;
    do {
        block_t *block_looper = page_looper->block_head;
        do {
            if (!block_looper->occupied && block_looper->size >= size) {
                void *ptr = block_looper->base;
                if (block_looper == page_looper->block_head) {
                    if (block_looper->size > size) {
                        block_t *block = (block_t *)malloc(sizeof(block_t));
                        block->base = block_looper->base + size;
                        block->size = block_looper->size - size;
                        block->occupied = FALSE;
                        block->child = block_looper->child;
                        block->parent = block_looper;
                        block_looper->size = size;
                        block_looper->occupied = TRUE;
                        block_looper->child->parent = block;
                        block_looper->child = block;
                    } else {
                        if (block_looper->child == block_looper) {
                            block_looper->occupied = TRUE;
                            assert(block_looper->size == page_looper->page->size);
                        } else {
                            block_t *block = block_looper->child;
                            block_looper->occupied = TRUE;
                            block_looper->size += block_looper->child->size;
                            block_looper->child->child->parent = block_looper;
                            block_looper->child = block_looper->child->child;
                            free(block);
                        }
                    }
                } else {
                    if (block_looper->size > size) {
                        block_looper->base += size;
                        block_looper->size -= size;
                        block_looper->parent->size += size;
                    } else {
                        if (block_looper->child == page_looper->block_head) {
                            block_looper->parent->size += block_looper->size;
                            block_looper->parent->child = block_looper->child;
                            block_looper->child->parent = block_looper->parent;
                            free(block_looper);
                        } else {
                            block_looper->parent->size += block_looper->size + block_looper->child->size;
                            block_looper->parent->child = block_looper->child->child;
                            block_looper->child->child->parent = block_looper->parent;
                            free(block_looper->child);
                            free(block_looper);
                        }
                    }
                }
                return ptr;
            }
            block_looper = block_looper->child;
        } while (block_looper != page_looper->block_head);
        page_looper = page_looper->child;
    } while (page_looper != page_head);
    page_wrapper_t *page_wrapper = (page_wrapper_t *)malloc(sizeof(page_wrapper_t));
    page_wrapper->block_head = (block_t *)malloc(sizeof(block_t));
    page_wrapper->page = get_page();
    page_wrapper->child = page_head;
    page_wrapper->parent = page_head->parent;
    page_head->parent->child = page_wrapper;
    page_head->parent = page_wrapper;
    
    block_t *occ = page_wrapper->block_head;
    occ->base = page_wrapper->page->ptr;
    occ->size = size;
    occ->occupied = TRUE;
    if (size < page_wrapper->page->size) {
        block_t *fre = (block_t *)malloc(sizeof(block_t));
        occ->child = fre;
        occ->parent = fre;
        fre->base = occ->base + size;
        fre->size = page_wrapper->page->size - size;
        fre->occupied = FALSE;
        fre->child = occ;
        fre->parent = occ;
    } else {
        occ->child = occ;
        occ->parent = occ;
    }
    return occ->base;
}

void kma_free(void* ptr, kma_size_t size) {
    assert(page_head);
    page_wrapper_t *page_looper = page_head;
    do {
        if (page_looper->page->ptr <= ptr && ptr + size <= page_looper->page->ptr + page_looper->page->size) {
            block_t *block_looper = page_looper->block_head;
            do {
                if (block_looper->occupied && block_looper->base <= ptr && ptr + size <= block_looper->base + block_looper->size) {
                    if (block_looper == page_looper->block_head) {
                        if (block_looper->size == size) {
                            if (block_looper->child == block_looper) {
                                block_looper->occupied = FALSE;
                                assert(block_looper->size == page_looper->page->size);
                            } else {
                                block_t *block = block_looper->child;
                                block_looper->occupied = FALSE;
                                block_looper->size += block_looper->child->size;
                                block_looper->child->child->parent = block_looper;
                                block_looper->child = block_looper->child->child;
                                free(block);
                            }
                        } else if (block_looper->base == ptr) {
                            block_t *block = (block_t *)malloc(sizeof(block_t));
                            block->occupied = TRUE;
                            block->size = block_looper->size - size;
                            block->base = block_looper->base + size;
                            block->child = block_looper->child;
                            block->parent = block_looper;
                            block_looper->occupied = FALSE;
                            block_looper->size = size;
                            block_looper->child->parent = block;
                            block_looper->child = block;
                        } else if (block_looper->base + block_looper->size == ptr + size) {
                            if (block_looper->child == block_looper) {
                                assert(block_looper->size == page_looper->page->size);
                                block_t *block = (block_t *)malloc(sizeof(block_t));
                                block->occupied = FALSE;
                                block->size = size;
                                block->base = ptr;
                                block->child = block_looper;
                                block->parent = block_looper;
                                block_looper->size -= size;
                                block_looper->child = block;
                                block_looper->parent = block;
                            } else {
                                block_looper->size -= size;
                                block_looper->child->base = ptr;
                                block_looper->child->size += size;
                            }
                            
                        } else {
                            block_t *fre = (block_t *)malloc(sizeof(block_t));
                            fre->occupied = FALSE;
                            fre->size = size;
                            fre->base = ptr;
                            block_t *occ = (block_t *)malloc(sizeof(block_t));
                            occ->occupied = TRUE;
                            occ->size = block_looper->size - (ptr - block_looper->base + size);
                            occ->base = ptr + size;
                            fre->parent = block_looper;
                            fre->child = occ;
                            occ->parent = fre;
                            occ->child = block_looper->child;
                            block_looper->size = ptr - block_looper->base;
                            block_looper->child->parent = occ;
                            block_looper->child = fre;
                        }
                    } else {
                        if (block_looper->size == size) {
                            if (block_looper->child == page_looper->block_head) {
                                block_looper->parent->size += size;
                                block_looper->parent->child = block_looper->child;
                                block_looper->child->parent = block_looper->parent;
                                free(block_looper);
                            } else {
                                block_looper->parent->size += block_looper->size + block_looper->child->size;
                                block_looper->parent->child = block_looper->child->child;
                                block_looper->child->child->parent = block_looper->parent;
                                free(block_looper->child);
                                free(block_looper);
                            }
                        } else if (block_looper->base == ptr) {
                            block_looper->base = ptr + size;
                            block_looper->size -= size;
                            block_looper->parent->size += size;
                        } else if (block_looper->base + block_looper->size == ptr + size) {
                            if (block_looper->child == page_looper->block_head) {
                                block_t *block = (block_t *)malloc(sizeof(block_t));
                                block->occupied = FALSE;
                                block->size = size;
                                block->base = ptr;
                                block->child = block_looper->child;
                                block->parent = block_looper;
                                block_looper->size -= size;
                                block_looper->child->parent = block;
                                block_looper->child = block;
                            } else {
                                block_looper->size -= size;
                                block_looper->child->base -= size;
                                block_looper->child->size += size;
                            }
                        } else {
                            block_t *fre = (block_t *)malloc(sizeof(block_t));
                            fre->occupied = FALSE;
                            fre->size = size;
                            fre->base = ptr;
                            block_t *occ = (block_t *)malloc(sizeof(block_t));
                            occ->occupied = TRUE;
                            occ->size = block_looper->size - (ptr - block_looper->base + size);
                            occ->base = ptr + size;
                            fre->parent = block_looper;
                            fre->child = occ;
                            occ->parent = fre;
                            occ->child = block_looper->child;
                            block_looper->size = ptr - block_looper->base;
                            block_looper->child->parent = occ;
                            block_looper->child = fre;
                        }
                    }
                    if (page_looper->block_head->child == page_looper->block_head) {
                        assert(page_looper->block_head->occupied == FALSE);
                        if (page_looper == page_head) {
                            if (page_looper->child == page_looper) {
                                free_page(page_looper->page);
                                free(page_looper);
                                page_head = NULL;
                            } else {
                                page_head = page_looper->child;
                                page_looper->parent->child = page_looper->child;
                                page_looper->child->parent = page_looper->parent;
                                free_page(page_looper->page);
                                free(page_looper);
                            }
                        } else {
                            page_looper->parent->child = page_looper->child;
                            page_looper->child->parent = page_looper->parent;
                            free_page(page_looper->page);
                            free(page_looper);
                        }
                    }
                    return;
                }
                block_looper = block_looper->child;
            } while (block_looper != page_looper->block_head);
            assert(FALSE);
        }
        page_looper = page_looper->child;
    } while (page_looper != page_head);
    assert(FALSE);
}

#endif // KMA_RM