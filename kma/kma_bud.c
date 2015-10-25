/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
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
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *

 ************************************************************************/

/************************************************************************
Project Group: NetID1, NetID2, NetID3

***************************************************************************/

#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <string.h>
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

#define MINPOWER 5
#define FL_SIZE 8
#define BM_SIZE (PAGESIZE >> MINPOWER) / (sizeof(kma_size_t) << 3)


typedef struct page_wrapper_t {
    void *free_list[FL_SIZE];
    kma_size_t bitmap[BM_SIZE];
    kma_page_t *page;
    struct page_wrapper_t *next;
} page_wrapper_t;



page_wrapper_t *main_entry = NULL;

kma_size_t idx(size) {
    return
        size > 256
            ? size > 512
                ? size > 1024
                    ? size > 2048
                        ? size > 4096 ? 8 : 7
                        : 6
                    : 5
                : 4
            : size > 128
                ? 3
                : size > 64
                    ? 2
                    : size > 32 ? 1 : 0;
}

void set_bitmap(kma_size_t *bitmap, kma_size_t offset, kma_size_t length, kma_size_t value) {
    kma_size_t i;
    kma_size_t s = offset >> 5;
    kma_size_t e = (offset + length - 1) >> 5;
    kma_size_t sv = 0xFFFFFFFF >> (offset & 0x0000001F);
    kma_size_t ev = 0xFFFFFFFF << (0x00000020 - ((offset + length) & 0x0000001F));
    kma_size_t vv = 0xFFFFFFFF;
    if (value) {
        if (s == e) {
            bitmap[s] |= (sv & ev);
        } else {
            bitmap[s] |= sv;
            bitmap[e] |= ev;
            for (i = s + 1; i <= e - 1; i++) {
                bitmap[i] |= vv;
            }
        }
    } else {
        sv = ~sv;
        ev = ~ev;
        vv = ~vv;
        if (s == e) {
            bitmap[s] &= (sv | ev);
        } else {
            bitmap[s] &= sv;
            bitmap[e] &= ev;
            for (i = s + 1; i <= e - 1; i++) {
                bitmap[i] &= vv;
            }
        }
    }
}

kma_size_t check_buddy(kma_size_t *bitmap, kma_size_t buddy_offset, kma_size_t bufsize) {
    kma_size_t i;
    for (i = buddy_offset; i < buddy_offset + bufsize; i++) {
        if (bitmap[i >> 5] & (0x80000000 >> (i & 0x0000001F))) {
            return FALSE;
        }
    }
    return TRUE;
}

void merge_free(page_wrapper_t *pw, void *ptr, kma_size_t index) {
    kma_size_t i = index, j;
    kma_size_t offset = (ptr - pw->page->ptr) >> MINPOWER;
    kma_size_t buddy_offset = ((offset >> i) & 0x00000001) ? offset - (1 << i) : offset + (1 << i);
    while (check_buddy(pw->bitmap, buddy_offset, 1 << i)) {
        i++;
        offset = offset < buddy_offset ? offset : buddy_offset;
        buddy_offset = ((offset >> i) & 0x00000001) ? offset - (1 << i) : offset + (1 << i);
    }
    offset <<= MINPOWER;
    kma_size_t bufsize = 1 << (i + MINPOWER);
    for (j = index; j < i; j++) {
        void *fb_pre = NULL;
        void *fb_cur = pw->free_list[j];
        while (fb_cur) {
            if (pw->page->ptr + offset <= fb_cur && fb_cur < pw->page->ptr + offset + bufsize) {
                if (fb_pre) {
                    *((void **)fb_pre) = *((void **)fb_cur);
                } else {
                    pw->free_list[j] = *((void **)fb_cur);
                }
            }
            fb_pre = fb_cur;
            fb_cur = *((void **)fb_cur);
        }
    }
    *((void **)(pw->page->ptr + offset)) = pw->free_list[i];
    pw->free_list[i] = pw->page->ptr + offset;
}

page_wrapper_t *init_large_page_wrapper() {
    kma_page_t *page = get_page();
    page_wrapper_t *pw = (page_wrapper_t *)page->ptr;
    memset(pw->free_list, 0, FL_SIZE * sizeof(void *));
    //memset(pw->bitmap, 0xFFFFFFFF, BM_SIZE * sizeof(kma_size_t));
    pw->page = page;
    pw->next = NULL;
    return pw;
}

page_wrapper_t *init_page_wrapper() {
    kma_page_t *page = get_page();
    page_wrapper_t *pw = (page_wrapper_t *)page->ptr;
    memset(pw->free_list, 0, FL_SIZE * sizeof(void *));
    memset(pw->bitmap, 0, BM_SIZE * sizeof(kma_size_t));
    pw->page = page;
    pw->next = NULL;
    kma_size_t index = idx(sizeof(page_wrapper_t));
    set_bitmap(pw->bitmap, 0, 1 << index, 1);
    kma_size_t i = FL_SIZE;
    while (i-- > index) {
        kma_size_t offset = 1 << (MINPOWER + i);
        *((void **)(page->ptr + offset)) = pw->free_list[i];
        pw->free_list[i] = page->ptr + offset;
    }
    return pw;
}

kma_size_t is_empty(kma_size_t *bitmap) {
    kma_size_t i;
    for (i = 1; i < BM_SIZE; i++) {
        if (bitmap[i]) {
            return FALSE;
        }
    }
    kma_size_t index = idx(sizeof(page_wrapper_t));
    if (bitmap[0] != ~(0xFFFFFFFF >> (1 << index))) {
        return FALSE;
    }
    return TRUE;
}

void* kma_malloc(kma_size_t size) {
    if (!main_entry) {
        main_entry = init_page_wrapper();
    }
    if (size > PAGESIZE / 2) {
        page_wrapper_t *pw = init_large_page_wrapper();
        pw->next = main_entry;
        main_entry = pw;
        return pw->page->ptr + sizeof(page_wrapper_t);
    }
    page_wrapper_t *pw_pre = NULL;
    page_wrapper_t *pw_cur = main_entry;
    kma_size_t index = idx(size);
    kma_size_t i;
    while (pw_cur) {
        for (i = index; i < FL_SIZE; i++) {
            if (pw_cur->free_list[i]) {
                void *space = pw_cur->free_list[i];
                pw_cur->free_list[i] = *((void **)pw_cur->free_list[i]);
                while (i-- > index) {
                    kma_size_t offset = 1 << (MINPOWER + i);
                    *((void **)(space + offset)) = pw_cur->free_list[i];
                    pw_cur->free_list[i] = space + offset;
                }
                set_bitmap(pw_cur->bitmap, (space - pw_cur->page->ptr) >> MINPOWER, 1 << index, 1);
                return space;
            }
        }
        pw_pre = pw_cur;
        pw_cur = pw_cur->next;
    }
    pw_cur = init_page_wrapper();
    pw_pre->next = pw_cur;
    for (i = index; i < FL_SIZE; i++) {
        if (pw_cur->free_list[i]) {
            void *space = pw_cur->free_list[i];
            pw_cur->free_list[i] = *((void **)pw_cur->free_list[i]);
            while (i-- > index) {
                kma_size_t offset = 1 << (MINPOWER + i);
                *((void **)(space + offset)) = pw_cur->free_list[i];
                pw_cur->free_list[i] = space + offset;
            }
            set_bitmap(pw_cur->bitmap, (space - pw_cur->page->ptr) >> MINPOWER, 1 << index, 1);
            return space;
        }
    }
    return NULL;
}


void kma_free(void* ptr, kma_size_t size) {
    page_wrapper_t *pw_pre = NULL;
    page_wrapper_t *pw_cur = main_entry;
    kma_size_t index = idx(size);
    while (pw_cur) {
        if (pw_cur->page->ptr <= ptr && ptr < pw_cur->page->ptr + pw_cur->page->size) {
            if (size > PAGESIZE / 2) {
                if (pw_pre) {
                    pw_pre->next = pw_cur->next;
                } else {
                    main_entry = pw_cur->next;
                }
                free_page(pw_cur->page);
                return;
            }
            set_bitmap(pw_cur->bitmap, (ptr - pw_cur->page->ptr) >> MINPOWER, 1 << index, 0);
            merge_free(pw_cur, ptr, index);
            if (is_empty(pw_cur->bitmap)) {
                if (pw_pre) {
                    pw_pre->next = pw_cur->next;
                } else {
                    main_entry->next = pw_cur->next;
                }
                free_page(pw_cur->page);
            }
            return;
        }
        pw_pre = pw_cur;
        pw_cur = pw_cur->next;
    }
}

#endif // KMA_BUD
