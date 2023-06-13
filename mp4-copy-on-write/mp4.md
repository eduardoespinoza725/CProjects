Changes made in xv6:
1. File vm.c 
   1. Function: copyuvm_cow
      1. Lines 347- 386

2. Added functions to trapasm.S
   1. read_cr2
   2. flush_tlb_all

3. Added function declarations in defs.h

4. Added handle_pgflt funciton
   1. File vm.c 
      1. lines 389 - 414

5. Added debugging prints to mappages
   1. line 72