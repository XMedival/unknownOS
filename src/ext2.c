#include <ext2.h>
#include <ata.h>
#include <vfs.h>
#include <kalloc.h>
#include <string.h>
#include <log.h>

// Per-mount ext2 filesystem state
struct ext2_fs {
    u8 drive;
    u32 block_size;
    u32 inodes_per_group;
    u32 blocks_per_group;
    u32 inode_size;
    u32 groups_count;
    struct ext2_superblock sb;
    struct ext2_group_desc *groups;     // Block group descriptor table
};

// Private data attached to each VFS node
struct ext2_node_data {
    struct ext2_fs *fs;
    u32 inode_num;
    struct ext2_inode inode;
};

// Forward declarations
static int ext2_read_block(struct ext2_fs *fs, u32 block, void *buf);
static int ext2_read_inode(struct ext2_fs *fs, u32 ino, struct ext2_inode *inode);
static u32 ext2_get_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx);
static struct vfs_node *ext2_create_node(struct ext2_fs *fs, u32 ino, const char *name);

// VFS callbacks
static int ext2_vfs_read(struct vfs_node *node, u64 offset, u64 size, void *buf);
static struct vfs_dirent *ext2_vfs_readdir(struct vfs_node *node, u32 index);
static struct vfs_node *ext2_vfs_finddir(struct vfs_node *node, const char *name);

// Filesystem registration
static struct vfs_filesystem ext2_vfs = {
    .name = "ext2",
    .mount = ext2_mount_fs,
    .unmount = ext2_unmount_fs,
};

// Read a block from disk
static int ext2_read_block(struct ext2_fs *fs, u32 block, void *buf) {
    u32 sectors_per_block = fs->block_size / 512;
    u64 lba = (u64)block * sectors_per_block;

    // Read in 512-byte chunks (ATA PIO reads sectors)
    for (u32 i = 0; i < sectors_per_block; i++) {
        if (ata_read(fs->drive, lba + i, 1, (u8 *)buf + i * 512) < 0) {
            return -1;
        }
    }
    return 0;
}

// Read an inode by number
static int ext2_read_inode(struct ext2_fs *fs, u32 ino, struct ext2_inode *inode) {
    if (ino == 0) return -1;

    // Inode numbers start at 1
    u32 group = (ino - 1) / fs->inodes_per_group;
    u32 index = (ino - 1) % fs->inodes_per_group;

    if (group >= fs->groups_count) return -1;

    // Find the block containing this inode
    u32 inode_table_block = fs->groups[group].bg_inode_table;
    u32 inodes_per_block = fs->block_size / fs->inode_size;
    u32 block_offset = index / inodes_per_block;
    u32 inode_offset = (index % inodes_per_block) * fs->inode_size;

    // Read the block containing the inode
    u8 *block_buf = (u8 *)kalloc();
    if (!block_buf) return -1;

    if (ext2_read_block(fs, inode_table_block + block_offset, block_buf) < 0) {
        kfree((char *)block_buf);
        return -1;
    }

    memcpy(inode, block_buf + inode_offset, sizeof(struct ext2_inode));
    kfree((char *)block_buf);
    return 0;
}

// Get block number for a given block index in a file
// Handles direct, indirect, doubly indirect, and triply indirect blocks
static u32 ext2_get_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx) {
    u32 ptrs_per_block = fs->block_size / 4;
    u32 *indirect_buf = 0;
    u32 block_num = 0;

    if (block_idx < EXT2_NDIR_BLOCKS) {
        // Direct block
        return inode->i_block[block_idx];
    }

    block_idx -= EXT2_NDIR_BLOCKS;

    indirect_buf = (u32 *)kalloc();
    if (!indirect_buf) return 0;

    if (block_idx < ptrs_per_block) {
        // Singly indirect
        if (ext2_read_block(fs, inode->i_block[EXT2_IND_BLOCK], indirect_buf) < 0) {
            kfree((char *)indirect_buf);
            return 0;
        }
        block_num = indirect_buf[block_idx];
        kfree((char *)indirect_buf);
        return block_num;
    }

    block_idx -= ptrs_per_block;

    if (block_idx < ptrs_per_block * ptrs_per_block) {
        // Doubly indirect
        if (ext2_read_block(fs, inode->i_block[EXT2_DIND_BLOCK], indirect_buf) < 0) {
            kfree((char *)indirect_buf);
            return 0;
        }

        u32 dind_idx = block_idx / ptrs_per_block;
        u32 ind_idx = block_idx % ptrs_per_block;

        u32 ind_block = indirect_buf[dind_idx];
        if (ext2_read_block(fs, ind_block, indirect_buf) < 0) {
            kfree((char *)indirect_buf);
            return 0;
        }

        block_num = indirect_buf[ind_idx];
        kfree((char *)indirect_buf);
        return block_num;
    }

    block_idx -= ptrs_per_block * ptrs_per_block;

    // Triply indirect (rare for small files)
    if (ext2_read_block(fs, inode->i_block[EXT2_TIND_BLOCK], indirect_buf) < 0) {
        kfree((char *)indirect_buf);
        return 0;
    }

    u32 tind_idx = block_idx / (ptrs_per_block * ptrs_per_block);
    u32 remainder = block_idx % (ptrs_per_block * ptrs_per_block);
    u32 dind_idx = remainder / ptrs_per_block;
    u32 ind_idx = remainder % ptrs_per_block;

    u32 dind_block = indirect_buf[tind_idx];
    if (ext2_read_block(fs, dind_block, indirect_buf) < 0) {
        kfree((char *)indirect_buf);
        return 0;
    }

    u32 ind_block = indirect_buf[dind_idx];
    if (ext2_read_block(fs, ind_block, indirect_buf) < 0) {
        kfree((char *)indirect_buf);
        return 0;
    }

    block_num = indirect_buf[ind_idx];
    kfree((char *)indirect_buf);
    return block_num;
}

// VFS read callback
static int ext2_vfs_read(struct vfs_node *node, u64 offset, u64 size, void *buf) {
    struct ext2_node_data *data = (struct ext2_node_data *)node->fs_data;
    struct ext2_fs *fs = data->fs;
    struct ext2_inode *inode = &data->inode;

    u32 file_size = inode->i_size;
    if (offset >= file_size) return 0;
    if (offset + size > file_size) size = file_size - offset;

    u8 *block_buf = (u8 *)kalloc();
    if (!block_buf) return -1;

    u8 *dst = (u8 *)buf;
    u64 bytes_read = 0;

    while (bytes_read < size) {
        u32 block_idx = (offset + bytes_read) / fs->block_size;
        u32 block_offset = (offset + bytes_read) % fs->block_size;
        u32 to_read = fs->block_size - block_offset;
        if (to_read > size - bytes_read) to_read = size - bytes_read;

        u32 block_num = ext2_get_block(fs, inode, block_idx);
        if (block_num == 0) {
            // Sparse file - return zeros
            memset(dst + bytes_read, 0, to_read);
        } else {
            if (ext2_read_block(fs, block_num, block_buf) < 0) {
                kfree((char *)block_buf);
                return bytes_read > 0 ? (int)bytes_read : -1;
            }
            memcpy(dst + bytes_read, block_buf + block_offset, to_read);
        }

        bytes_read += to_read;
    }

    kfree((char *)block_buf);
    return (int)bytes_read;
}

// VFS readdir callback
static struct vfs_dirent *ext2_vfs_readdir(struct vfs_node *node, u32 index) {
    static struct vfs_dirent dirent;
    struct ext2_node_data *data = (struct ext2_node_data *)node->fs_data;
    struct ext2_fs *fs = data->fs;
    struct ext2_inode *inode = &data->inode;

    u8 *block_buf = (u8 *)kalloc();
    if (!block_buf) return 0;

    u32 current_index = 0;
    u32 offset = 0;

    while (offset < inode->i_size) {
        u32 block_idx = offset / fs->block_size;
        u32 block_num = ext2_get_block(fs, inode, block_idx);

        if (block_num == 0 || ext2_read_block(fs, block_num, block_buf) < 0) {
            kfree((char *)block_buf);
            return 0;
        }

        u32 block_offset = offset % fs->block_size;
        while (block_offset < fs->block_size && offset < inode->i_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buf + block_offset);

            if (entry->inode != 0) {
                if (current_index == index) {
                    // Found the entry
                    dirent.inode = entry->inode;
                    u32 name_len = entry->name_len;
                    if (name_len >= VFS_MAX_NAME) name_len = VFS_MAX_NAME - 1;
                    memcpy(dirent.name, entry->name, name_len);
                    dirent.name[name_len] = '\0';
                    kfree((char *)block_buf);
                    return &dirent;
                }
                current_index++;
            }

            offset += entry->rec_len;
            block_offset += entry->rec_len;

            if (entry->rec_len == 0) break;  // Prevent infinite loop
        }
    }

    kfree((char *)block_buf);
    return 0;
}

// VFS finddir callback
static struct vfs_node *ext2_vfs_finddir(struct vfs_node *node, const char *name) {
    struct ext2_node_data *data = (struct ext2_node_data *)node->fs_data;
    struct ext2_fs *fs = data->fs;
    struct ext2_inode *inode = &data->inode;

    u8 *block_buf = (u8 *)kalloc();
    if (!block_buf) return 0;

    u32 name_len = strlen(name);
    u32 offset = 0;

    while (offset < inode->i_size) {
        u32 block_idx = offset / fs->block_size;
        u32 block_num = ext2_get_block(fs, inode, block_idx);

        if (block_num == 0 || ext2_read_block(fs, block_num, block_buf) < 0) {
            kfree((char *)block_buf);
            return 0;
        }

        u32 block_offset = offset % fs->block_size;
        while (block_offset < fs->block_size && offset < inode->i_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buf + block_offset);

            if (entry->inode != 0 && entry->name_len == name_len) {
                if (memcmp(entry->name, name, name_len) == 0) {
                    // Found it
                    u32 ino = entry->inode;
                    kfree((char *)block_buf);
                    return ext2_create_node(fs, ino, name);
                }
            }

            offset += entry->rec_len;
            block_offset += entry->rec_len;

            if (entry->rec_len == 0) break;
        }
    }

    kfree((char *)block_buf);
    return 0;
}

// Create a VFS node for an inode
static struct vfs_node *ext2_create_node(struct ext2_fs *fs, u32 ino, const char *name) {
    struct vfs_node *node = (struct vfs_node *)kalloc();
    if (!node) return 0;
    memset(node, 0, sizeof(struct vfs_node));

    struct ext2_node_data *data = (struct ext2_node_data *)kalloc();
    if (!data) {
        kfree((char *)node);
        return 0;
    }
    memset(data, 0, sizeof(struct ext2_node_data));

    data->fs = fs;
    data->inode_num = ino;

    if (ext2_read_inode(fs, ino, &data->inode) < 0) {
        kfree((char *)data);
        kfree((char *)node);
        return 0;
    }

    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->name[VFS_MAX_NAME - 1] = '\0';
    node->inode = ino;
    node->size = data->inode.i_size;
    node->fs_data = data;

    // Set type based on inode mode
    u16 mode = data->inode.i_mode;
    if ((mode & 0xF000) == EXT2_S_IFDIR) {
        node->type = VFS_DIRECTORY;
        node->readdir = ext2_vfs_readdir;
        node->finddir = ext2_vfs_finddir;
    } else if ((mode & 0xF000) == EXT2_S_IFREG) {
        node->type = VFS_FILE;
        node->read = ext2_vfs_read;
    } else if ((mode & 0xF000) == EXT2_S_IFLNK) {
        node->type = VFS_SYMLINK;
        node->read = ext2_vfs_read;
    } else if ((mode & 0xF000) == EXT2_S_IFBLK) {
        node->type = VFS_BLOCKDEV;
    } else if ((mode & 0xF000) == EXT2_S_IFCHR) {
        node->type = VFS_CHARDEV;
    }

    node->permissions = mode & 0x0FFF;

    return node;
}

// Mount an ext2 filesystem (VFS callback)
struct vfs_node *ext2_mount_fs(const char *device, const char *options) {
    (void)options;

    // Parse device name to get drive number
    // Expected format: "ata0", "ata1", etc.
    u8 drive = 0;
    if (device && device[0] == 'a' && device[1] == 't' && device[2] == 'a') {
        drive = device[3] - '0';
    }

    if (!ata_drive_exists(drive)) {
        LOG_FAIL("ext2: drive %d not found", drive);
        return 0;
    }

    struct ext2_fs *fs = (struct ext2_fs *)kalloc();
    if (!fs) return 0;
    memset(fs, 0, sizeof(struct ext2_fs));

    fs->drive = drive;

    // Read superblock (at byte 1024, so LBA 2)
    u8 sb_buf[1024];
    if (ata_read(drive, 2, 2, sb_buf) < 0) {
        LOG_FAIL("ext2: failed to read superblock");
        kfree((char *)fs);
        return 0;
    }

    memcpy(&fs->sb, sb_buf, sizeof(struct ext2_superblock));

    // Verify magic
    if (fs->sb.s_magic != EXT2_MAGIC) {
        LOG_FAIL("ext2: bad magic 0x%x (expected 0xEF53)", fs->sb.s_magic);
        kfree((char *)fs);
        return 0;
    }

    // Calculate filesystem parameters
    fs->block_size = 1024 << fs->sb.s_log_block_size;
    fs->inodes_per_group = fs->sb.s_inodes_per_group;
    fs->blocks_per_group = fs->sb.s_blocks_per_group;
    fs->inode_size = (fs->sb.s_rev_level >= 1) ? fs->sb.s_inode_size : 128;
    fs->groups_count = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;

    LOG_INFO("ext2: block_size=%d, groups=%d, inodes_per_group=%d",
             fs->block_size, fs->groups_count, fs->inodes_per_group);

    // Read block group descriptor table
    // Located in the block after the superblock
    u32 bgdt_block = (fs->block_size == 1024) ? 2 : 1;
    u32 bgdt_size = fs->groups_count * sizeof(struct ext2_group_desc);
    u32 bgdt_blocks = (bgdt_size + fs->block_size - 1) / fs->block_size;

    fs->groups = (struct ext2_group_desc *)kalloc();
    if (!fs->groups) {
        kfree((char *)fs);
        return 0;
    }

    u8 *bgdt_buf = (u8 *)kalloc();
    if (!bgdt_buf) {
        kfree((char *)fs->groups);
        kfree((char *)fs);
        return 0;
    }

    for (u32 i = 0; i < bgdt_blocks; i++) {
        if (ext2_read_block(fs, bgdt_block + i, bgdt_buf) < 0) {
            kfree((char *)bgdt_buf);
            kfree((char *)fs->groups);
            kfree((char *)fs);
            return 0;
        }
        u32 copy_size = fs->block_size;
        if (i * fs->block_size + copy_size > bgdt_size) {
            copy_size = bgdt_size - i * fs->block_size;
        }
        memcpy((u8 *)fs->groups + i * fs->block_size, bgdt_buf, copy_size);
    }
    kfree((char *)bgdt_buf);

    // Create root node (inode 2)
    struct vfs_node *root = ext2_create_node(fs, EXT2_ROOT_INODE, "/");
    if (!root) {
        kfree((char *)fs->groups);
        kfree((char *)fs);
        return 0;
    }

    LOG_OK("ext2: mounted drive %d", drive);
    return root;
}

// Unmount ext2 filesystem
int ext2_unmount_fs(struct vfs_node *root) {
    if (!root || !root->fs_data) return -1;

    struct ext2_node_data *data = (struct ext2_node_data *)root->fs_data;
    struct ext2_fs *fs = data->fs;

    if (fs) {
        if (fs->groups) kfree((char *)fs->groups);
        kfree((char *)fs);
    }

    kfree((char *)data);
    kfree((char *)root);
    return 0;
}

// Mount ext2 on a specific drive and mountpoint
int ext2_mount(u8 drive, const char *mountpoint) {
    char device[8];
    device[0] = 'a';
    device[1] = 't';
    device[2] = 'a';
    device[3] = '0' + drive;
    device[4] = '\0';

    return vfs_mount(device, mountpoint, "ext2");
}

// Initialize ext2 driver
void ext2_init(void) {
    vfs_register_fs(&ext2_vfs);
    LOG_OK("ext2: driver registered");
}
