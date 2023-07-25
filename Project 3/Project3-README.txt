--------------
GROUP MEMBERS: 
--------------
Kejvi Cupa
Luke Morreale 

Note: Same members as last project. 


------------------
System Enviroment: 
------------------
Kejvi - Windows 10 - WSL (Windows Subsystem for Linux)
CPU: AMD Ryzen Threadripper 2970WX 24-Core

Luke - Ubuntu

------------------
*** PART 1 *****
------------------

* Note part 1 is implemented and can be found in folder xv6_code_PART1.

2. In xv6, the memory is divided into pages (fixed-size units) which are usually 4096 bytes in size.
In xv6, this is specified in param.h, KSTACKSIZE which we can see it is set to 4096. That is the size of per-process kernel stack.
The kernel will manage the allocation and deallocation of pages using a data structure called free list.
The free list tracks the available physical memory pages. Each entry represents the address of a free physical page.
In our context, we call growproc(n), which then calls allocumv() which allocates page tables and physical memory to grow process from oldsz to newsz.
Inside allocumv(), kalloc() is called which allocates a new page by removing the first entry from the free list and returning its address.
kfree() is used to release the physical page when is no longer needed. It clears the contents and adds it to the beginning of the free list.

3. When we re-write sbrk() what we basically do is disable memory allocation. By commenting out growproc(), we are unable to properly allocate new pages, and we increment the process size by n.
However, we didn't increase the memory allocation to account for that, so now the process believes it has access to more memory than it actually does(we didn't allocate more memory). 
This leads to multiple errors such as kernel panic, segmentation fault etc. 

4. sbrk() is very important within xv6 because as we discussed above as well it allows for a way to allocate and manage memnory for a process.
It is able to grow the memory size of the process by n(parameter) bytes and it returns the starting address of the newly allocated region. 
Incrementing process size without actually allocating memory will lead to multiple errors which we already mentioned. That is why the changed sbrk() is broken and the sbrk() (working one)
is vital to the system. 

------------------
*** PART 3 *****
------------------

2.
Line by Line:

	Line 98 will allow a page fault into the boolean statement. The condition of a page fault will be designated by tf->trapno == 14, because T_PGFLT = 14 in traps.h.
	In adition to this the condition that the privalage level of the trapframe struct is set to 3. In x86 architectures, the least significant two bits of the code segment register
	are used to indicate the privilege level of the currently executing code. A value of 3 indicates that the code is running at ring 3, which is the least privileged level in a standard x86 system.
	This expression is used to check if a page fault occurred in the user mode.

	Line 100, char *mem declares a pointer to a character (byte) in memory. It is used later in the code to store the address of a newly allocated page of memory.

	Line 101, uint faulty_addr declares an unsigned integer variable named faulty_addr. It is used to store the virtual address that caused the page fault.
	uint bound declares an unsigned integer variable named bound. It is used to store the virtual address of the start of the page that contains the faulty address. 

	Line 104, The value returned by rcr2() is stored in the faulty_addr variable. This address is then used to calculate the starting virtual address of the page that
	caused the fault by rounding down to the nearest lower multiple of the page size using the PGROUNDDOWN() macro.

	Line 108, PGROUNDDOWN() is a macro that will round down the virtual address to the nearest lower multiple of the page size which is in this case 4KB or 4096 bytes.

	Line 112, This line allocates a page-sized block of physical memory for a process that has caused the page fault. The function kalloc() is a kernel function that returns
	a pointer to a page of memory that is not currently being used by any other process. This line provides a new physical page to the process to store the data that caused the page fault.

	Line 113, Will just set all the bytes of the newly allocated physical memory to zero.

	Line 116, mappages() is a kernel function that maps a range of virtual addresses to a range of physical addresses. It takes five arguments these include the page directory, the starting virtual
	address to be mapped the size of the virtual memory region to be mapped, the physical address to map the virtual memory region to, and a set of flags that determine the protection level of the
	virtual memory region. The purpose of this line is to map the newly allocated physical memory page to the process's virtual address space.

	The rest of the lines are just print statements used to ensure the correct execution of the program.
	
Summary:

	The program operates by executing lazy allocation just before a page fault exception is thrown by the operating system. In the event of such an expected exception the faulting address will be
	caught in line 104. From there the bound will be found. This is specific to the 4KB page sizes that this operating system was designed to handle. Memory is allocated and set to all zeros.
	Then finally the virtual address will be mapped to a physical address. In the event of an error and the user will be alerted with a message. 

//Code for Referance	
Line 98	// We need to respond to PAGE FAULT from user space (tf->cs&3 == 3). In traps.h T_PGFLT = 14
99	else if (tf->trapno == 14 && (tf->cs&3) == 3){
        
100    	char *mem;
    	uint faulty_addr, bound;

    	// Get faulty address from rcr2 register
104    	faulty_addr = rcr2();
105		cprintf("The fault address: %d\n", faulty_addr);

    	// Round down virtual page to get boundary
108    	bound = PGROUNDDOWN(faulty_addr);
109		cprintf("The boundary: %d\n", bound);
        
    	// Allocate new page
112    	mem = kalloc();
113    	memset(mem, 0, PGSIZE);

    	// Mapping 
116    	if(mappages(myproc()->pgdir, (char*)bound, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
117        	cprintf("allocuvm out of memory (2)\n");
118    	}
		cprintf("Physical address: 0x%x\n",V2P(mem));
    	cprintf("allocuvm contains enough memory\n");
    	// Return back to user space to let the process continue executing
122    	return;
	}

3.
Screenshots can be found in the screenshots folder.

The print statements will first show the page faulting address. Assuming the address is correct then the program is executing just before a page fault execption would have been thrown and the
reulting address is correctly displaying the address to which as thrown the faulting error. The next print statement will indicate the boundary which should if run correctly be the closest
lower value address that is modulo 4KB.Then there are print statements to verify that the virtual address was successfully mapped to the physcial address. In the event of an error at this
stage then the program would indicated that there was not enough memory to perform the allocation. The physical address is also displayed which should be the address to which the virtual
address is mapped.


