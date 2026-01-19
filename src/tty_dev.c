#include <tty.h>
#include <vt.h>
#include <vfs.h>
#include <string.h>
#include <kalloc.h>
#include <log.h>

// TTY device node data
struct tty_dev_data {
    int tty_index;      // TTY index (0-7), or -1 for console, -2 for /dev/tty
};

// Forward declarations
static int ttydev_open(struct vfs_node *node, uint32_t flags);
static int ttydev_close(struct vfs_node *node);
static int ttydev_read(struct vfs_node *node, uint64_t off, uint64_t size, void *buf);
static int ttydev_write(struct vfs_node *node, uint64_t off, uint64_t size, const void *buf);

// Resolve TTY index to actual TTY
static struct tty_struct *tty_resolve(int idx) {
    if (idx >= 0 && idx < NR_TTYS) {
        return &ttys[idx];
    }
    if (idx == -1) {
        // /dev/console - use tty0
        return &ttys[0];
    }
    if (idx == -2) {
        // /dev/tty - current controlling terminal
        return tty_current();
    }
    return NULL;
}

// Create a TTY device node
static struct vfs_node *tty_create_devnode(const char *name, int tty_index) {
    struct vfs_node *node = (struct vfs_node *)kalloc();
    if (!node) return NULL;
    memset(node, 0, sizeof(struct vfs_node));

    struct tty_dev_data *data = (struct tty_dev_data *)kalloc();
    if (!data) {
        kfree((char *)node);
        return NULL;
    }

    data->tty_index = tty_index;

    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->name[VFS_MAX_NAME - 1] = '\0';
    node->type = VFS_CHARDEV;
    node->permissions = 0666;
    node->size = 0;
    node->fs_data = data;

    node->open = ttydev_open;
    node->close = ttydev_close;
    node->read = ttydev_read;
    node->write = ttydev_write;

    return node;
}

// VFS callbacks

static int ttydev_open(struct vfs_node *node, uint32_t flags) {
    (void)flags;

    struct tty_dev_data *data = (struct tty_dev_data *)node->fs_data;
    if (!data) return -1;

    struct tty_struct *tty = tty_resolve(data->tty_index);
    if (!tty) return -1;

    return tty_open(tty);
}

static int ttydev_close(struct vfs_node *node) {
    struct tty_dev_data *data = (struct tty_dev_data *)node->fs_data;
    if (!data) return -1;

    struct tty_struct *tty = tty_resolve(data->tty_index);
    if (!tty) return -1;

    return tty_close(tty);
}

static int ttydev_read(struct vfs_node *node, uint64_t off, uint64_t size, void *buf) {
    (void)off;  // TTYs don't support seeking

    struct tty_dev_data *data = (struct tty_dev_data *)node->fs_data;
    if (!data) return -1;

    struct tty_struct *tty = tty_resolve(data->tty_index);
    if (!tty) return -1;

    return tty_read(tty, buf, size);
}

static int ttydev_write(struct vfs_node *node, uint64_t off, uint64_t size, const void *buf) {
    (void)off;

    struct tty_dev_data *data = (struct tty_dev_data *)node->fs_data;
    if (!data) return -1;

    struct tty_struct *tty = tty_resolve(data->tty_index);
    if (!tty) return -1;

    return tty_write(tty, buf, size);
}

// Initialize TTY device nodes in /dev
void tty_dev_init(void) {
    // Find /dev directory
    struct vfs_node *dev = vfs_lookup("/dev");
    if (!dev) {
        LOG_FAIL("tty_dev: /dev not found");
        return;
    }

    // Create /dev/tty0 through /dev/tty7
    for (int i = 0; i < NR_TTYS; i++) {
        char name[8];
        name[0] = 't';
        name[1] = 't';
        name[2] = 'y';
        name[3] = '0' + i;
        name[4] = '\0';

        struct vfs_node *node = tty_create_devnode(name, i);
        if (node) {
            vfs_add_device(dev, node);
        }
    }

    // Create /dev/console (alias to tty0)
    struct vfs_node *console = tty_create_devnode("console", -1);
    if (console) {
        vfs_add_device(dev, console);
    }

    // Create /dev/tty (controlling terminal)
    struct vfs_node *tty = tty_create_devnode("tty", -2);
    if (tty) {
        vfs_add_device(dev, tty);
    }

    LOG_OK("tty_dev: device nodes created in /dev");
}
