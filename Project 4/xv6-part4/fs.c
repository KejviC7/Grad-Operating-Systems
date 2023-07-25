// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents (list of other inodes!)
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "fcntl.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
static void itrunc(struct inode*);
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// Zero a block.
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  panic("balloc: out of blocks");
}

// Free a disk block.
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inodes.
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... examine and modify ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
  int i = 0;
  
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&icache.inode[i].lock, "inode");
  }

  readsb(dev, &sb);
  cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n", sb.size, sb.nblocks,
          sb.ninodes, sb.nlog, sb.logstart, sb.inodestart,
          sb.bmapstart);
}

static struct inode* iget(uint dev, uint inum);

//PAGEBREAK!
// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;

      //NEW CODE. INITIALIZE THE EXTENTS. NOT NECESSARY TO DO THAT HERE. KEPT FOR THOUGHT-FLOW DEBUGGING. 
      // If the new inode is an extent-based file, initialize the extents
      
      //cprintf(" Step 0 - 2. Inside ialloc(), initializing extents.\n");
      // // if (dip->type == T_EXTENT) {
      // //   for (int i = 0; i < NDIRECT; i++) {
      // //     dip->extents[i].addr = 0;
      // //     dip->extents[i].length = 255;
      // //   }
      // // }

      // if (dip->type == T_EXTENT) {
      //   // Initialize only the first extent
      //   //uint ex_addr = balloc(dip->div);
      //   dip->extents[0].addr = 1;
      //   dip->extents[0].length = 0;
      // }
      

      log_write(bp);   // mark it allocated on the disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// Unlock the given inode.
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void
iput(struct inode *ip)
{
  acquiresleep(&ip->lock);
  if(ip->valid && ip->nlink == 0){
    acquire(&icache.lock);
    int r = ip->ref;
    release(&icache.lock);
    if(r == 1){
      // inode has no links and no other references: truncate and free.
      itrunc(ip);
      ip->type = 0;
      iupdate(ip);
      ip->valid = 0;
    }
  }
  releasesleep(&ip->lock);

  acquire(&icache.lock);
  ip->ref--;
  release(&icache.lock);
}

// Common idiom: unlock, then put.
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

//PAGEBREAK!
// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
/*
static uint
bmap_extent_mode(struct inode *ip, uint bn)
{
  int i;
  uint cur_blocks = 0;
  uint addr;
  struct extent ext;
  struct buf *bp;

  cprintf(" Step 2 - 2: Entering bmap_extent_mode. \n");
  // Find the extent containing the required block number
  for (i = 0; i < NDIRECT; i++) {
    ext = ip->extents[i];

    if (bn < cur_blocks + ext.length && bn >= cur_blocks) {
      // Calculate the disk sector address
      cprintf(" Step 2 - 3. bn: %d, cur_blocks: %d, ext.length: %d \n", bn, cur_blocks, ext.length);
      cprintf(" Step 2 - 3. bn < cur_blocks + ext.length so returning from bmap_extent_mode. \n");
      return ext.addr + (bn - cur_blocks);
    }
    cprintf(" Step 2 - 4: Incrementing cur_blocks with ext.length. \n");
    cur_blocks += ext.length;
    cprintf(" Step 2 - 5 Loop: Cur Blocks: cur_blocks %d, ext.length %d\n", cur_blocks, ext.length);
  }
  

  // If no extent was found, allocate a new one
  addr = balloc(ip->dev);
  if (addr == 0) {
    panic("bmap_extent_mode: out of disk blocks");
  }

  // Try to extend the last extent if possible
  if (i > 0 && ip->extents[i - 1].addr + ip->extents[i - 1].length == addr) {
    ip->extents[i - 1].length++;
  } else {
    // Otherwise, create a new extent
    if (i >= NDIRECT) {
      panic("bmap_extent_mode: out of extents");
    }

    ip->extents[i].addr = addr;
    ip->extents[i].length = 1;
  }

  //cur_blocks += ip->extents[i].length;
  // Update the inode on disk
  //iupdate(ip);
  cprintf(" Step 2 - 6. Returning addr from bmap_extent_mode. \n");
  return addr;
  
}
*/
static uint
bmap_extent_dynamic2(struct inode *ip, uint bn)
{
  int i = 0; 
  uint curr_length = 0;
  //uint prev_offset;
  uint addr, newAddr; 
  struct extent ext;
  // Add a traverse_flag
  int traverse_flag = 0;

  //cprintf("Inside the bmap_extent_dynamic.\n");
  //cprintf("The requested block number is: %d \n", bn);
  cprintf("\n->Block Number: %d.\n", bn);
  
  while(i < NDIRECT) {
    ext = ip->extents[i];

    // Check if bn is inside the extent
    // Search for the block inside the extent
    if (bn < curr_length + ext.length) {
      // We found the block so return the address
      cprintf("->Found block %d in extent %d.\n", bn, i);
      addr = ext.addr + (bn - curr_length);
      cprintf("->Block's address: %d\n", addr);
      
      return addr;
    } else {
      cprintf("->Couldn't find block %d's address in extent %d.\n", bn, i);
      //cprintf("->Allocating a new block.\n");
      
      // This will allow us to only allocate once for a new block while we traverse the extents
      if (traverse_flag == 0) {
        cprintf("->Allocating a new block.\n");
        newAddr = balloc(ip->dev);
        //Set flag to 1
        cprintf("-> Setting traverse flag to 1.\n");
        //traverse_flag = 1;
      }
      // If the new allocated block is the first in the extent then we need to set the start address of the extent to that address 
      if (ext.length == 0) {
        cprintf("-->First block (block no: %d) in new extent %d.\n", bn, i);
        ip->extents[i].addr = newAddr;
        ip->extents[i].length = 0;
        // Update inode
        iupdate(ip);
        // Update manually once the ext
        ext = ip->extents[i];
      }
      if (traverse_flag == 0) {
        cprintf("-->The address of the new allocated block: %d is: %d\n", bn, newAddr);
      }
      // Now we want to check if the new allocated block is contiguous within the current extent
      if (newAddr < ext.addr + 3) {
        cprintf("->The new allocated block is contiguous within extent %d.\n",i);
        cprintf("->Including it in extent %d and extending the length of extent %d\n", i, i);
        cprintf("->Extent's %d old length was %d.\n", i, ip->extents[i].length);
        ip->extents[i].length++;
        // Update inode
        iupdate(ip);
        cprintf("->Extent's %d new length is %d.\n", i, ip->extents[i].length);
      } else {
        if (traverse_flag == 0) {
          cprintf("->The new allocated block is not contiguous within extent %d.\n",i);
          cprintf("->Allocating a new extent and including the new block %d there.\n",bn);
        }
        // Increment i to go to the next extent
        i++;
        // Update offset for the next extent
        curr_length += ext.length;
      
      }

    }
    // Set traverse flag to 1
    traverse_flag = 1;
  }

  // If we exceed the number of available extents panic
  if (i >= NDIRECT) {
    panic("bmap_extent_mode: out of extents");
  }

  // Update the inode on disk
  iupdate(ip);
  cprintf("REACHED END!\n");
  addr = 0;
  return addr;
}

static uint
bmap_extent_dynamic(struct inode *ip, uint bn)
{
  int i = 0; 
  uint curr_length = 0;
  uint prev_offset;
  uint addr, newAddr; 
  struct extent ext;

  //cprintf("Inside the bmap_extent_dynamic.\n");
  //cprintf("The requested block number is: %d \n", bn);
  cprintf("\n->Block Number: %d.\n", bn);
  while(i < NDIRECT) {
    //cprintf("INSIDE while loop\n");
    // Get extent
    ext = ip->extents[i];

    // Check if it is allocated
    if (ext.length == 0) {
      // If it is not allocated we have two choices: Allocate a new one or extend the previous extent if length less than 255.
      cprintf("->The extent: %d has not been allocated. Deciding whether to allocate it or extend the previous extent if possible...\n", i);

      // Try to extend the last extent if possible first
      if (i > 0 && ip->extents[i - 1].length < 3 ){
        cprintf("->Decided to extend the length of the previous extent.\n");
        cprintf("->Extent's %d old length was %d: .\n", i-1, ip->extents[i-1].length);
        ip->extents[i - 1].length++;
        cprintf("->Extent's %d new length is %d: .\n", i-1, ip->extents[i-1].length);
        // Since we made changes to the inode we need to update it
        iupdate(ip);
        // Update the selected extent in use to the previous one
        ext = ip->extents[i-1];
        cprintf("->Changing offset back to prev_offset.\n");
        curr_length = prev_offset;
        // We need to also decrement i
        i--;
      } else {
        // Allocate new extent
        cprintf("->Couldn't extend the previous extend further. Decided to allocate a new extent!\n");
        newAddr = balloc(ip->dev);
        cprintf("-->The ORIGINAL address of the new allocated extent: %d is: %d\n", i, newAddr);
        // Balloc weird behavior with overlapping addresses. Locks might help, but here for simplicity's sake we will check and assign a different address manually.
        if (i > 0 && newAddr <= ip->extents[i-1].addr + ip->extents[i-i].length) {
          newAddr = ip->extents[i-1].addr + ip->extents[i-i].length + 1;
        }
        cprintf("-->The address of the new allocated extent: %d is: %d\n", i, newAddr);
        // Otherwise, create a new extent
        if (i >= NDIRECT) {
          panic("bmap_extent_mode: out of extents");
        }
        ip->extents[i].addr = newAddr;
        ip->extents[i].length = 1;
        
        //Update
        iupdate(ip);
        
        // For extent 0, we need to update ext to be able to use it. ONLY for Extent 0
        if (i == 0) {
          ext = ip->extents[i];
          cprintf("UPDATING EXTENT %d TO REFLECT NEW EXTENT LENGTH of %d!\n",i, ext.length);
        }
  
      }

    }

    // Search for the block inside the extent
    if (bn < curr_length + ext.length) {
      // We found the block so return the address
      cprintf("->Found block %d in extent %d.\n", bn, i);
      addr = ext.addr + (bn - curr_length);
      cprintf("->Block's address: %d\n", addr);
      return addr;
    } else {
      cprintf("->Couldn't find block %d's address in extent %d.\n", bn, i);
      cprintf("->Trying Again!\n");
    }

    // Increment i++ to go to next extent
    i++;
    // Change offset for next extent address
    prev_offset = curr_length;
    curr_length += ext.length;
    
    // If we exceed the number of available extents panic
    if (i >= NDIRECT) {
          panic("bmap_extent_mode: out of extents");
    }
  }

  // Update the inode on disk
  iupdate(ip);
  cprintf("REACHED END!\n");
  addr = 0;
  return addr;
}

// A version of bmap for extent files that we originally were using. We decided it wasn't very efficient, so we wrote a better one. bmap_extent_dynamic 
// We actually decided to repurpose this function for readi only. It has a nice for loop that allows us to traverse all of the extents and return addresses of the blocks that are present in the extent. 
static uint
bmap_extent_read_mode(struct inode *ip, uint bn)
{
  int i;
  uint cur_length = 0;
  uint addr;
  struct extent ext;

  cprintf("\nThe requested block number is: %d \n", bn);

  // Find the extent containing the required block number
  for (i = 0; i < NDIRECT; i++) {
    ext = ip->extents[i];
  
    if (bn < cur_length + ext.length) {
      // Calculate the disk sector address
      cprintf("Block %d's address returned: %d\n", bn, ext.addr + (bn - cur_length));
      return ext.addr + (bn - cur_length);
    } 
    cur_length += ext.length;
  }
  // Return the disk sector address
  cprintf("Reached end!\n");
  addr = 0;
  return addr;
}


// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  // NEW CODE. FREE BLOCKS FOR EXTENT FILES
  if (ip->type == T_EXTENT) {
    for (i=0; i < NDIRECT; i++) {
      struct extent *ext = &ip->extents[i];
      if (ext->length > 0) {
        for (j=0; j<ext->length;j++) {
          bfree(ip->dev, ext->addr + j);
        }
        // Reset addr, length to 0
        ext->addr = 0;
        ext->length = 0;
      }
    }
  } else {
    // REGULAR LOGIC FOR POINTER-BASED
    for(i = 0; i < NDIRECT; i++){
      if(ip->addrs[i]){
        bfree(ip->dev, ip->addrs[i]);
        ip->addrs[i] = 0;
      }
    }

    if(ip->addrs[NDIRECT]){
      bp = bread(ip->dev, ip->addrs[NDIRECT]);
      a = (uint*)bp->data;
      for(j = 0; j < NINDIRECT; j++){
        if(a[j])
          bfree(ip->dev, a[j]);
      }
      brelse(bp);
      bfree(ip->dev, ip->addrs[NDIRECT]);
      ip->addrs[NDIRECT] = 0;
    }
  }
  ip->size = 0;
  iupdate(ip);
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;

  // We need to collect some additional information for extent files
  if (st->type == T_EXTENT) {
    // Loop over extents
    for (int i = 0; i < NDIRECT; i++) {
      st->extent_addr[i] = ip->extents[i].addr;
      st->extent_len[i] = ip->extents[i].length;
      if (ip->extents[i].length != 0) {
        st->n_extents++;
      }
    }
  } else if (st->type == T_FILE) {
    // Loop over pointers????
    for (int i = 0; i < NDIRECT+1; i++) {
      st->ptr_addrs[i] = ip->addrs[i];
    }
  }



}

//PAGEBREAK!
// Read data from inode.
// Caller must hold ip->lock.
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
      return -1;
    return devsw[ip->major].read(ip, dst, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > ip->size)
    n = ip->size - off;
  //cprintf("ENTERING readi().\n");
  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    // NEW CODE 
    // We need to check the flag to determine whether to use bmap or bmap_extent_mode
    
    if (ip->type == T_EXTENT) {
      bp = bread(ip->dev, bmap_extent_read_mode(ip, off/BSIZE));
    } else {
      bp = bread(ip->dev, bmap(ip, off/BSIZE));
    }
    
    //bp = bread(ip->dev, bmap(ip, off/BSIZE));
    m = min(n - tot, BSIZE - off%BSIZE);
    memmove(dst, bp->data + off%BSIZE, m);
    brelse(bp);
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int
writei(struct inode *ip, char *src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(ip->type == T_DEV){
    if(ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
      return -1;
    return devsw[ip->major].write(ip, src, n);
  }

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    // NEW CODE 
    // We need to check the flag to determine whether to use bmap or bmap_extent_mode
    if (ip->type == T_EXTENT) {
      cprintf(" \n[FOR-LOOP] T_EXTENT file. Calling bmap_extent_dynamic. \n");
      bp = bread(ip->dev, bmap_extent_dynamic2(ip, off/BSIZE));

      //bp = bread(ip->dev, bmap_extent_mode(ip, off/BSIZE));
    } else {
      //cprintf(" Step 2 - 1. Calling regular bmap for pointer-based.\n");
      bp = bread(ip->dev, bmap(ip, off/BSIZE));
    }
    m = min(n - tot, BSIZE - off%BSIZE);
    cprintf("->INFO for next iteration: m: %d, n: %d, off: %d, off/BSIZE: %d, \n", m, n, off, off/BSIZE);
    memmove(bp->data + off%BSIZE, src, m);
    log_write(bp);
    brelse(bp);
  }

  if(n > 0 && off > ip->size){
    ip->size = off;
    iupdate(ip);
  }
  return n;
}

//PAGEBREAK!
// Directories

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // Check that name is not present.
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

//PAGEBREAK!
// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
