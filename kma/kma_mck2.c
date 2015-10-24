/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the McKusick-Karels
 *              algorithm
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

#ifdef KMA_MCK2
#define __KMA_IMPL__

/************System include***********************************************/
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

#define MINPOWER 4
#define FREELISTSIZE 9
#if __x86_64__ || __ppc64__
#define MASK 0xFFFFFFFFFFFFE000
#else
#define MASK 0xFFFFE000
#endif


typedef struct page_t {
    kma_page_t *page;
    kma_size_t block_size;
    kma_size_t used_count;
    struct page_t *next;
} page_t;

page_t page_stat[MAXPAGES];
page_t *page_head = NULL;
page_t *free_head = NULL;
void *freelist[FREELISTSIZE] = {0};



kma_size_t IDX(size) {
    // not process size larger than 4096
    return
        size > 256
            ? size > 512
                ? size > 1024
                    ? size > 2048 ? 8 : 7
                    : 6
                : 5
            : size > 128
                ? 4
                : size > 64
                    ? 3
                    : size > 32
                        ? 2
                        : size > 16 ? 1 : 0;
}


void inc_used(void* ptr) {
    page_t *looper = page_head;
    while (looper) {
        if ((void *)((unsigned long)ptr & MASK) == looper->page->ptr) {
            looper->used_count++;
            return;
        }
        looper = looper->next;
    }
}


void* kma_malloc(kma_size_t size) {
    if (!free_head && !page_head) {
        int i;
        for (i = 0; i < MAXPAGES - 1; i++) {
            (page_stat[i]).next = page_stat + i + 1;
        }
        (page_stat[MAXPAGES - 1]).next = NULL;
        free_head = page_stat;
    }
    void *space = NULL;
    if (size > PAGESIZE / 2) {
        kma_page_t *page = get_page();
        page_t *tmp = free_head;
        free_head = free_head->next;
        tmp->page = page;
        tmp->used_count = 1;
        tmp->block_size = page->size;
        tmp->next = page_head;
        page_head = tmp;
        space = page->ptr;
    } else {
        kma_size_t idx = IDX(size);
        kma_size_t bufsize = 1 << (MINPOWER + idx);
        if (freelist[idx]) {
            space = freelist[idx];
            inc_used(freelist[idx]);
            freelist[idx] = *((void **)freelist[idx]);
        } else {
            kma_page_t *page = get_page();
            page_t *tmp = free_head;
            free_head = free_head->next;
            tmp->page = page;
            tmp->used_count = 1;
            tmp->block_size = bufsize;
            tmp->next = page_head;
            page_head = tmp;
            void *ptr;
            for (ptr = page->ptr + bufsize; ptr < page->ptr + page->size - bufsize; ptr += bufsize) {
                *((void **)ptr) = ptr + bufsize;
            }
            *((void **)(page->ptr + page->size - bufsize)) = freelist[idx];
            freelist[idx] = page->ptr + bufsize;
            space = page->ptr;
        }
    }
    return space;
}

void kma_free(void* ptr, kma_size_t size) {
    page_t *looper = page_head;
    page_t *looper_pre = NULL;
    while (looper) {
        if ((void *)((unsigned long)ptr & MASK) == looper->page->ptr) {
            looper->used_count--;
            if (looper->used_count) {
                kma_size_t idx = IDX(looper->block_size);
                *((void **)ptr) = freelist[idx];
                freelist[idx] = ptr;
            } else {
                if (looper->block_size < looper->page->size) {
                    kma_size_t idx = IDX(looper->block_size);
                    void *block_looper = freelist[idx];
                    freelist[idx] = NULL;
                    while (block_looper) {
                        void *tmp = block_looper;
                        block_looper = *((void **)block_looper);
                        if ((void *)((unsigned long)tmp & MASK) != looper->page->ptr) {
                            *((void **)tmp) = freelist[idx];
                            freelist[idx] = tmp;
                        }
                    }
                }
                free_page(looper->page);
                if (looper != page_head) {
                    looper_pre->next = looper->next;
                } else {
                    page_head = looper->next;
                }
                looper->next = free_head;
                free_head = looper;
            }
            return;
        }
        looper_pre = looper;
        looper = looper->next;
    }
}

#endif // KMA_MCK2
