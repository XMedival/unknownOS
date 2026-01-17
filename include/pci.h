#pragma once
#include <types.h>

// Common header (first 16 bytes, offsets 0x00-0x0F)
struct pciDeviceHeader {
    ushort vendor, device;
    ushort command, status;
    uchar  revision_id, prog_if, subclass, class_code;
    uchar  cline_size, latency_timer, header_type, bist;
} __attribute__((packed));

// Header type 0x00 - Standard device
struct pciHeader0 {
    struct pciDeviceHeader common;
    uint   bar[6];              // 0x10-0x27: Base Address Registers
    uint   cardbus_cis;         // 0x28: CardBus CIS Pointer
    ushort subsys_vendor;       // 0x2C: Subsystem Vendor ID
    ushort subsys_id;           // 0x2E: Subsystem ID
    uint   expansion_rom;       // 0x30: Expansion ROM Base Address
    uchar  capabilities_ptr;    // 0x34: Capabilities Pointer
    uchar  reserved[7];         // 0x35-0x3B: Reserved
    uchar  interrupt_line;      // 0x3C: Interrupt Line
    uchar  interrupt_pin;       // 0x3D: Interrupt Pin
    uchar  min_grant;           // 0x3E: Min Grant
    uchar  max_latency;         // 0x3F: Max Latency
} __attribute__((packed));

#define PCI_HEADER_TYPE_MASK  0x7F
#define PCI_HEADER_TYPE_0     0x00  // Standard device
#define PCI_HEADER_TYPE_1     0x01  // PCI-to-PCI bridge
#define PCI_HEADER_TYPE_2     0x02  // CardBus bridge
#define PCI_MULTIFUNCTION     0x80  // Multi-function bit

bool pciGetDevice(uchar bus, uchar slot, uchar func, struct pciDeviceHeader *h);
bool pciGetHeader0(uchar bus, uchar slot, uchar func, struct pciHeader0 *h);

void pci_init();
