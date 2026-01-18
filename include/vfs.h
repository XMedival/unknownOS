#pragma once
#include <types.h>

// File types
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEV     0x03
#define VFS_BLOCKDEV    0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06
#define VFS_MOUNTPOINT  0x08  // Can be OR'd with type

// Open flags
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_APPEND        0x0008
#define O_CREAT         0x0200
#define O_TRUNC         0x0400
#define O_EXCL          0x0800

// Seek modes
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

// Max limits
#define VFS_MAX_PATH    256
#define VFS_MAX_NAME    64
#define VFS_MAX_FD      64
#define VFS_MAX_MOUNTS  8

// Forward declarations
struct vfs_node;
struct vfs_dirent;

// Filesystem operations (implemented by each filesystem)
typedef int (*read_fn)(struct vfs_node *, uint64_t offset, uint64_t size, void *buf);
typedef int (*write_fn)(struct vfs_node *, uint64_t offset, uint64_t size, const void *buf);
typedef int (*open_fn)(struct vfs_node *, uint32_t flags);
typedef int (*close_fn)(struct vfs_node *);
typedef struct vfs_dirent *(*readdir_fn)(struct vfs_node *, uint32_t index);
typedef struct vfs_node *(*finddir_fn)(struct vfs_node *, const char *name);
typedef int (*create_fn)(struct vfs_node *parent, const char *name, uint32_t type);
typedef int (*unlink_fn)(struct vfs_node *parent, const char *name);

// VFS node (represents a file/directory in the filesystem)
struct vfs_node {
    char name[VFS_MAX_NAME];
    uint32_t type;              // VFS_FILE, VFS_DIRECTORY, etc.
    uint32_t permissions;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t inode;             // Filesystem-specific identifier
    uint32_t open_count;        // Reference count

    // Filesystem operations
    read_fn read;
    write_fn write;
    open_fn open;
    close_fn close;
    readdir_fn readdir;
    finddir_fn finddir;
    create_fn create;
    unlink_fn unlink;

    // For mountpoints
    struct vfs_node *ptr;       // Points to root of mounted filesystem
    struct vfs_node *parent;    // Parent directory

    // Filesystem-specific data
    void *fs_data;
};

// Directory entry (returned by readdir)
struct vfs_dirent {
    char name[VFS_MAX_NAME];
    uint64_t inode;
};

// File descriptor
struct vfs_fd {
    struct vfs_node *node;
    uint64_t offset;
    uint32_t flags;
    int in_use;
};

// Filesystem type (for registering filesystems)
struct vfs_filesystem {
    char name[32];
    struct vfs_node *(*mount)(const char *device, const char *options);
    int (*unmount)(struct vfs_node *root);
};

// Mount entry
struct vfs_mount {
    char path[VFS_MAX_PATH];
    struct vfs_node *root;
    struct vfs_filesystem *fs;
    int in_use;
};

// Initialize VFS
void vfs_init(void);

// File operations (use file descriptors)
int vfs_open(const char *path, uint32_t flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, uint64_t size);
int vfs_write(int fd, const void *buf, uint64_t size);
int64_t vfs_seek(int fd, int64_t offset, int whence);
int64_t vfs_tell(int fd);

// Directory operations
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
struct vfs_dirent *vfs_readdir(int fd, uint32_t index);

// Path operations
struct vfs_node *vfs_lookup(const char *path);
int vfs_create(const char *path, uint32_t type);
int vfs_unlink(const char *path);

// Mount operations
int vfs_mount(const char *device, const char *mountpoint, const char *fstype);
int vfs_unmount(const char *mountpoint);

// Filesystem registration
int vfs_register_fs(struct vfs_filesystem *fs);

// Get root node
struct vfs_node *vfs_get_root(void);
