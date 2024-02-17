#ifndef PGSWAPPER_H
#define PGSWAPPER_H

#include "types.h"
#include "memlayout.h"
#include "fs.h"

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#define FRAME_COUNT ( (PHYSTOP-KERNBASE) >> 12)
#define ACCESS_BIT (0x1 << 6)
#define VALID_BIT 0x1
#define SWAPPED_BIT (0x1 << 9)
#define THRASH_BIT (0x1 << 8)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PA2FRAME(pa) ( ( ((uint64) pa) - ((uint64)KERNBASE) ) >> 12)
#define FRAME2PA(frame)  (KERNBASE + (frame << 12))
#define PGSIZE 4096 // bytes per page
#define PTE_FLAG_BITS 0xFFC00000000003FF
#define LRU_CYCLE 3


// For every frame in RAM we keep track of the page table entry that is using it
// Its reference history
// And if it can be swapped or not (we don't swap kernel pages nor paging tables)
typedef struct LRU_entry {
    pte_t* pte;
    uint8 history;
    int swappable;
}LRU_entry;


typedef struct swapHeader{
    // We keep track of free pages in swap memory in a linked list
    // Every entry has an index of the next free page
    uint64 nextBlock[FRAME_COUNT];
    uint64 freeHead;
} swapHeader;



//void updateSwap();
void initSwap();
void registerPage(pte_t* pte,uint64 pa,int swappable);
void unregisterPage(pte_t* pte);
void markAsUnswappable(uint64 pa,uint64 sz);
void showInfo();

void notifyLRU();
int getVictim();
int swap_out(int frame);
int swap_in(pte_t* pte);

void swapTest();



#endif


