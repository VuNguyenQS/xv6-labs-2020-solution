// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

#define NBUCK 13

struct {
  struct spinlock lock[NBUCK];
  struct buf buck[NBUCK];
} htable;

void
binit(void)
{
  struct buf *b;
  struct buf *run = htable.buck;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){ 
    initsleeplock(&b->lock, "buffer");
    run->samehash = b;
    run = b;
  }
  for (int i = 0; i < NBUCK; i++)
    initlock(htable.lock + i, "bcache.bucket");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  //find the right bucket
  int hash = blockno % NBUCK;
  acquire(htable.lock + hash);
  //Look up the block in hash bucket
  struct buf *found = 0;
  for (struct buf *run = htable.buck[hash].samehash; run; run = run->samehash) {
    if (run->blockno == blockno && run->dev == dev) {
      run->refcnt++;
      release(htable.lock + hash);
      //release(&bcache.lock);
      acquiresleep(&run->lock);
      return run;
    }
    if (run->refcnt == 0) {
      if (found) {
        if (found->time < run->time)
          found = run;
      }
      else found = run;
    }
  }
  
  if (found) {
    found->blockno = blockno;
    found->dev = dev;
    found->valid = 0;
    found->refcnt = 1;
    release(htable.lock + hash);
    acquiresleep(&found->lock);
    return found;
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    if (b->refcnt == 0) {
      hash = b->blockno % NBUCK;
      acquire(htable.lock + hash);
      if (b->refcnt == 0){
        struct buf * run = htable.buck + hash;
        for (; run->samehash != b; run = run->samehash);
        run->samehash = b->samehash;
        release(htable.lock + hash);
        b->refcnt = 1;
        release(&bcache.lock);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        //put it on new hashbucket
        hash = blockno % NBUCK;
        b->samehash = htable.buck[hash].samehash;
        htable.buck[hash].samehash = b;
        release(htable.lock + hash);
        acquiresleep(&b->lock);
        return b;
      }
      release(htable.lock + hash);
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int hash = b->blockno % NBUCK;
  acquire(htable.lock + hash);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->time = ticks;
  } 
  release(htable.lock + hash);
}

void
bpin(struct buf *b) {
  acquire(htable.lock + b->blockno % NBUCK);
  b->refcnt++;
  release(htable.lock + b->blockno % NBUCK);
}

void
bunpin(struct buf *b) {
  acquire(htable.lock + b->blockno % NBUCK);
  b->refcnt--;
  release(htable.lock + b->blockno % NBUCK);
}


