/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
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

#ifdef KMA_P2FL
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

/*
 * statistics in 5.trace
 * 4:		0
 * 8:		1698
 * 16:		9286
 * 32:		9412
 * 64: 		9681
 * 128: 	9958
 * 256: 	10045
 * 512: 	10109
 * 1024:	10228
 * 2048: 	10012
 * 4096: 	10127
 * 8192:	9444
 * 16384:	0
 */

#define PTRSIZE sizeof(void *)
#define MINPOWER 4
#define FREELISTSIZE 10

typedef struct page_wrapper_t {
    kma_page_t *page;
    struct page_wrapper_t *next;
} page_wrapper_t;

page_wrapper_t *page_head = NULL;
void *free_list[FREELISTSIZE] = {0};


void* kma_malloc(kma_size_t size) {
    kma_size_t idx = 0;
    kma_size_t bufsize = 1 << MINPOWER;
    size += PTRSIZE;
    while (bufsize < size) {
        idx++;
        bufsize <<= 1;
    }
    if (!free_list[idx]) {
        kma_page_t *page = get_page();
        page_wrapper_t *page_wrapper = (page_wrapper_t *)malloc(sizeof(page_wrapper_t));
        page_wrapper->page = page;
        page_wrapper->next = page_head;
        page_head = page_wrapper;
        void *ptr;
        for (ptr = page->ptr; ptr < page->ptr + page->size - bufsize; ptr += bufsize) {
            *((void **)ptr) = ptr + bufsize;
        }
        *((void **)(page->ptr + page->size - bufsize)) = free_list[idx];
        free_list[idx] = page->ptr;
    }
    void *space = free_list[idx];
    free_list[idx] = *((void **)free_list[idx]);
    *((void **)space) = free_list + idx;
    return space + PTRSIZE;
}

void kma_free(void* ptr, kma_size_t size) {
    ptr -= PTRSIZE;
    kma_size_t idx = 0;
    while (*((void **)ptr) != free_list + idx) {
        idx++;
    }
    *((void **)ptr) = free_list[idx];
    free_list[idx] = ptr;
    page_wrapper_t *page_cur = page_head;
    page_wrapper_t *page_pre = NULL;
    while (page_cur) {
        kma_page_t *page = page_cur->page;
        if (page->ptr <= ptr && ptr < page->ptr + page->size) {
            kma_size_t bufsize = 1 << (idx + MINPOWER);
            void *tmp;
            for (tmp = page->ptr; tmp < page->ptr + page->size; tmp += bufsize) {
                if (*((void **)tmp) == free_list + idx) {
                    return;
                }
            }
            void *block = free_list[idx];
            free_list[idx] = NULL;
            while (block) {
                void *tmp = block;
                block = *((void **)block);
                if (tmp < page->ptr || tmp > page->ptr + page->size) {
                    *((void **)tmp) = free_list[idx];
                    free_list[idx] = tmp;
                }
            }
            if (!page_pre) {
                page_head = page_cur->next;
            } else {
                page_pre->next = page_cur->next;
            }
            free_page(page_cur->page);
            free(page_cur);
            return;
        }
        page_pre = page_cur;
        page_cur = page_cur->next;
    }
}

#endif // KMA_P2FL
