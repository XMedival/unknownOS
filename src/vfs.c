#include <vfs.h>
#include <string.h>
#include <log.h>
#include <kalloc.h>

// File descriptor table
static struct vfs_fd fd_table[VFS_MAX_FD];

// Mount table
static struct vfs_mount mount_table[VFS_MAX_MOUNTS];

// Registered filesystems
#define VFS_MAX_FS 8
static struct vfs_filesystem *filesystems[VFS_MAX_FS];
static int num_filesystems = 0;

// Root node of the VFS
static struct vfs_node *vfs_root = 0;

// Forward declarations for root filesystem
static int rootfs_read(struct vfs_node *node, uint64_t offset, uint64_t size, void *buf);
static int rootfs_write(struct vfs_node *node, uint64_t offset, uint64_t size, const void *buf);
static struct vfs_dirent *rootfs_readdir(struct vfs_node *node, uint32_t index);
static struct vfs_node *rootfs_finddir(struct vfs_node *node, const char *name);

// Simple in-memory root filesystem
#define ROOTFS_MAX_CHILDREN 32

struct rootfs_dir {
    struct vfs_node *children[ROOTFS_MAX_CHILDREN];
    int num_children;
};

// Allocate a new VFS node
static struct vfs_node *vfs_alloc_node(void) {
    struct vfs_node *node = (struct vfs_node *)kalloc();
    if (node) {
        memset(node, 0, sizeof(struct vfs_node));
    }
    return node;
}

// Create root filesystem node
static struct vfs_node *rootfs_create_node(const char *name, uint32_t type) {
    struct vfs_node *node = vfs_alloc_node();
    if (!node) return 0;

    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->name[VFS_MAX_NAME - 1] = '\0';
    node->type = type;
    node->permissions = 0755;

    if (type == VFS_DIRECTORY) {
        struct rootfs_dir *dir = (struct rootfs_dir *)kalloc();
        if (!dir) {
            kfree((char *)node);
            return 0;
        }
        memset(dir, 0, sizeof(struct rootfs_dir));
        node->fs_data = dir;
        node->readdir = rootfs_readdir;
        node->finddir = rootfs_finddir;
    } else {
        node->read = rootfs_read;
        node->write = rootfs_write;
    }

    return node;
}

// Root filesystem operations
static int rootfs_read(struct vfs_node *node, uint64_t offset, uint64_t size, void *buf) {
    (void)node; (void)offset; (void)size; (void)buf;
    return 0;  // Empty file
}

static int rootfs_write(struct vfs_node *node, uint64_t offset, uint64_t size, const void *buf) {
    (void)node; (void)offset; (void)size; (void)buf;
    return 0;  // Discard writes
}

static struct vfs_dirent *rootfs_readdir(struct vfs_node *node, uint32_t index) {
    static struct vfs_dirent dirent;
    struct rootfs_dir *dir = (struct rootfs_dir *)node->fs_data;

    if (!dir || index >= (uint32_t)dir->num_children) {
        return 0;
    }

    struct vfs_node *child = dir->children[index];
    strncpy(dirent.name, child->name, VFS_MAX_NAME - 1);
    dirent.name[VFS_MAX_NAME - 1] = '\0';
    dirent.inode = child->inode;

    return &dirent;
}

static struct vfs_node *rootfs_finddir(struct vfs_node *node, const char *name) {
    struct rootfs_dir *dir = (struct rootfs_dir *)node->fs_data;

    if (!dir) return 0;

    for (int i = 0; i < dir->num_children; i++) {
        if (strcmp(dir->children[i]->name, name) == 0) {
            return dir->children[i];
        }
    }

    return 0;
}

// Add child to directory
static int rootfs_add_child(struct vfs_node *parent, struct vfs_node *child) {
    struct rootfs_dir *dir = (struct rootfs_dir *)parent->fs_data;

    if (!dir || dir->num_children >= ROOTFS_MAX_CHILDREN) {
        return -1;
    }

    dir->children[dir->num_children++] = child;
    child->parent = parent;
    return 0;
}

void vfs_init(void) {
    // Clear tables
    memset(fd_table, 0, sizeof(fd_table));
    memset(mount_table, 0, sizeof(mount_table));
    memset(filesystems, 0, sizeof(filesystems));

    // Create root directory
    vfs_root = rootfs_create_node("/", VFS_DIRECTORY);
    if (!vfs_root) {
        LOG_FAIL("vfs: failed to create root node");
        return;
    }

    // Create some standard directories
    struct vfs_node *dev = rootfs_create_node("dev", VFS_DIRECTORY);
    struct vfs_node *mnt = rootfs_create_node("mnt", VFS_DIRECTORY);

    if (dev) rootfs_add_child(vfs_root, dev);
    if (mnt) rootfs_add_child(vfs_root, mnt);

    LOG_OK("vfs: initialized");
}

// Find an unused file descriptor
static int vfs_alloc_fd(void) {
    for (int i = 0; i < VFS_MAX_FD; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;
}

// Look up a path and return the node
struct vfs_node *vfs_lookup(const char *path) {
    if (!path || !vfs_root) return 0;

    // Handle root
    if (path[0] == '/' && path[1] == '\0') {
        return vfs_root;
    }

    // Skip leading slash
    if (path[0] == '/') path++;

    struct vfs_node *node = vfs_root;
    char component[VFS_MAX_NAME];
    int i = 0;

    while (*path) {
        // Extract next path component
        i = 0;
        while (*path && *path != '/' && i < VFS_MAX_NAME - 1) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        // Skip trailing slashes
        while (*path == '/') path++;

        // Empty component (e.g., double slash)
        if (i == 0) continue;

        // Check for mountpoint
        if (node->type & VFS_MOUNTPOINT) {
            node = node->ptr;  // Follow to mounted root
        }

        // Look up in directory
        if (!(node->type & VFS_DIRECTORY)) {
            return 0;  // Not a directory
        }

        if (!node->finddir) {
            return 0;  // Directory doesn't support finddir
        }

        struct vfs_node *next = node->finddir(node, component);
        if (!next) {
            return 0;  // Not found
        }

        node = next;
    }

    // Check final mountpoint
    if (node->type & VFS_MOUNTPOINT) {
        node = node->ptr;
    }

    return node;
}

int vfs_open(const char *path, uint32_t flags) {
    struct vfs_node *node = vfs_lookup(path);

    // Handle O_CREAT
    if (!node && (flags & O_CREAT)) {
        if (vfs_create(path, VFS_FILE) < 0) {
            return -1;
        }
        node = vfs_lookup(path);
    }

    if (!node) return -1;

    int fd = vfs_alloc_fd();
    if (fd < 0) return -1;

    fd_table[fd].node = node;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = flags;

    if (node->open) {
        if (node->open(node, flags) < 0) {
            fd_table[fd].in_use = 0;
            return -1;
        }
    }

    node->open_count++;

    // Handle O_TRUNC
    if (flags & O_TRUNC) {
        node->size = 0;
    }

    // Handle O_APPEND
    if (flags & O_APPEND) {
        fd_table[fd].offset = node->size;
    }

    return fd;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return -1;
    }

    struct vfs_node *node = fd_table[fd].node;

    if (node->close) {
        node->close(node);
    }

    if (node->open_count > 0) {
        node->open_count--;
    }

    fd_table[fd].in_use = 0;
    fd_table[fd].node = 0;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = 0;

    return 0;
}

int vfs_read(int fd, void *buf, uint64_t size) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return -1;
    }

    struct vfs_fd *fh = &fd_table[fd];
    struct vfs_node *node = fh->node;

    if (!node->read) {
        return -1;
    }

    int bytes = node->read(node, fh->offset, size, buf);
    if (bytes > 0) {
        fh->offset += bytes;
    }

    return bytes;
}

int vfs_write(int fd, const void *buf, uint64_t size) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return -1;
    }

    struct vfs_fd *fh = &fd_table[fd];
    struct vfs_node *node = fh->node;

    if (!node->write) {
        return -1;
    }

    int bytes = node->write(node, fh->offset, size, buf);
    if (bytes > 0) {
        fh->offset += bytes;
        if (fh->offset > node->size) {
            node->size = fh->offset;
        }
    }

    return bytes;
}

int64_t vfs_seek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return -1;
    }

    struct vfs_fd *fh = &fd_table[fd];
    struct vfs_node *node = fh->node;
    int64_t new_offset;

    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = (int64_t)fh->offset + offset;
        break;
    case SEEK_END:
        new_offset = (int64_t)node->size + offset;
        break;
    default:
        return -1;
    }

    if (new_offset < 0) {
        return -1;
    }

    fh->offset = (uint64_t)new_offset;
    return (int64_t)fh->offset;
}

int64_t vfs_tell(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return -1;
    }
    return (int64_t)fd_table[fd].offset;
}

struct vfs_dirent *vfs_readdir(int fd, uint32_t index) {
    if (fd < 0 || fd >= VFS_MAX_FD || !fd_table[fd].in_use) {
        return 0;
    }

    struct vfs_node *node = fd_table[fd].node;

    if (!(node->type & VFS_DIRECTORY) || !node->readdir) {
        return 0;
    }

    return node->readdir(node, index);
}

int vfs_mkdir(const char *path) {
    return vfs_create(path, VFS_DIRECTORY);
}

int vfs_rmdir(const char *path) {
    return vfs_unlink(path);
}

int vfs_create(const char *path, uint32_t type) {
    if (!path || !vfs_root) return -1;

    // Find parent directory
    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];

    // Extract parent path and name
    int len = strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash < 0) {
        // No slash, parent is root
        strcpy(parent_path, "/");
        strncpy(name, path, VFS_MAX_NAME - 1);
        name[VFS_MAX_NAME - 1] = '\0';
    } else if (last_slash == 0) {
        // Root is parent
        strcpy(parent_path, "/");
        strncpy(name, path + 1, VFS_MAX_NAME - 1);
        name[VFS_MAX_NAME - 1] = '\0';
    } else {
        strncpy(parent_path, path, last_slash);
        parent_path[last_slash] = '\0';
        strncpy(name, path + last_slash + 1, VFS_MAX_NAME - 1);
        name[VFS_MAX_NAME - 1] = '\0';
    }

    struct vfs_node *parent = vfs_lookup(parent_path);
    if (!parent || !(parent->type & VFS_DIRECTORY)) {
        return -1;
    }

    // Check if node already exists
    if (parent->finddir && parent->finddir(parent, name)) {
        return -1;  // Already exists
    }

    // Use filesystem's create if available
    if (parent->create) {
        return parent->create(parent, name, type);
    }

    // For rootfs, create node directly
    struct vfs_node *node = rootfs_create_node(name, type);
    if (!node) return -1;

    return rootfs_add_child(parent, node);
}

int vfs_unlink(const char *path) {
    struct vfs_node *node = vfs_lookup(path);
    if (!node || !node->parent) return -1;

    struct vfs_node *parent = node->parent;

    if (parent->unlink) {
        return parent->unlink(parent, node->name);
    }

    // For rootfs, remove from parent's children
    struct rootfs_dir *dir = (struct rootfs_dir *)parent->fs_data;
    if (!dir) return -1;

    for (int i = 0; i < dir->num_children; i++) {
        if (dir->children[i] == node) {
            // Shift remaining children
            for (int j = i; j < dir->num_children - 1; j++) {
                dir->children[j] = dir->children[j + 1];
            }
            dir->num_children--;

            // Free the node (and its fs_data if directory)
            if (node->type & VFS_DIRECTORY) {
                kfree((char *)node->fs_data);
            }
            kfree((char *)node);
            return 0;
        }
    }

    return -1;
}

int vfs_mount(const char *device, const char *mountpoint, const char *fstype) {
    // Find filesystem
    struct vfs_filesystem *fs = 0;
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(filesystems[i]->name, fstype) == 0) {
            fs = filesystems[i];
            break;
        }
    }

    if (!fs) {
        LOG_FAIL("vfs: unknown filesystem: %s", fstype);
        return -1;
    }

    // Find mountpoint node
    struct vfs_node *mp = vfs_lookup(mountpoint);
    if (!mp || !(mp->type & VFS_DIRECTORY)) {
        LOG_FAIL("vfs: invalid mountpoint: %s", mountpoint);
        return -1;
    }

    // Find empty mount slot
    int slot = -1;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mount_table[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        LOG_FAIL("vfs: mount table full");
        return -1;
    }

    // Mount the filesystem
    struct vfs_node *root = fs->mount(device, 0);
    if (!root) {
        LOG_FAIL("vfs: failed to mount %s on %s", fstype, mountpoint);
        return -1;
    }

    // Set up mount entry
    strncpy(mount_table[slot].path, mountpoint, VFS_MAX_PATH - 1);
    mount_table[slot].root = root;
    mount_table[slot].fs = fs;
    mount_table[slot].in_use = 1;

    // Mark mountpoint
    mp->type |= VFS_MOUNTPOINT;
    mp->ptr = root;

    LOG_OK("vfs: mounted %s on %s", fstype, mountpoint);
    return 0;
}

int vfs_unmount(const char *mountpoint) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mount_table[i].in_use && strcmp(mount_table[i].path, mountpoint) == 0) {
            // Find mountpoint node
            struct vfs_node *mp = vfs_lookup(mountpoint);
            if (mp) {
                mp->type &= ~VFS_MOUNTPOINT;
                mp->ptr = 0;
            }

            // Unmount
            if (mount_table[i].fs->unmount) {
                mount_table[i].fs->unmount(mount_table[i].root);
            }

            mount_table[i].in_use = 0;
            LOG_OK("vfs: unmounted %s", mountpoint);
            return 0;
        }
    }

    return -1;
}

int vfs_register_fs(struct vfs_filesystem *fs) {
    if (num_filesystems >= VFS_MAX_FS) {
        return -1;
    }

    filesystems[num_filesystems++] = fs;
    LOG_INFO("vfs: registered filesystem: %s", fs->name);
    return 0;
}

struct vfs_node *vfs_get_root(void) {
    return vfs_root;
}

int vfs_add_device(struct vfs_node *parent, struct vfs_node *device) {
    if (!parent || !device)
        return -1;

    if (!(parent->type & VFS_DIRECTORY))
        return -1;

    return rootfs_add_child(parent, device);
}
