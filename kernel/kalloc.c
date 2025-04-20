#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void superfreerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem, superkmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)SUPERPGSTART);
  initlock(&superkmem.lock, "superkmem");
  superfreerange((void*)SUPERPGSTART, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

void superfreerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char*)SUPERPGROUNDUP((uint64)pa_start);
  for (; p + SUPERPGSIZE <= (char*)pa_end; p += SUPERPGSIZE)
    ksuperfree(p);
}

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= SUPERPGSTART)
    panic("kfree");

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void ksuperfree(void *pa) {
  struct run *r;
  if(((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa < SUPERPGSTART || (uint64)pa >= PHYSTOP)
    panic("ksuperfree");
  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&superkmem.lock);
  r->next = superkmem.freelist;
  superkmem.freelist = r;
  release(&superkmem.lock);
}

void*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}

void*
ksuperalloc(void) {
  struct run *r;

  acquire(&superkmem.lock);
  r = superkmem.freelist;
  if(r)
    superkmem.freelist = r->next;
  release(&superkmem.lock);

  if(r)
    memset((char*)r, 5, SUPERPGSIZE);
  return (void*)r;
}