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

#define PTRSIZE sizeof(void *)
#define INTSIZE sizeof(kma_size_t)


typedef struct page_wrapper_t {
    void *fb_head;
    kma_page_t *page;
    struct page_wrapper_t *next;
} page_wrapper_t;

page_wrapper_t page_stat[MAXPAGES];
page_wrapper_t *page_head = NULL;
page_wrapper_t *free_head = NULL;



void *kma_malloc(kma_size_t size) {
    if (!page_head && !free_head) {
        free_head = page_stat;
        (page_stat[MAXPAGES - 1]).next = NULL;
        int i;
        for (i = 0; i < MAXPAGES - 1; i++) {
            (page_stat[i]).next = page_stat + i + 1;
        }
    }
    if (!page_head) {
        page_head = free_head;
        free_head = free_head->next;
        kma_page_t *page = get_page();
        *((void **)page->ptr) = NULL;
        *((kma_size_t *)(page->ptr + PTRSIZE)) = page->size - PTRSIZE - INTSIZE;
        page_head->page = page;
        page_head->fb_head = page->ptr;
        page_head->next = NULL;
    }
    page_wrapper_t *page_pre = NULL;
    page_wrapper_t *page_cur = page_head;
    while (page_cur) {
        void *fb_pre = NULL;
        void *fb_cur = page_cur->fb_head;
        while (fb_cur) {
            if (*((kma_size_t *)(fb_cur + PTRSIZE)) >= size) {
                void *space = fb_cur;
                fb_cur += size;
                *((void **)fb_cur) = *((void **)space);
                *((kma_size_t *)(fb_cur + PTRSIZE)) = *((kma_size_t *)(space + PTRSIZE)) - size;
                if (fb_pre == NULL) {
                    page_cur->fb_head = fb_cur;
                } else {
                    *((void **)fb_pre) = fb_cur;
                }
                return space;
            }
            fb_pre = fb_cur;
            fb_cur = *((void **)fb_cur);
        }
        page_pre = page_cur;
        page_cur = page_cur->next;
    }
    page_cur = free_head;
    free_head = free_head->next;
    page_pre->next = page_cur;
    kma_page_t *page = get_page();
    page_cur->page = page;
    page_cur->next = NULL;
    *((void **)(page->ptr + size)) = NULL;
    *((kma_size_t *)(page->ptr + size + PTRSIZE)) = page->size - size - PTRSIZE - INTSIZE;
    page_cur->fb_head = page->ptr + size;
    return page->ptr;
}

void kma_free(void* ptr, kma_size_t size) {
    page_wrapper_t *page_pre = NULL;
    page_wrapper_t *page_cur = page_head;
    while (page_cur) {
        if (page_cur->page->ptr <= ptr && ptr + size <= page_cur->page->ptr + page_cur->page->size) {
            void *fb_pre = NULL;
            void *fb_cur = page_cur->fb_head;
            while (fb_cur) {
                if (ptr < fb_cur) {
                    if (!fb_pre) {
                        if (ptr + size == fb_cur) {
                            void *space = fb_cur;
                            fb_cur = ptr;
                            *((void **)fb_cur) = *((void **)space);
                            *((kma_size_t *)(fb_cur + PTRSIZE)) = *((kma_size_t *)(space + PTRSIZE)) + size;
                        } else {
                            *((void **)ptr) = fb_cur;
                            *((kma_size_t *)(ptr + PTRSIZE)) = size - PTRSIZE - INTSIZE;
                        }
                        page_cur->fb_head = ptr;
                    } else {
                        if (fb_cur - fb_pre - *((kma_size_t *)(fb_pre + PTRSIZE)) - PTRSIZE - INTSIZE == size) {
                            *((void **)fb_pre) = *((void **)fb_cur);
                            *((kma_size_t *)(fb_pre + PTRSIZE)) += size + *((kma_size_t *)(fb_cur + PTRSIZE)) + PTRSIZE + INTSIZE;
                        } else if (ptr + size == fb_cur) {
                            void *space = fb_cur;
                            fb_cur = ptr;
                            *((void **)fb_cur) = *((void **)space);
                            *((kma_size_t *)(fb_cur + PTRSIZE)) = *((kma_size_t *)(space + PTRSIZE)) + size;
                            *((void **)fb_pre) = fb_cur;
                        } else if (ptr == fb_pre + PTRSIZE + INTSIZE + *((kma_size_t *)(fb_pre + PTRSIZE))) {
                            *((kma_size_t *)(fb_pre + PTRSIZE)) += size;
                        } else {
                            *((void **)ptr) = fb_cur;
                            *((kma_size_t *)(ptr + PTRSIZE)) = size - PTRSIZE - INTSIZE;
                            *((void **)fb_pre) = ptr;
                        }
                    }
                    if (page_cur->fb_head == page_cur->page->ptr && *((kma_size_t *)(page_cur->fb_head + PTRSIZE)) + PTRSIZE + INTSIZE == page_cur->page->size) {
                        if (!page_pre) {
                            page_head = page_cur->next;
                        } else {
                            page_pre->next = page_cur->next;
                        }
                        free_page(page_cur->page);
                        page_cur->next = free_head;
                        free_head = page_cur;
                    }
                    return;
                }
                fb_pre = fb_cur;
                fb_cur = *((void **)fb_cur);
            }
        }
        page_pre = page_cur;
        page_cur = page_cur->next;
    }
}

#endif // KMA_RM