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
  struct buf buckets[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock,"bcache");
  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(int i=0; i<NBUCKET; i++) {//每个哈希桶的元素都是一个head，来达到划分buf的目的
                                //同时每一个哈希桶都需要满足双向列表       
    initlock(&bcache.locks[i], "locks");

    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buflock");
    b->lasttick=0;
    b->dev=-1;
    b->blockno=-1;
    b->refcnt=0;
    b->next = bcache.buckets[0].next;
    b->prev = &bcache.buckets[0]; 
    bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int index = blockno % NBUCKET;
  acquire(&bcache.locks[index]);

  // Is the block already cached?
  for(b = bcache.buckets[index].next; b != &bcache.buckets[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.locks[index]);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  
  struct buf *tarbuf=0;
  uint minticks=ticks;
  acquire(&bcache.lock);
  acquire(&bcache.locks[index]);
  for(b = bcache.buckets[index].next; b != &bcache.buckets[index]; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
            // found in hash table
            // increase ref count
            b->refcnt++;
            release(&bcache.locks[index]);
            release(&bcache.lock);
            // acquire sleep lock
            acquiresleep(&b->lock);
            return b;
        }
    }
  for(b = bcache.buckets[index].next; b != &bcache.buckets[index]; b = b->next){
    if(b->refcnt==0&&b->lasttick<minticks){//LRU
      minticks=b->lasttick;
      tarbuf=b;
    }
  }
  if(!tarbuf)//此时需要向其他哈希桶中借buf
  {
    
    for(int i=0; i<NBUCKET; i++) {
      if(i == index)
        continue;

      acquire(&bcache.locks[i]);
      minticks = ticks;
      for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
        if(b->refcnt==0 && b->lasttick<=minticks) {
          minticks = b->lasttick;
          tarbuf = b;
        }
      }
      if(tarbuf){
        tarbuf->dev = dev;
        tarbuf->blockno = blockno;
        tarbuf->valid = 0;
        tarbuf->refcnt = 1;

        //将其从i号桶中取出，双向列表的删除操作
        tarbuf->next->prev = tarbuf->prev;
        tarbuf->prev->next = tarbuf->next;
        

        //将其放入index对应的桶,双向列表的插入操作
        tarbuf->next = bcache.buckets[index].next;
        bcache.buckets[index].next->prev = tarbuf;
        bcache.buckets[index].next = tarbuf;
        tarbuf->prev = &bcache.buckets[index];
        release(&bcache.locks[i]);
        release(&bcache.locks[index]);//释放锁
        release(&bcache.lock);
        acquiresleep(&tarbuf->lock);
        return tarbuf;
      }
      release(&bcache.locks[i]);//释放该锁，继续循环
    }
    release(&bcache.locks[index]);//释放该锁
    release(&bcache.lock);
    panic("bget: no buffers");
  }
  //此时tarbuf不为空，不需要借
  //初始化
  tarbuf->dev = dev;
  tarbuf->blockno = blockno;
  tarbuf->valid = 0;
  tarbuf->refcnt = 1;
  release(&bcache.locks[index]);
  release(&bcache.lock);
  acquiresleep(&tarbuf->lock);
  return tarbuf;


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
  int index=b->blockno%NBUCKET;
  
  acquire(&bcache.locks[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    //b->next->prev = b->prev;
    //b->prev->next = b->next;
    //b->next = bcache.buckets[index].next;
    //b->prev = &bcache.buckets[index];
    //bcache.buckets[index].next->prev = b;
    //bcache.buckets[index].next = b;
    b->lasttick=ticks;
  }
  
  release(&bcache.locks[index]);
}

void
bpin(struct buf *b) {
  int index=b->blockno%NBUCKET;
  acquire(&bcache.locks[index]);
  b->refcnt++;
  release(&bcache.locks[index]);
}

void
bunpin(struct buf *b) {
  int index=b->blockno%NBUCKET;
  acquire(&bcache.locks[index]);
  b->refcnt--;
  release(&bcache.locks[index]);
}


