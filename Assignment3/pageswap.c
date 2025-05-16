#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h" // <-- ADD THIS
#include "mmu.h"       // <-- UNCOMMENT THIS
#include "spinlock.h"  // <-- UNCOMMENT THIS
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "swap.h"
#include "x86.h"
#define SWAPPAGES (SWAPBLOCKS / 8)

#ifndef DEBUG
#define DEBUG 0
#endif

struct swapinfo
{
    int pid;
    int page_perm;
    int is_free;
};

struct swapinfo swp[SWAPPAGES];

// buffer

void debugger31()
{
    if (DEBUG)
    {
        cprintf("Debug mode on\n");
    }
}

void swapInit(void)
{
    for (int i = 0; i < SWAPPAGES; i++)
    {
        if (DEBUG)
        {
            cprintf("[SWAPINIT] Swap slot %d initialized\n", i);
        }
        swp[i].page_perm = 0;
        swp[i].is_free = 1;
    }
}

void swapOut()
{
    struct superblock sb;
    readsb(ROOTDEV, &sb);
    // find victim page
    cprintf("");
    struct proc *p = find_victim_process();
    uint victim_page_VA = find_victim_page(p);
    if (DEBUG)
    {
        cprintf("[SWAPOUT] Victim process id : %d\n", p->pid);
        cprintf("[SWAPOUT] Victim page VA : %p\n", victim_page_VA);
    }
    pte_t *victim_pte = walkpgdir(p->pgdir, (void *)victim_page_VA, 0);

    int i;
    // find free swap slot
    for (i = 0; i < SWAPPAGES; ++i)
    {
        if (DEBUG)
        {
            cprintf("[SWAPOUT] Swap slot %d\n", i);
        }
        if (swp[i].is_free)
        {
            if (DEBUG)
            {
                cprintf("[SWAPOUT] Swap slot %d is free\n", i);
            }
            break;
        }
    }
    cprintf("");
    // cprintf("[SWAPOUT] Current slot : %d\n", i);
    swp[i].page_perm = 0;
    swp[i].is_free = 0;
    cprintf("");
    swp[i].pid = p->pid;

    // update swap slot permissions to match victim page permissions
    swp[i].page_perm = PTE_FLAGS(*victim_pte);
    if (DEBUG)
    {
        cprintf("[SWAPOUT] Victim page permissions : %d\n", swp[i].page_perm);
    }
    cprintf("");
    // cprintf("Victim page addr : %p\n", PTE_ADDR(*victim_page_VA));
    char *pa = (char *)P2V(PTE_ADDR(*victim_pte));
    uint addressOffset;
    for (int j = 0; j < 8; ++j)
    {
        if (DEBUG)
        {
            cprintf("[SWAPOUT] Victim page copied Block no : %d\n", sb.swapstart + (8 * i) + j);
        }
        // cprintf("Victim page copied Block no : %d\n", sb.swapstart + (8 * i) + j);
        addressOffset = PTE_ADDR(*victim_pte) + (j * BSIZE);
        struct buf *b = bread(ROOTDEV, sb.swapstart + (8 * i) + j);
        if (DEBUG)
        {
            cprintf("[SWAPOUT] Victim page copied Block no : %d\n", sb.swapstart + (8 * i) + j);
        }
        memmove(b->data, (void *)P2V(addressOffset), BSIZE);
        b->blockno = (sb.swapstart + (8 * i) + j);
        b->flags |= B_DIRTY;
        b->dev = ROOTDEV;
        if (DEBUG)
        {
            cprintf("[SWAPOUT] Victim page copied Block no : %d\n", b->blockno);
        }
        bwrite(b);
        brelse(b);
        cprintf("");
    }
    cprintf("");
    // cprintf("[SWAPOUT] Swap slot used : %d\n", sb.swapstart + 8 * i);
    (*victim_pte) = ((sb.swapstart + (8 * i)) << 12) & (~0xFFF);
    *victim_pte &= ~PTE_P;
    cprintf("");
    lcr3(V2P(p->pgdir));
    // cprintf("[SWAPOUT] Swap slot block no : %d\n", *victim_pte >> 12);
    // cprintf("[SWAPOUT] VA : %p\n", pa);
    // cprintf("[SWAPOUT] kfree\n");
    if (DEBUG)
    {
        cprintf("[SWAPOUT] Victim page freed : %p\n", pa);
    }
    kfree(pa);
    // cprintf("[SWAPOUT] Page freed by swapOut\n");
    // return (char *)victim_page_VA;
}

void swapIn(char *memory)
{
    struct proc *p = myproc();
    uint addr = rcr2();
    cprintf("");
    // cprintf("[SWAPIN] Process is requesting page : %p\n", V2P(addr));
    pde_t *pd = p->pgdir;
    pte_t *pg = walkpgdir(pd, (void *)(addr), 0);

    uint swapSlot = (*pg >> 12); // physical address of swap block
    cprintf("");
    // cprintf("[SWAPIN] Swap slot block no : %p\n", swapSlot);
    int swapIdx = (swapSlot - 2) / 8;
    if (DEBUG)
    {
        cprintf("Swap Slot :%d\n", swapIdx);
        cprintf("[SWAPIN] Swap slot index : %d\n", swapIdx);
    }

    *pg = (V2P((uint)memory) & (~0xFFF)) | swp[swapIdx].page_perm;
    // int swapSlot = swap;         // swap block number (convert it to integer)
    for (int i = 0; i < 8; i++) // writes page into physical memory
    {
        if (DEBUG)
        {
            cprintf("[SWAPIN] Swap slot %d read\n", swapSlot + i);
        }
        struct buf *b = bread(ROOTDEV, swapSlot + i);
        cprintf("");
        memmove(memory, b->data, BSIZE);
        brelse(b);
        memory += BSIZE;
        if (DEBUG)
        {
            cprintf("memory : %p\n", memory);
        }
    }
    swp[swapIdx].is_free = 1;
    swp[swapIdx].pid = 0;
    if (DEBUG)
    {
        cprintf("[SWAPIN] Swap slot freed : %d\n", swapIdx);
    }
    // cprintf("[SWAPIN] Swap slot freed : %d\n", swapIdx);
    // swp[swapSlot].page_perm = 0;
}

void PrintingSwapFree()
{
    cprintf("");
    cprintf("[SWAPIN] Free swap slots\n");
    for (int i = 0; i < SWAPPAGES; i++)
    {
        if (swp[i].is_free)
        {
            cprintf("[SWAPIN] Swap slot %d is free\n", i);
        }
    }
}

void freeSwapSlot(int pid)
{
    struct superblock sb;
    readsb(ROOTDEV, &sb);
    int i;
    for (i = 0; i < SWAPPAGES; i++)
    {
        if (DEBUG)
        {
            cprintf("[FREESWAP] Swap slot %d is free\n", i);
        }
        if (swp[i].pid == pid)
            break;
    }
    for (int j = 0; j < 8; j++)
    {
        if (DEBUG)
        {
            cprintf("[FREESWAP] Swap slot %d freed\n", sb.swapstart + (8 * i) + j);
        }
        struct buf *b = bread(ROOTDEV, sb.swapstart + (8 * i) + j);
        memset(b->data, 0, BSIZE);
        brelse(b);
    }
    cprintf("");
    swp[i].is_free = 0;
    swp[i].pid = 0;
}

void PrintSwap()
{
    struct proc *p = myproc();
    cprintf("");
    cprintf("[SWAPIN] Process id : %d\n", p->pid);
    cprintf("[SWAPIN] Process name : %s\n", p->name);
    cprintf("[SWAPIN] Process state : %d\n", p->state);
    cprintf("[SWAPIN] Process rss : %d\n", p->rss);
}