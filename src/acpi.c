#include <acpi.h>
#include <types.h>
#include <x86.h>
#include <log.h>

static struct acpi_rsdt *rsdt = 0;
static struct acpi_fadt *fadt = 0;
static ushort slp_typa = 0;  // S5 sleep type value

static bool rsdp_checksum_valid(struct acpi_rsdp *rsdp) {
    uchar sum = 0;
    uchar *p = (uchar*)rsdp;
    for (int i = 0; i < 20; i++) {
        sum += p[i];
    }
    return sum == 0;
}

static bool sdt_checksum_valid(struct acpi_sdt_header *header) {
    uchar sum = 0;
    uchar *p = (uchar*)header;
    for (uint i = 0; i < header->Length; i++) {
        sum += p[i];
    }
    return sum == 0;
}

// Scan memory region for RSDP signature on 16-byte boundaries
static void *find_rsdp_in_range(uchar *start, uchar *end) {
    for (uchar *p = start; p < end; p += 16) {
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ') {
            return p;
        }
    }
    return 0;
}

// Find RSDP by scanning EBDA and BIOS ROM area
static struct acpi_rsdp *find_rsdp(void) {
    struct acpi_rsdp *rsdp = 0;

    // 1. Search EBDA (Extended BIOS Data Area)
    ushort ebda_seg = *(ushort*)0x40E;
    if (ebda_seg) {
        uchar *ebda = (uchar*)((uint)ebda_seg << 4);
        rsdp = find_rsdp_in_range(ebda, ebda + 1024);
        if (rsdp && rsdp_checksum_valid(rsdp)) return rsdp;
    }

    // 2. Search BIOS ROM area (0xE0000 - 0xFFFFF)
    rsdp = find_rsdp_in_range((uchar*)0xE0000, (uchar*)0x100000);
    if (rsdp && rsdp_checksum_valid(rsdp)) return rsdp;

    return 0;
}

// Compare 4-byte signature
static bool sig_match(const char *a, const char *b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

void *acpi_find_table(const char *signature) {
    if (!rsdt) return 0;

    uint entries = (rsdt->header.Length - sizeof(struct acpi_sdt_header)) / 4;

    for (uint i = 0; i < entries; i++) {
        struct acpi_sdt_header *header = (struct acpi_sdt_header*)rsdt->Entry[i];
        if (header && sig_match(header->Signature, signature)) {
            if (sdt_checksum_valid(header)) {
                return header;
            }
        }
    }
    return 0;
}

struct acpi_rsdt *acpi_get_rsdt(void) {
    return rsdt;
}

// Parse DSDT to find \_S5 sleep type value
// This is a simplified parser - looks for the _S5_ pattern
static ushort parse_s5_from_dsdt(uchar *dsdt, uint length) {
    // Search for "_S5_" or "_S5 " in the DSDT
    // The S5 object typically looks like: Name(_S5_, Package(){...})
    // In AML bytecode, we look for the pattern
    for (uint i = 0; i < length - 8; i++) {
        if (dsdt[i] == '_' && dsdt[i+1] == 'S' && dsdt[i+2] == '5' &&
            (dsdt[i+3] == '_' || dsdt[i+3] == ' ')) {
            // Found _S5_, now look for the package data
            // Skip ahead to find the sleep type value
            // This is a simplified approach - real AML parsing is complex
            for (uint j = i + 4; j < i + 32 && j < length - 1; j++) {
                // Look for BytePrefix (0x0A) followed by a value
                if (dsdt[j] == 0x0A) {
                    return dsdt[j + 1];
                }
                // Or a simple byte value after package op
                if (dsdt[j] == 0x12) { // PackageOp
                    // Skip package length encoding
                    uint k = j + 1;
                    if (dsdt[k] & 0xC0) k += (dsdt[k] >> 6);
                    k++; // skip length byte(s)
                    k++; // skip element count
                    // First element should be SLP_TYPa
                    if (dsdt[k] == 0x0A) { // BytePrefix
                        return dsdt[k + 1];
                    } else if (dsdt[k] < 0x40) { // Small integer
                        return dsdt[k];
                    }
                }
            }
        }
    }
    // Default for QEMU/Bochs if not found
    return 0;
}

void acpi_init(void) {
    // Find RSDP
    struct acpi_rsdp *rsdp = find_rsdp();
    if (!rsdp) {
        LOG_WARN("ACPI: RSDP not found");
        return;
    }

    // Get OEM ID
    char oemid[7];
    for (int i = 0; i < 6; i++) oemid[i] = rsdp->OEMID[i];
    oemid[6] = '\0';

    // Get RSDT
    rsdt = (struct acpi_rsdt*)rsdp->RsdtAddress;
    if (!rsdt || !sdt_checksum_valid(&rsdt->header)) {
        LOG_WARN("ACPI: invalid RSDT");
        rsdt = 0;
        return;
    }

    LOG_INFO("ACPI: OEMID=%s RSDT=0x%x", oemid, rsdp->RsdtAddress);

    // Find FADT
    fadt = acpi_find_table("FACP");
    if (fadt) {
        LOG_INFO("ACPI: FADT found, PM1a=0x%x", fadt->PM1aControlBlock);

        // Parse DSDT for S5 sleep type
        if (fadt->Dsdt) {
            struct acpi_sdt_header *dsdt = (struct acpi_sdt_header*)fadt->Dsdt;
            if (sdt_checksum_valid(dsdt)) {
                slp_typa = parse_s5_from_dsdt((uchar*)dsdt + sizeof(struct acpi_sdt_header),
                                              dsdt->Length - sizeof(struct acpi_sdt_header));
                LOG_INFO("ACPI: S5 SLP_TYP=%u", slp_typa);
            }
        }
    } else {
        LOG_WARN("ACPI: FADT not found");
    }

    LOG_OK("ACPI initialized");
}

void acpi_shutdown(void) {
    if (!fadt || !fadt->PM1aControlBlock) {
        LOG_FAIL("ACPI: cannot shutdown, no FADT");
        return;
    }

    LOG_INFO("ACPI: shutting down...");

    // Write SLP_TYPa | SLP_EN to PM1a control block
    ushort val = ACPI_SLP_TYP(slp_typa) | ACPI_SLP_EN;
    outw(fadt->PM1aControlBlock, val);

    // If PM1b exists, write to it too
    if (fadt->PM1bControlBlock) {
        outw(fadt->PM1bControlBlock, val);
    }

    // Wait for shutdown to take effect
    for (volatile int i = 0; i < 10000000; i++) {
        asm volatile("pause");
    }

    // If we get here, shutdown failed
    LOG_FAIL("ACPI: shutdown failed");
}
