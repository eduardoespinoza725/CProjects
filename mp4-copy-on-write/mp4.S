# Function to read the CR2 register (page fault address)
.globl read_cr2
read_cr2:
  movl %cr2, %eax
  ret

# Function to read the CR3 register (page directory base)
.globl flush_tlb_all
flush_tlb_all:
  movl %cr3, %eax
  movl %eax, %cr3
  ret