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

lseek:

The system call lseek was fairly simple it is located in the sysfile.c file. It takes in two parameters from the user, first the file descriptor and then the new offset value to be set. The
file descriptor input is an integer and that is converted to a struct file. With that we check if the requested offset goes beyond the size. Size called with f->ip->size. In the event that
the offset does not go beyond f->ip->size then the offset value in the file is just set to the requested value. Otherwise the holes in the file will be filled with zeros. This is done within
individual blocks as seen with the while loop in the function. The difference is calculated by subtracting the offset from the size and then incrementally all the blocks are filled with zeros.
Once this is complete the file offset is then set to the requested value. 

------------------
*** PART 2 *****
------------------

symlink:

This system call is created by first adding a new file type T_SYMLINK to stat.h. Within sysfile.c there is the implementation of symlink. It will essentially create a new file of the type
T_SYMLINK that will save the value of the target path in the inode. The inode will store the value with the name target. The sys_open function in the same class had to also be modified so that
when a file of the type T_SYMLINK was opened it would handle this case by openning the target path stored in the inode of type T_SYMLINK. There were a few more things that were requested in
the assignment. The first was that a symlink would go no farther than a certain level. We picked 10. Once we have gone down 10 paths in a recursive search of the original target to a symlink
the function will return an error. This will prevent infinite loops. This was implemented simply with a recursive search. It will first drop all the locks because the search will need them then
search and recur making sure to drop the locks until 10 recursive cycles or a target was found. The next request from the assignment was to add O_NOFOLLOW to fcntl.h. When the flag is set for
O_NOFOLLOW the symlink will not follow. This was a simple boolean statement inside the sys_symlink function. The next request or more so criteria was to not allow symlinks on directories. This
was done simply by adding an if statement in our symlink function. If the target is a directory then release locks and return -1. The final requirement was to allow link and unlink to detect
the symlinks and not follow them. There would be no hard link of a soft link. Likewise this was done by adding a boolean expression inside the link and unlink functions so basically when a
inode of the type T_SYMLINK was detected the function would release locks and return -1. It wasn't too hard because the function was already designed to do with for directories as well.

------------------
*** PART 3 *****
------------------
In this part we need to add support for large files. With the current implementation xv6 files are limited to 140 blocks. An xv6 inode contains 12 direct block numbers and one singly-indirect block number,
which holds up to 128 more blocks so effectively we get 12 + 128 blocks = 140 blocks. If we try to write to a file with more than 140 blocks we will get an error that write has failed. That is because 
we exceeded the maximum. In order to implement large file support, we will 'sacrifice' one of the direct blocks to use for double-indirect block. We will have 11 direct blocks, 1 singly-indirect block and
1 doubly-indirect blocks. Now we effectively get 11 + 128 + 128*128 = 16523 blocks. If we try to write more than 16523 blocks we will get an error that write has failed. Ok, so now let's get more specific
into the implementation. We needed to modify the Macros in fs.h and add a new macro for NDINDIRECT, changed FSSIZE to 200,000 (now it takes a while to compile and build xv6). Since the size of the addrs[]
doesn't change but NDIRECT is now 11 we need to make the size of the array NDIRECT + 2(It was NDIRECT+1). Whenever we write to the file, we do so in blocks (BSIZE = 512), and we pass block number which is 
calculated from off/BSIZE to bmap() and get an address where we will write the data to. The bulk of the implementation is in bmap(). We didn't have to do a lot of changes. We kept what was already there
and simply added the logic to support the doubly-indirect pointer. The doubly-indirect pointer basically contains 128 singly-indirect pointers inside, so once you write that logic, the rest is the same as 
the logic alrady implemented by default for the singly-indirect block. We load the double-indirect block and allocate it if needed. Since we mentioned that the double indirect block contains single indirect blocks
we check whether those are allocated. We use [nb / NINDIRECT] to get the index of the singly-indirect block in the double-indirect block. Since it is a division it will return a real value only when divisible, 
nothing in between. To access the blocks in the single-indirect blocks we use the modulus operation [bn % NINDIRECT] and the result is each block inside the single-indirect block which on itself is inside the 
double-indirect block. It is easier to draw it to understand the way the mapping works. But this implementation allows us support for large files as we now can use 16523 blocks.
If we try to write more than 140 blocks which originally would fail, it will now write to file succesfully. If we exceed the 16523 however it would fail. Of course, we needed to make changes to itrunc() to allow for 
correctly freeing the blocks now that we have a doubly-indirect implementation. The itrunc() code that already exists is good and may still work as is, however to be sure(and be safer) we decided to also free all 
doubly-indirect blocks explicitly. So we basically access each singly-indirect block and iterate inside of it to get all the other indirect blocks (doubly-indirect) blocks and call bfree(). Then we bfree() 
the singly-indirect block and the logic goes on the same way it was before. This will ensure we free all blocks correctly. To make testing of our implementation easier, we implemented the writelarge.c user program 
and you pass as an argument the number of blocks you want to write to it. This is a very neat program and it allows testing the implementation
quite easily. We have added screenshots of using that program and confirm that our implementation is correct and works exactly as expected.  

------------------
*** PART 4 *****
------------------

There are multiple changes that needed to be done in order to implement the extent-based file support. Within fs.h we needed to add the extent struct which has addr and length and their respective sizes
are specified by the problem restrictions. Addr is 3-byte and length is 1-byte. There are 12 extents based on NDIRECT, and each extent can at most contain 2^8 blocks inside (1 byte = 8bits). Additionally,
we needed to support backward compatibility so both pointer-based and extent files are to be supported. We had to add flags, and extent[NDIRECT] inside the dinode. We had to add padding to make the size 
of the dinode divisible with the block size. We used union to determine which struct to use. The way union works is that it reserves the same space in memory for both addrs and extents but will only allocate 
for one of them depending what file type are trying to create. In file.h we need to make the same changes to the inode struct. We needed to make some changes in some systemcalls such as open and create to
allow for support for T_EXTENT files. We added T_EXTENT and O_EXTENT flag which is turned on and off. We added the logic to differentiate between the T_FILE and T_EXTENT. Then we made the main changes in
fs.c. We added changes in writei, readi to support both T_FILE, T_EXTENT files to get the address for the blocks. We simply added conditions to check whether to use bmap() or bmap_extent_dynamic2(). We also
want to be able deallocate extent-based files so made the necessary changes to itrunc to support reseting the extents's addr, length to 0. In itrunc for extent-based files we basically traverse the extents
and check each extent whose length is not 0, and iterate inside the blocks of that extent and use bfree(). In order to read the contents you can use the cat function which uses readi(). We created a helper functions
called bmap_extent_read_mode() which basically traverses the extents and finds the blocks and returns their address. Note there will be some printf statements still active for debugging purposes to make it easier for grading.
The core of the logic for this part is in bmap_extent_dynamic(), where we allow for extents to be created, and extended before checking whether or not to allocate new extents for the blocks. This makes our bmap_extent_dynamic2() very efficient and dynamic. All extents are initially not allocated
and whenever we allocate we set the length to 1. If we need to increase the length (the number of blocks we can hold within one extent), we try and do so for as long as the length of the extent is < 255 (2^8). 
If we reach that limit, and only then, do we allocate a new extent. This way we don't waste useful space and helps with internal fragmentation, and helps with workload of mix of small and large files. We create extents
using our createExtents system call. We need to path a filename to it and it will write to it whatever we passed to the buffer inside the systemcall. We have included multiple print statements to show the logic
flow which might be commented out to prevent clogging the terminal with too many statements. We also needed to modify the stat struct and fstat() system call to ensure we can print information for the extent file and 
pointer-based file. Changes made to stat were adding n_extents, extent_addr, extent_length, and also for pointer-based files add ptr_addr. In order to populate st struct we needed to modify stati() to copy data
from ip(inode) to stat struct. Additionally, stat.c user program was created to allow for using the fstat() system call and print all relevant information about extent-based and pointer-based files.
Screenshots are included to show that the implementation is correct and works exactly as expected. Enjoy playing with it. 


Note: For testing functionality purposes the maximum length of out extents is 3. The actual maximum supported is 255. So if you would like
to test with that simply change the condition check from "newAddr < ext.addr + 3" to "newAddr < ext.addr + 255". 

Part of the assignment was kept seperate from parts 1-3. The folders are labeled xv6-part(1-3) and xv6-part4.

