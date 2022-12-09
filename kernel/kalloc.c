// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

extern void pop_off();
extern void push_off();

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 count;
} kmem[NCPU];

void 
stealfreemem(int myc) {
  for (int cid = 0; cid < NCPU && kmem[cid].lock.name; cid++) {
    acquire(&kmem[cid].lock);
    if (kmem[cid].count > 0) {
      uint64 numofmem = kmem[cid].count / 2;
      struct run firstrun;
      firstrun.next = kmem[cid].freelist;
      struct run *r = &firstrun;
      for (uint64 num = 0; num < numofmem; num++)
        r = r->next;
      acquire(&kmem[myc].lock);
      kmem[myc].count = kmem[cid].count - numofmem;
      kmem[myc].freelist = r->next;
      r->next = 0;
      release(&kmem[myc].lock);
      kmem[cid].count = numofmem;
      kmem[cid].freelist = firstrun.next;
      release(&kmem[cid].lock);
      break;
    }
    release(&kmem[cid].lock);
  }
}

void
kinit()
{
  push_off();
  int myc = cpuid();
  initlock(&kmem[myc].lock, "kmem");
  if (myc == 0)
    freerange(end, (void*)PHYSTOP);
  pop_off();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int myc = cpuid();
  acquire(&kmem[myc].lock);
  r->next = kmem[myc].freelist;
  kmem[myc].freelist = r;
  kmem[myc].count++;
  release(&kmem[myc].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int myc = cpuid();
  acquire(&kmem[myc].lock);
  if (kmem[myc].count == 0) {
    release(&kmem[myc].lock);
    stealfreemem(myc);
    acquire(&kmem[myc].lock);
  }
  r = kmem[myc].freelist;
  if(r) {
    kmem[myc].freelist = r->next;
    kmem[myc].count--;
  }
  release(&kmem[myc].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}
