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

#define PGIDX(pa) ((pa - freestart)/PGSIZE)
uint64 freestart;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uchar *countrefs;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  
  freestart  = PGROUNDUP((uint64)(end + (PHYSTOP - PGROUNDUP((uint64)end))/PGSIZE));
  kmem.countrefs = (uchar*) end;
  
  freerange((void *)freestart, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  struct run *r = 0, *head = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    head = (struct run *) p;
    head->next = r;
    r = head;
  }
  kmem.freelist = head;
    //kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < freestart || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (kmem.countrefs[PGIDX((uint64)pa)] <= 0)
    panic("free a memory have count <=0");
  if ((--kmem.countrefs[PGIDX((uint64)pa)]) == 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    if (kmem.countrefs[PGIDX((uint64) r)] != 0)
      panic("alloc a free memory have count not zero");
    kmem.freelist = r->next;
    kmem.countrefs[PGIDX((uint64) r)] = 1;
  }
  release(&kmem.lock);

  if(r) 
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}

void
reference(uint64 pa)
{
  if((pa % PGSIZE) != 0 || pa < freestart || pa >= PHYSTOP)
    panic("unvalid pa in refs");
  acquire(&kmem.lock);
  if (kmem.countrefs[PGIDX(pa)] < 1) {
    printf("%d\n", kmem.countrefs[PGIDX(pa)]);
    panic("refer a free memory from reference");
  }
  kmem.countrefs[PGIDX(pa)]++;
  release(&kmem.lock);
}

uint64
kcopy(uint64 pa) {
  struct run *r;
  if((pa % PGSIZE) != 0 || pa < freestart || pa >= PHYSTOP)
    panic("unvalid pa in refs");
  acquire(&kmem.lock);
  if (kmem.countrefs[PGIDX(pa)] < 1){
    printf("%d\n", kmem.countrefs[PGIDX(pa)]);
    panic("refer a free memory");
  }
  if (kmem.countrefs[PGIDX(pa)] == 1) {
    release(&kmem.lock);
    return pa;
  }
  kmem.countrefs[PGIDX(pa)]--;
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.countrefs[PGIDX((uint64) r)] = 1;
  }
  release(&kmem.lock);
  if(r) 
    //memset((char*)r, 5, PGSIZE); // fill with junk
    memmove(r, (void *) pa, PGSIZE);
  return (uint64) r;
}
