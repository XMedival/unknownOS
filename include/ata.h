#pragma once
#include <types.h>

// ATA drive information
struct ata_drive_info {
    uint8_t  present;        // Drive exists
    uint8_t  channel;        // 0=primary, 1=secondary
    uint8_t  drive;          // 0=master, 1=slave
    char     model[41];      // Model string (null-terminated)
    uint64_t sectors;        // Total sectors
    uint32_t sector_size;    // Bytes per sector (usually 512)
};

// Initialize ATA driver, detect drives
void ata_init(void);

// Read sectors from a drive
// drive: 0-3 (drives across both channels)
// lba: starting sector
// count: number of sectors (1-255, 0 means 256)
// buf: destination buffer (must be at least count * 512 bytes)
// Returns 0 on success, -1 on error
int ata_read(uint8_t drive, uint64_t lba, uint8_t count, void *buf);

// Write sectors to a drive
int ata_write(uint8_t drive, uint64_t lba, uint8_t count, const void *buf);

// Get drive information (NULL if drive doesn't exist)
struct ata_drive_info *ata_get_drive_info(uint8_t drive);

// Check if drive exists
int ata_drive_exists(uint8_t drive);

// IRQ handler (called from idt.c)
void ata_irq_handler(int channel);
