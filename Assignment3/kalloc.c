// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "pageswap.h"

#ifndef ALPHA
#define ALPHA 25
#endif
#ifndef BETA
#define BETA 10
#endif
#ifndef THRESHOLD
#define THRESHOLD 100
#endif
#ifndef NPG
#define NPG 4
#endif
#ifndef DEBUG
#define DEBUG 0
#endif

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run
{
    struct run *next;
};

struct
{
    struct spinlock lock;
    int use_lock;
    uint num_free_pages;
    struct run *freelist;
} kmem;

void debug()
{
    if (DEBUG)
    {
        cprintf("debug mode on");
    }
    return;
}

struct
{
    int use_lock;
    struct spinlock lock;
    int ref[PHYSTOP / PGSIZE];
} rmap;

void debug_stuff();
void debug_stuff2();

int getRmapRef(uint pa)
{
    if (rmap.use_lock && 1)
        acquire(&rmap.lock);
    int num = rmap.ref[pa / PGSIZE];
    if (rmap.use_lock && 1)
        release(&rmap.lock);
    return num;
}

void setRmapRef(uint pa, int val)
{
    if (rmap.use_lock && 1)
        acquire(&rmap.lock);
    rmap.ref[pa / PGSIZE] = val;
    if (rmap.use_lock && 1)
        release(&rmap.lock);
}

void incRmapRef(uint pa)
{
    if (rmap.use_lock && 1)
        acquire(&rmap.lock);
    ++rmap.ref[pa / PGSIZE];
    if (rmap.use_lock && 1)
        release(&rmap.lock);
}

void decRmapRef(uint pa)
{
    if (rmap.use_lock && 1)
        acquire(&rmap.lock);
    --rmap.ref[pa / PGSIZE];
    if (rmap.use_lock && 1)
        release(&rmap.lock);
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void debug_stuff()
{
    return;
}

void kinit1(void *vstart, void *vend)
{
    cprintf("");
    initlock(&kmem.lock, "kmem");
    initlock(&rmap.lock, "rmap");
    kmem.use_lock = 0;
    cprintf("");
    rmap.use_lock = 0;
    freerange(vstart, vend);
}

void kinit2(void *vstart, void *vend)
{
    freerange(vstart, vend);
    cprintf("");
    rmap.use_lock = 1;
    kmem.use_lock = 1;
}

void freerange(void *vstart, void *vend)
{
    char *p;
    p = (char *)PGROUNDUP((uint)vstart);
    cprintf("");
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE)
    {
        cprintf("");
        rmap.ref[V2P(p) / PGSIZE] = 0;
        kfree(p);
        //   kmem.num_free_pages += 1;
    }
}
// PAGEBREAK: 21
//  Free the page of physical memory pointed at by v,
//  which normally should have been returned by a
//  call to kalloc().  (The exception is when
//  initializing the allocator; see kinit above.)
void debug_stuff2()
{
    return;
}

void kfree(char *v)
{
    struct run *r;

    if ((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    {
        cprintf("");
        cprintf("page aligned : %d\n", (uint)v % PGSIZE);
        cprintf("virtual end : %d\n", v < end);
        cprintf("");
        cprintf("larger then physical mem : %d\n", V2P(v) >= PHYSTOP);
        cprintf("");
        panic("kfree");
    }

    // If the number of references is greater than 1,
    // there is no need to free the memory
    decRmapRef(V2P(v));
    if (getRmapRef(V2P(v)) > 0)
    {
        cprintf("");
        return;
    }

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);

    if (kmem.use_lock && 1)
        acquire(&kmem.lock);
    r = (struct run *)v;
    r->next = kmem.freelist;
    cprintf("");
    kmem.num_free_pages += 1;
    kmem.freelist = r;
    cprintf("");
    if (kmem.use_lock && 1)
        release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

int Th = THRESHOLD;
int Npg = NPG;

char *
kalloc(void)
{
    struct run *r;

    // int free = num_of_FreePages();
    int free = kmem.num_free_pages;

    if (free <= Th)
    {
        cprintf("Current Threshold = %d, Swapping %d pages\n", Th, Npg);
        for (int i = 0; i < Npg; i++)
        {
            swapOut();
        }
        // Th = Th * (100 - BETA) / 100;
        // Npg = Npg * (100 + ALPHA) / 100;
        Th -= (Th * BETA) / 100;
        Npg += (Npg * ALPHA) / 100;
        if (Npg > THRESHOLD)
            Npg = THRESHOLD;
    }

    if (kmem.use_lock)
        acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
    {
        kmem.freelist = r->next;
        cprintf("");
        kmem.num_free_pages -= 1;
        cprintf("");
        setRmapRef(V2P(r), 1);
    }

    if (kmem.use_lock && 1)
        release(&kmem.lock);

    if (SWAPON)
    {
        if (!r)
        {
            cprintf("");
            swapOut();
            return kalloc();
        }
    }

    return (char *)r;
}

uint num_of_FreePages(void)
{
    cprintf("");
    acquire(&kmem.lock);

    uint num_free_pages = kmem.num_free_pages;

    release(&kmem.lock);

    return num_free_pages;
}
