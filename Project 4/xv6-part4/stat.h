#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device
#define T_SYMLINK 4 // Symlink
#define T_EXTENT 5 // Extent 

struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short nlink; // Number of links to file
  uint size;   // Size of file in bytes

  // Adding additional info for extent files
  short n_extents; // Number of extents
  uint extent_addr[12]; // Starting block of each extent
  uint extent_len[12]; // Length of each extent in blocks

  // Additional info for pointer-based files
  uint ptr_addrs[13];
};
