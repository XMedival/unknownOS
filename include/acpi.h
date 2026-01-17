#pragma once
#include <types.h>

// ACPI SDT Header (common to all tables)
struct acpi_sdt_header {
    char     Signature[4];
    uint     Length;
    uchar    Revision;
    uchar    Checksum;
    char     OEMID[6];
    char     OEMTableID[8];
    uint     OEMRevision;
    uint     CreatorID;
    uint     CreatorRevision;
} __attribute__((packed));

// ACPI 1.0 RSDP (20 bytes)
struct acpi_rsdp {
    char     Signature[8];
    uchar    Checksum;
    char     OEMID[6];
    uchar    Revision;
    uint     RsdtAddress;
} __attribute__((packed));

// ACPI 2.0+ XSDP (36 bytes)
struct acpi_xsdp {
    char     Signature[8];
    uchar    Checksum;
    char     OEMID[6];
    uchar    Revision;
    uint     RsdtAddress;
    uint     Length;
    ulong    XsdtAddress;
    uchar    ExtendedChecksum;
    uchar    Reserved[3];
} __attribute__((packed));

// RSDT (Root System Description Table)
struct acpi_rsdt {
    struct acpi_sdt_header header;
    uint     Entry[];  // Array of 32-bit pointers to other tables
} __attribute__((packed));

// FADT (Fixed ACPI Description Table) - partial, fields we need
struct acpi_fadt {
    struct acpi_sdt_header header;
    uint     FirmwareCtrl;      // 36
    uint     Dsdt;              // 40
    uchar    Reserved;          // 44
    uchar    PreferredPMProfile;// 45
    ushort   SCI_Interrupt;     // 46
    uint     SMI_CommandPort;   // 48
    uchar    AcpiEnable;        // 52
    uchar    AcpiDisable;       // 53
    uchar    S4BIOS_REQ;        // 54
    uchar    PSTATE_CNT;        // 55
    uint     PM1aEventBlock;    // 56
    uint     PM1bEventBlock;    // 60
    uint     PM1aControlBlock;  // 64 - needed for shutdown
    uint     PM1bControlBlock;  // 68
    uint     PM2ControlBlock;   // 72
    uint     PMTimerBlock;      // 76
    uint     GPE0Block;         // 80
    uint     GPE1Block;         // 84
    uchar    PM1EventLength;    // 88
    uchar    PM1ControlLength;  // 89
    uchar    PM2ControlLength;  // 90
    uchar    PMTimerLength;     // 91
    uchar    GPE0Length;        // 92
    uchar    GPE1Length;        // 93
    uchar    GPE1Base;          // 94
    uchar    CStateControl;     // 95
    ushort   WorstC2Latency;    // 96
    ushort   WorstC3Latency;    // 98
    ushort   FlushSize;         // 100
    ushort   FlushStride;       // 102
    uchar    DutyOffset;        // 104
    uchar    DutyWidth;         // 105
    uchar    DayAlarm;          // 106
    uchar    MonthAlarm;        // 107
    uchar    Century;           // 108
    ushort   BootArchFlags;     // 109 (ACPI 2.0+)
    uchar    Reserved2;         // 111
    uint     Flags;             // 112
    // ... more fields for ACPI 2.0+ but we don't need them
} __attribute__((packed));

// PM1 Control Register bits
#define ACPI_SLP_EN    (1 << 13)  // Sleep enable
#define ACPI_SLP_TYP(x) ((x) << 10) // Sleep type (bits 10-12)

// Sleep states
#define ACPI_S5_SOFT_OFF  5

// Initialize ACPI subsystem
void acpi_init(void);

// Shutdown the system (S5 state)
void acpi_shutdown(void);

// Get RSDT pointer (NULL if not found)
struct acpi_rsdt *acpi_get_rsdt(void);

// Find table by signature (e.g., "FACP" for FADT)
void *acpi_find_table(const char *signature);
