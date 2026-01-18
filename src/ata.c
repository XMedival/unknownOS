#include <ata.h>
#include <x86.h>
#include <string.h>
#include <log.h>

// ATA I/O ports
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

// Register offsets from I/O base
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

// Status register bits
#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DSC          0x10
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

// ATA commands
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_FLUSH       0xE7

// Channel state
struct ata_channel {
    uint16_t io_base;
    uint16_t ctrl_base;
    volatile uint8_t irq_fired;
};

// Global state
static struct ata_channel channels[2];
static struct ata_drive_info drives[4];

// 400ns delay by reading alternate status 4 times
static void ata_delay(struct ata_channel *ch) {
    inb(ch->ctrl_base);
    inb(ch->ctrl_base);
    inb(ch->ctrl_base);
    inb(ch->ctrl_base);
}

// Select drive on channel
static void ata_select_drive(struct ata_channel *ch, uint8_t drive) {
    outb(ch->io_base + ATA_REG_DRIVE, 0xA0 | (drive << 4));
    ata_delay(ch);
}

// Wait for BSY to clear, optionally check DRQ
static int ata_wait(struct ata_channel *ch, int check_drq) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ch->io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) return -1;
            if (status & ATA_SR_DF) return -1;
            if (!check_drq || (status & ATA_SR_DRQ)) {
                return 0;
            }
        }
    }
    return -1;  // Timeout
}

// Wait for IRQ
static int ata_wait_irq(struct ata_channel *ch) {
    for (int i = 0; i < 1000000; i++) {
        if (ch->irq_fired) {
            ch->irq_fired = 0;
            return 0;
        }
    }
    return -1;  // Timeout
}

// Swap byte pairs in string (ATA strings are weird)
static void ata_fix_string(char *s, int len) {
    for (int i = 0; i < len; i += 2) {
        char tmp = s[i];
        s[i] = s[i + 1];
        s[i + 1] = tmp;
    }
    // Trim trailing spaces
    for (int i = len - 1; i >= 0 && s[i] == ' '; i--) {
        s[i] = '\0';
    }
}

// Send IDENTIFY command and parse drive info
static int ata_identify(uint8_t channel, uint8_t drive, struct ata_drive_info *info) {
    struct ata_channel *ch = &channels[channel];
    uint16_t identify_buf[256];

    info->present = 0;
    info->channel = channel;
    info->drive = drive;

    // Select drive
    ata_select_drive(ch, drive);

    // Clear sector count and LBA registers
    outb(ch->io_base + ATA_REG_SECCOUNT, 0);
    outb(ch->io_base + ATA_REG_LBA_LO, 0);
    outb(ch->io_base + ATA_REG_LBA_MID, 0);
    outb(ch->io_base + ATA_REG_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ch->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ch);

    // Check if drive exists
    uint8_t status = inb(ch->io_base + ATA_REG_STATUS);
    if (status == 0) {
        return -1;  // No drive
    }

    // Wait for BSY to clear
    while ((status = inb(ch->io_base + ATA_REG_STATUS)) & ATA_SR_BSY);

    // Check for ATAPI (we only support ATA)
    uint8_t lba_mid = inb(ch->io_base + ATA_REG_LBA_MID);
    uint8_t lba_hi = inb(ch->io_base + ATA_REG_LBA_HI);
    if (lba_mid != 0 || lba_hi != 0) {
        return -1;  // Not ATA (probably ATAPI)
    }

    // Wait for DRQ or ERR
    if (ata_wait(ch, 1) < 0) {
        return -1;
    }

    // Read identify data (256 words)
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(ch->io_base + ATA_REG_DATA);
    }

    // Parse identify data
    info->present = 1;
    info->sector_size = 512;

    // Model string (words 27-46)
    memcpy(info->model, &identify_buf[27], 40);
    info->model[40] = '\0';
    ata_fix_string(info->model, 40);

    // Total sectors (words 60-61 for LBA28)
    info->sectors = identify_buf[60] | ((uint32_t)identify_buf[61] << 16);

    // Check for LBA48 support (word 83 bit 10)
    if (identify_buf[83] & (1 << 10)) {
        // LBA48 sector count (words 100-103)
        info->sectors = identify_buf[100] |
                       ((uint64_t)identify_buf[101] << 16) |
                       ((uint64_t)identify_buf[102] << 32) |
                       ((uint64_t)identify_buf[103] << 48);
    }

    return 0;
}

void ata_init(void) {
    // Initialize channels
    channels[0].io_base = ATA_PRIMARY_IO;
    channels[0].ctrl_base = ATA_PRIMARY_CTRL;
    channels[0].irq_fired = 0;

    channels[1].io_base = ATA_SECONDARY_IO;
    channels[1].ctrl_base = ATA_SECONDARY_CTRL;
    channels[1].irq_fired = 0;

    // Clear drive info
    memset(drives, 0, sizeof(drives));

    // Detect drives
    int found = 0;
    for (int ch = 0; ch < 2; ch++) {
        for (int drv = 0; drv < 2; drv++) {
            int idx = ch * 2 + drv;
            if (ata_identify(ch, drv, &drives[idx]) == 0) {
                uint64_t mb = (drives[idx].sectors * 512) / (1024 * 1024);
                LOG_OK("ata%d: %s (%lu MB)", idx, drives[idx].model, mb);
                found++;
            }
        }
    }

    if (found == 0) {
        LOG_INFO("ata: no drives found");
    }
}

int ata_read(uint8_t drive, uint64_t lba, uint8_t count, void *buf) {
    if (drive >= 4 || !drives[drive].present) {
        return -1;
    }

    struct ata_drive_info *info = &drives[drive];
    struct ata_channel *ch = &channels[info->channel];
    uint16_t *wbuf = (uint16_t *)buf;

    // Select drive with LBA mode
    outb(ch->io_base + ATA_REG_DRIVE, 0xE0 | (info->drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay(ch);

    // Set sector count and LBA
    outb(ch->io_base + ATA_REG_SECCOUNT, count);
    outb(ch->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ch->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ch->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // Send READ command
    ch->irq_fired = 0;
    outb(ch->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    // Read sectors
    uint8_t sectors = count == 0 ? 256 : count;
    for (int i = 0; i < sectors; i++) {
        // Wait for DRQ
        if (ata_wait(ch, 1) < 0) {
            return -1;
        }

        // Read 256 words (512 bytes)
        for (int j = 0; j < 256; j++) {
            *wbuf++ = inw(ch->io_base + ATA_REG_DATA);
        }
    }

    return 0;
}

int ata_write(uint8_t drive, uint64_t lba, uint8_t count, const void *buf) {
    if (drive >= 4 || !drives[drive].present) {
        return -1;
    }

    struct ata_drive_info *info = &drives[drive];
    struct ata_channel *ch = &channels[info->channel];
    const uint16_t *wbuf = (const uint16_t *)buf;

    // Select drive with LBA mode
    outb(ch->io_base + ATA_REG_DRIVE, 0xE0 | (info->drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay(ch);

    // Set sector count and LBA
    outb(ch->io_base + ATA_REG_SECCOUNT, count);
    outb(ch->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ch->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ch->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // Send WRITE command
    ch->irq_fired = 0;
    outb(ch->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    // Write sectors
    uint8_t sectors = count == 0 ? 256 : count;
    for (int i = 0; i < sectors; i++) {
        // Wait for DRQ
        if (ata_wait(ch, 1) < 0) {
            return -1;
        }

        // Write 256 words (512 bytes)
        for (int j = 0; j < 256; j++) {
            outw(ch->io_base + ATA_REG_DATA, *wbuf++);
        }
    }

    // Flush cache
    outb(ch->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    if (ata_wait(ch, 0) < 0) {
        return -1;
    }

    return 0;
}

struct ata_drive_info *ata_get_drive_info(uint8_t drive) {
    if (drive >= 4 || !drives[drive].present) {
        return 0;
    }
    return &drives[drive];
}

int ata_drive_exists(uint8_t drive) {
    return drive < 4 && drives[drive].present;
}

void ata_irq_handler(int channel) {
    if (channel < 2) {
        // Read status to clear interrupt
        inb(channels[channel].io_base + ATA_REG_STATUS);
        channels[channel].irq_fired = 1;
    }
}
