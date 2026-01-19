#pragma once
#include <types.h>
#include <vfs.h>

#define EXT2_MAGIC          0xEF53
#define EXT2_ROOT_INODE     2
#define EXT2_SUPERBLOCK_OFF 1024

// Superblock (at byte 1024, size 1024 bytes)
struct ext2_superblock {
    u32 s_inodes_count;         // Total inodes
    u32 s_blocks_count;         // Total blocks
    u32 s_r_blocks_count;       // Reserved blocks for superuser
    u32 s_free_blocks_count;    // Free blocks
    u32 s_free_inodes_count;    // Free inodes
    u32 s_first_data_block;     // First data block (0 for 1K+ blocks, 1 for 1K blocks)
    u32 s_log_block_size;       // Block size = 1024 << s_log_block_size
    u32 s_log_frag_size;        // Fragment size (usually same as block)
    u32 s_blocks_per_group;     // Blocks per block group
    u32 s_frags_per_group;      // Fragments per block group
    u32 s_inodes_per_group;     // Inodes per block group
    u32 s_mtime;                // Last mount time
    u32 s_wtime;                // Last write time
    u16 s_mnt_count;            // Mounts since last check
    u16 s_max_mnt_count;        // Max mounts before check
    u16 s_magic;                // Magic (0xEF53)
    u16 s_state;                // FS state (1=clean, 2=errors)
    u16 s_errors;               // Error behavior
    u16 s_minor_rev_level;      // Minor revision
    u32 s_lastcheck;            // Last check time
    u32 s_checkinterval;        // Max time between checks
    u32 s_creator_os;           // OS that created fs
    u32 s_rev_level;            // Revision level (0=old, 1=dynamic)
    u16 s_def_resuid;           // Default UID for reserved blocks
    u16 s_def_resgid;           // Default GID for reserved blocks
    // Extended superblock fields (rev >= 1)
    u32 s_first_ino;            // First non-reserved inode
    u16 s_inode_size;           // Inode size (128 for rev 0)
    u16 s_block_group_nr;       // Block group of this superblock
    u32 s_feature_compat;       // Compatible features
    u32 s_feature_incompat;     // Incompatible features
    u32 s_feature_ro_compat;    // Read-only compatible features
    u8  s_uuid[16];             // UUID
    u8  s_volume_name[16];      // Volume name
    u8  s_last_mounted[64];     // Last mount path
    u32 s_algo_bitmap;          // Compression algorithm
    // Performance hints
    u8  s_prealloc_blocks;      // Blocks to preallocate for files
    u8  s_prealloc_dir_blocks;  // Blocks to preallocate for dirs
    u16 s_padding1;
    // Journaling (ext3)
    u8  s_journal_uuid[16];
    u32 s_journal_inum;
    u32 s_journal_dev;
    u32 s_last_orphan;
    u8  s_reserved[788];        // Padding to 1024 bytes
} __attribute__((packed));

// FS state values
#define EXT2_VALID_FS   1       // Clean
#define EXT2_ERROR_FS   2       // Errors detected

// Error handling
#define EXT2_ERRORS_CONTINUE    1
#define EXT2_ERRORS_RO          2
#define EXT2_ERRORS_PANIC       3

// Feature flags
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC    0x0001
#define EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002

// Block Group Descriptor (32 bytes)
struct ext2_group_desc {
    u32 bg_block_bitmap;        // Block bitmap block
    u32 bg_inode_bitmap;        // Inode bitmap block
    u32 bg_inode_table;         // Inode table start block
    u16 bg_free_blocks_count;   // Free blocks in group
    u16 bg_free_inodes_count;   // Free inodes in group
    u16 bg_used_dirs_count;     // Directories in group
    u16 bg_pad;
    u8  bg_reserved[12];
} __attribute__((packed));

// Inode (128 bytes for rev 0, s_inode_size for rev 1+)
struct ext2_inode {
    u16 i_mode;                 // File type and permissions
    u16 i_uid;                  // Owner UID
    u32 i_size;                 // Size in bytes (low 32 bits)
    u32 i_atime;                // Access time
    u32 i_ctime;                // Creation time
    u32 i_mtime;                // Modification time
    u32 i_dtime;                // Deletion time
    u16 i_gid;                  // Group ID
    u16 i_links_count;          // Hard link count
    u32 i_blocks;               // 512-byte blocks count
    u32 i_flags;                // Inode flags
    u32 i_osd1;                 // OS-dependent value 1
    u32 i_block[15];            // Block pointers:
                                //   [0-11]  Direct blocks
                                //   [12]    Singly indirect
                                //   [13]    Doubly indirect
                                //   [14]    Triply indirect
    u32 i_generation;           // File version (for NFS)
    u32 i_file_acl;             // File ACL block
    u32 i_dir_acl;              // Directory ACL / size high (for large files)
    u32 i_faddr;                // Fragment address
    u8  i_osd2[12];             // OS-dependent value 2
} __attribute__((packed));

// Inode type (upper 4 bits of i_mode)
#define EXT2_S_IFSOCK   0xC000  // Socket
#define EXT2_S_IFLNK    0xA000  // Symbolic link
#define EXT2_S_IFREG    0x8000  // Regular file
#define EXT2_S_IFBLK    0x6000  // Block device
#define EXT2_S_IFDIR    0x4000  // Directory
#define EXT2_S_IFCHR    0x2000  // Character device
#define EXT2_S_IFIFO    0x1000  // FIFO

// Inode permissions (lower 12 bits of i_mode)
#define EXT2_S_ISUID    0x0800  // Set UID
#define EXT2_S_ISGID    0x0400  // Set GID
#define EXT2_S_ISVTX    0x0200  // Sticky bit
#define EXT2_S_IRWXU    0x01C0  // User rwx
#define EXT2_S_IRWXG    0x0038  // Group rwx
#define EXT2_S_IRWXO    0x0007  // Others rwx

// Inode flags
#define EXT2_SECRM_FL       0x00000001  // Secure deletion
#define EXT2_UNRM_FL        0x00000002  // Undelete
#define EXT2_COMPR_FL       0x00000004  // Compressed
#define EXT2_SYNC_FL        0x00000008  // Synchronous updates
#define EXT2_IMMUTABLE_FL   0x00000010  // Immutable
#define EXT2_APPEND_FL      0x00000020  // Append only
#define EXT2_NODUMP_FL      0x00000040  // No dump
#define EXT2_NOATIME_FL     0x00000080  // No atime updates

// Directory entry
struct ext2_dir_entry {
    u32 inode;                  // Inode number
    u16 rec_len;                // Directory entry length
    u8  name_len;               // Name length
    u8  file_type;              // File type (if FILETYPE feature)
    char name[];                // File name (variable length)
} __attribute__((packed));

// Directory file types (if FILETYPE feature enabled)
#define EXT2_FT_UNKNOWN     0
#define EXT2_FT_REG_FILE    1
#define EXT2_FT_DIR         2
#define EXT2_FT_CHRDEV      3
#define EXT2_FT_BLKDEV      4
#define EXT2_FT_FIFO        5
#define EXT2_FT_SOCK        6
#define EXT2_FT_SYMLINK     7

// Block pointer indices
#define EXT2_NDIR_BLOCKS    12
#define EXT2_IND_BLOCK      12
#define EXT2_DIND_BLOCK     13
#define EXT2_TIND_BLOCK     14
#define EXT2_N_BLOCKS       15

// API functions
void ext2_init(void);
int ext2_mount(u8 drive, const char *mountpoint);

// Internal functions (for VFS integration)
struct vfs_node *ext2_mount_fs(const char *device, const char *options);
int ext2_unmount_fs(struct vfs_node *root);
