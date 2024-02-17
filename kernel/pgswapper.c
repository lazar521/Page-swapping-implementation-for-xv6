#include "pgswapper.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PG2BLK(pg) (pg * (PGSIZE/BSIZE))

static int LRU_ticks = 0;
static SWAP_INITIALIZED = 0;
static LRU_entry LRU[FRAME_COUNT];
static swapHeader swapfile;


static int doPrint = 0;
static int swapsDone = 0;

static struct spinlock swapLock;

// flush the TLB.
static inline void
sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}


static int swappedPagesCnt = 0;


void initSwap(){
    for(int i=0;i<FRAME_COUNT;i++){
        LRU[i].pte = 0;
        LRU[i].history = 0;
        LRU[i].swappable = 0;
    }

    for(int i=0;i<FRAME_COUNT-1;i++){
        swapfile.nextBlock[i] = i+1;
    }
    swapfile.nextBlock[FRAME_COUNT-1] = -1;

    swapfile.freeHead = 0;
    SWAP_INITIALIZED = 1;

    printf("\nSwap initalized\n");
}

void notifyLRU(){
    LRU_ticks++;
    if(LRU_ticks != LRU_CYCLE || !SWAP_INITIALIZED) return;
    LRU_ticks = 0;
    updateLRU();
}

void updateLRU(){
    uint8 refBit;
    for(int i=0;i<FRAME_COUNT;i++){
        if(LRU[i].swappable == 1 && LRU[i].pte != 0){
            refBit = (*LRU[i].pte & ACCESS_BIT) << 1;           // extract reference bit
            LRU[i].history = (LRU[i].history >> 1) | refBit;    // update reference history
            *LRU[i].pte &= (~ACCESS_BIT);                       // clear the access bit
        }
    }
}


// Register the page in our LRU table
void registerPage(pte_t* pte,uint64 pa,int swappable){
    if(!SWAP_INITIALIZED) return;
    if(pa < KERNBASE || pa >= PHYSTOP) return;

    uint64 frame = PA2FRAME(pa);

    LRU[frame].pte = pte;
    LRU[frame].history = 0x80;
    LRU[frame].swappable = swappable;
}


int freeSwapBlock(int blockNo);


void unregisterPage(pte_t* pte){
    if(!SWAP_INITIALIZED) return;
    if(pte == 0) return;

    // if page is currently in swap
    if(*pte & SWAPPED_BIT){
        //extract swapBlock number from page table entry
        int swapBlock = (*pte & (~PTE_FLAG_BITS)) >> 10;
        freeSwapBlock(swapBlock);
    }
    else {
        uint64 pa = PTE2PA(*pte);
        uint64 frame = PA2FRAME(pa);
        LRU[frame].pte = 0;
        LRU[frame].history = 0;
        LRU[frame].swappable = 0;
    }

}


int getVictim(){
    int victim = -1;
    int victim_history=0xFF;

    acquire(&swapLock);

    for(int i=0;i<FRAME_COUNT;i++){
        if(LRU[i].swappable == 0) continue;

        // Find the frame with the lowest reference history
        if(victim == -1 || LRU[i].history < victim_history ){
            victim = i;
            victim_history = LRU[i].history;
            if(victim_history == 0) break;
        }
    }

    if(victim != -1) LRU[victim].swappable = 0;

    release(&swapLock);

    return victim;
}

// Returns the allocated page number in swap
int allocateSwapBlock(){
    acquire(&swapLock);

    int block = swapfile.freeHead;
    if(block == -1){
        release(&swapLock);
        return -1;
    }

    swapfile.freeHead = swapfile.nextBlock[block];
    swapfile.nextBlock[block] = -1;

    swappedPagesCnt++;

    release(&swapLock);

    return block;
}

// Frees the give swap page
int freeSwapBlock(int pageNo){
    if(pageNo < 0 || pageNo >= FRAME_COUNT) return -1;


    acquire(&swapLock);

    swapfile.nextBlock[pageNo] = swapfile.freeHead;
    swapfile.freeHead = pageNo;

    swappedPagesCnt--;

    release(&swapLock);
    return 0;
}


// The blocks in swap are of size BSIZE, but our pages in RAM are of size PGSIZE
void write_swap(int swapBlock,uchar* pa, int busy_wait){
    for(int i=0;i<PGSIZE/BSIZE;i++){
        write_block(swapBlock, pa,busy_wait);
        pa += BSIZE;
        swapBlock++;
    }
}

// The blocks in swap are of size BSIZE, but our pages in RAM are of size PGSIZE
void read_swap(int swapBlock,uchar* pa, int busy_wait){
    for(int i=0;i<PGSIZE/BSIZE;i++){
        read_block(swapBlock, pa,busy_wait);
        pa += BSIZE;
        swapBlock++;
    }
}

int swap_out(int frame){
    if(!SWAP_INITIALIZED) return -1;
    pte_t* pte = LRU[frame].pte;


    if(pte == 0) panic("\nswap_out: Invalid pte\n");
    int swapPage = allocateSwapBlock();


    if(swapPage == -1) return -1;

    write_swap(PG2BLK(swapPage), FRAME2PA(frame),1);

    *pte &= ~VALID_BIT;          // clear VALID_BIT
    *pte &= ~ACCESS_BIT;

    *pte |= SWAPPED_BIT;         // set SWAPPED_BIT to 1
    *pte &= PTE_FLAG_BITS;       // clear PTE address bits
    *pte |= swapPage << 10;     // insert swapBlock number instead

    LRU[frame].pte = 0;
    LRU[frame].history = 0;
    LRU[frame].swappable = 0;

    sfence_vma();
    return 0;
}

int swap_in(pte_t* pte){
    if(!SWAP_INITIALIZED) return -1;

    if(pte == 0) panic("\nswap_in: Invalid pte\n");

    uint8* mem = kalloc();
    if(mem == 0) return -1;

    // Extract the swap block number where the swapped page is located
    int swapPage = (*pte & (~PTE_FLAG_BITS)) >> 10;

    read_swap(PG2BLK(swapPage),mem,1);

    freeSwapBlock(swapPage);

    *pte &= (~SWAPPED_BIT);      // clear SWAPPED_BIT
    *pte &= PTE_FLAG_BITS;       // clear PTE address bits
    *pte |= PA2PTE(mem);         // set address of newly allocated memory
    *pte |= VALID_BIT;

    // register the page again
    int frame = PA2FRAME(mem);
    LRU[frame].pte = pte;
    LRU[frame].history = 0x80;
    LRU[frame].swappable = 1;

    sfence_vma();
    return 0;
}

