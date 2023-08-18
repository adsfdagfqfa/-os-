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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int refcount[PHYSTOP/PGSIZE];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    refcount[(uint64)p / PGSIZE] = 1;//初始化阶段refcount内部元素都为0，
    //此时调用kfree会执行panic
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  acquire(&kmem.lock);
  int pn = (uint64)pa / PGSIZE;
  if(refcount[pn]<1)
    panic("kfree");//若引用计数小于1，则执行panic
  refcount[pn]--;
  int tmp = refcount[pn];
  release(&kmem.lock);
  if(tmp>0)
    return;//只有当引用计数为1时，此时temp应该为0，才执行释放页的操作。
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
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
  if(r){
    kmem.freelist = r->next;
    //当分配一个页面时，需要将引用计数设置为1 
    int pn=(uint64)r/PGSIZE;
    if(refcount[pn]!=0){//若分配时，该页面引用计数不为0，执行panic
      panic("kalloc");
    }
    refcount[pn]=1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int getrefcount(void *pa){//获取计数
  acquire(&kmem.lock);
  int pn = (uint64)pa/PGSIZE;
  int tmp = refcount[pn];
  release(&kmem.lock);
  return tmp;
}

void incr(void *pa){//增加引用
  acquire(&kmem.lock);
  int pn = (uint64)pa/PGSIZE;
  if((uint64)pa>=PHYSTOP||refcount[pn]<1)
    panic("incr");

  refcount[pn]++;
  release(&kmem.lock);
}
//用于在uvmcopy函数映射时调用。
//释放页时会根据其引用的数值决定是否释放页