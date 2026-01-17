#include <types.h>
#include <x86.h>
#include <pci.h>
#include <log.h>

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline uint pciConfReadDword(uchar bus, uchar slot, uchar func, uchar offset) {
    // offset must be dword-aligned
    uint address =
        ((uint)bus  << 16) |
        ((uint)slot << 11) |
        ((uint)func <<  8) |
        ((uint)offset & 0xFC) |
        0x80000000u;

    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static inline ushort pciConfReadWord(uchar bus, uchar slot, uchar func, uchar offset) {
    uint v = pciConfReadDword(bus, slot, func, offset);
    return (ushort)((v >> ((offset & 2) * 8)) & 0xFFFFu);
}

static inline uchar pciConfReadByte(uchar bus, uchar slot, uchar func, uchar offset) {
    uint v = pciConfReadDword(bus, slot, func, offset);
    return (uchar)((v >> ((offset & 3) * 8)) & 0xFFu);
}

// Zwraca true jeśli urządzenie istnieje (vendor != 0xFFFF) i wypełnia header.
// Uwaga: czytamy tylko "common header fields" (pierwsze 16 bajtów config space).
bool pciGetDevice(uchar bus, uchar slot, uchar func, struct pciDeviceHeader *h) {
    if (!h) return false;

    // 0x00: Device/Vendor (DWORD)
    uint id = pciConfReadDword(bus, slot, func, 0x00);
    h->vendor = (ushort)(id & 0xFFFFu);
    if (h->vendor == 0xFFFFu) return false;
    h->device = (ushort)(id >> 16);

    // 0x04: Status/Command (DWORD)
    uint sc = pciConfReadDword(bus, slot, func, 0x04);
    h->command = (ushort)(sc & 0xFFFFu);
    h->status  = (ushort)(sc >> 16);

    // 0x08: Class/Subclass/ProgIF/Revision (DWORD)
    uint cc = pciConfReadDword(bus, slot, func, 0x08);
    h->revision_id = (uchar)(cc & 0xFFu);
    h->prog_if     = (uchar)((cc >> 8) & 0xFFu);
    h->subclass    = (uchar)((cc >> 16) & 0xFFu);
    h->class_code  = (uchar)((cc >> 24) & 0xFFu);

    // 0x0C: BIST/HeaderType/Latency/CacheLine (DWORD)
    uint h3 = pciConfReadDword(bus, slot, func, 0x0C);
    h->cline_size    = (uchar)(h3 & 0xFFu);
    h->latency_timer = (uchar)((h3 >> 8) & 0xFFu);
    h->header_type   = (uchar)((h3 >> 16) & 0xFFu);
    h->bist          = (uchar)((h3 >> 24) & 0xFFu);

    return true;
}

// Parse header type 0x00 (standard device)
bool pciGetHeader0(uchar bus, uchar slot, uchar func, struct pciHeader0 *h) {
    if (!h) return false;

    // First read common header
    if (!pciGetDevice(bus, slot, func, &h->common))
        return false;

    // Check header type (mask out multi-function bit)
    if ((h->common.header_type & PCI_HEADER_TYPE_MASK) != PCI_HEADER_TYPE_0)
        return false;

    // 0x10-0x24: BAR0-BAR5
    for (int i = 0; i < 6; i++) {
        h->bar[i] = pciConfReadDword(bus, slot, func, 0x10 + i * 4);
    }

    // 0x28: CardBus CIS Pointer
    h->cardbus_cis = pciConfReadDword(bus, slot, func, 0x28);

    // 0x2C: Subsystem Vendor/ID
    uint subsys = pciConfReadDword(bus, slot, func, 0x2C);
    h->subsys_vendor = (ushort)(subsys & 0xFFFFu);
    h->subsys_id     = (ushort)(subsys >> 16);

    // 0x30: Expansion ROM Base Address
    h->expansion_rom = pciConfReadDword(bus, slot, func, 0x30);

    // 0x34: Capabilities Pointer (only lower byte used)
    h->capabilities_ptr = pciConfReadByte(bus, slot, func, 0x34);

    // 0x3C: Interrupt Line/Pin, Min Grant, Max Latency
    uint intinfo = pciConfReadDword(bus, slot, func, 0x3C);
    h->interrupt_line = (uchar)(intinfo & 0xFFu);
    h->interrupt_pin  = (uchar)((intinfo >> 8) & 0xFFu);
    h->min_grant      = (uchar)((intinfo >> 16) & 0xFFu);
    h->max_latency    = (uchar)((intinfo >> 24) & 0xFFu);

    return true;
}

static inline bool pciIsMultifunction(const struct pciDeviceHeader *h) {
    return (h->header_type & PCI_MULTIFUNCTION) != 0;
}

static inline uchar pciGetHeaderType(const struct pciDeviceHeader *h) {
    return h->header_type & PCI_HEADER_TYPE_MASK;
}

static void pciLogDevice(uchar bus, uchar slot, uchar func, const struct pciDeviceHeader *d) {
    LOG_INFO("PCI %u:%u.%u %x:%x class %x:%x (type %x)",
             bus, slot, func, d->vendor, d->device,
             d->class_code, d->subclass, pciGetHeaderType(d));
}

static void pciLogHeader0(uchar bus, uchar slot, uchar func, const struct pciHeader0 *h) {
    pciLogDevice(bus, slot, func, &h->common);

    // Log BARs that are non-zero
    for (int i = 0; i < 6; i++) {
        if (h->bar[i] != 0) {
            LOG_INFO("  BAR%d: %x", i, h->bar[i]);
        }
    }

    // Log interrupt info if configured
    if (h->interrupt_pin != 0) {
        LOG_INFO("  IRQ line %u, pin %u", h->interrupt_line, h->interrupt_pin);
    }
}

static void pciScanDevice(uchar bus, uchar slot, uchar func) {
    struct pciDeviceHeader common;
    if (!pciGetDevice(bus, slot, func, &common))
        return;

    uchar htype = pciGetHeaderType(&common);

    switch (htype) {
        case PCI_HEADER_TYPE_0: {
            struct pciHeader0 h0;
            if (pciGetHeader0(bus, slot, func, &h0)) {
                pciLogHeader0(bus, slot, func, &h0);
            }
            break;
        }
        case PCI_HEADER_TYPE_1:
            // PCI-to-PCI bridge - TODO
            pciLogDevice(bus, slot, func, &common);
            LOG_INFO("  (PCI-to-PCI bridge - not parsed)");
            break;
        case PCI_HEADER_TYPE_2:
            // CardBus bridge - TODO
            pciLogDevice(bus, slot, func, &common);
            LOG_INFO("  (CardBus bridge - not parsed)");
            break;
        default:
            pciLogDevice(bus, slot, func, &common);
            LOG_INFO("  (unknown header type)");
            break;
    }
}

void pci_init() {
    for (uint bus = 0; bus < 256; bus++) {
        for (uchar slot = 0; slot < 32; slot++) {

            struct pciDeviceHeader d0;
            if (!pciGetDevice((uchar)bus, slot, 0, &d0))
                continue;

            pciScanDevice((uchar)bus, slot, 0);

            // Jeśli multi-function, skanuj func 1..7
            if (pciIsMultifunction(&d0)) {
                for (uchar func = 1; func < 8; func++) {
                    pciScanDevice((uchar)bus, slot, func);
                }
            }
        }
    }
    LOG_OK("PCI initialized");
}
