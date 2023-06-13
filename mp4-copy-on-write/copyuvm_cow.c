// ******************************************************************
// * function: copyuvm_cow
// * Description: copyuvm_cow is a modified version of copyuvm that
// *              implements copy-on-write. It is called by fork
// *              when the child process is created.
// *              The only change is that it clears the PTE_W flag
// *              for the child process.
// *              This is the only change we need to make to the
// *              copyuvm function to implement copy-on-write.  
// *              The rest of the code is the same as copyuvm.
// ******************************************************************
pde_t*
copyuvm_cow(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);

    // ******************************************************************
    // * clear flags
    // * Here we are clearing the PTE_W flag to make the page read-only 
    // * for the child process.
    // ******************************************************************
    flags = PTE_FLAGS(*pte) & ~PTE_W;

    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }

  // ******************************************************************
  // * Flush the TLB since the active page table is changing.
  // * This is necessary because the child process will have a different
  // * page table. 
  // ******************************************************************
  flush_tlb_all();
  return d;

bad:
  freevm(d);
  return 0;
}



// ******************************************************************
// * function: deallocuvm
// * Description: a modified version of deallocuvm that
// *              implements copy-on-write. It is called by exit
// *              when the process is terminated. 
// *              The only change is that it only frees the page if
// *              the PTE_W flag is set.
// ******************************************************************
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      
      // ******************************************************************
      // * Only free the page if it is not read-only.
      // * If the page is read-only, then it is a shared page and we
      // * should not free it.
      // * pa is the physical address of the page. 
      // * P2V(pa) is the virtual address of the page.
      // ******************************************************************
      if((*pte & PTE_W) != 0){
         char *v = P2V(pa);
         kfree(v);
         *pte = 0;
      }
    }
  }
  return newsz;
}

// ******************************************************************
// * function: handle_pgflt
// * Description: This function is called when a page fault occurs.
// *              It is called from trap.c. Ikt updates the PTE entry
// *              with write permissions. 
// ******************************************************************
void
handle_pgflt(void) 
{
  // Get the pointer to the current process structure
  struct proc *curproc = myproc();
  uint fa; // fault address
  pte_t *pte; 

  // Get the faulting address from the CR2 register
  fa = read_cr2();

  // Get the physical address of the currently mapped physical frame
  if((pte = walkpgdir(curproc->pgdir, (void *)fault_addrs, 0)) == 0)
    panic("handle_pgflt: pte should exist");

  // Check if the page is present in memory
  if((*pte & PTE_P) == 0)
    panic("handle_pgflt: page not present");

  // Check if the page is already writable
  if(*pte & PTE_W){
    cprintf("PTE has write flag, returning.\n");
    // If the page is already writable, then we don't need to do anything
    return;
  }

  // ******************************************************************
  // * Here we are setting the PTE_W flag to make the page writable 
  // * The "|=" operator is a bitwise OR operator.
  // * In this case, we are setting the PTE_W bit to 1. 
  // ******************************************************************
  *pte |= PTE_W;
}