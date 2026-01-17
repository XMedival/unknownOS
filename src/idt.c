#include <mmu.h>
#include <x86.h>
#include <EGA.h>
#include <syscall.h>
#include <proc.h>

// Exception numbers
#define T_DIVIDE     0      // Divide error
#define T_DEBUG      1      // Debug exception
#define T_NMI        2      // Non-maskable interrupt
#define T_BRKPT      3      // Breakpoint
#define T_OFLOW      4      // Overflow
#define T_BOUND      5      // Bound range exceeded
#define T_ILLOP      6      // Invalid opcode
#define T_DEVICE     7      // Device not available
#define T_DBLFLT     8      // Double fault
#define T_COPROC     9      // Coprocessor segment overrun
#define T_TSS       10      // Invalid TSS
#define T_SEGNP     11      // Segment not present
#define T_STACK     12      // Stack-segment fault
#define T_GPFLT     13      // General protection fault
#define T_PGFLT     14      // Page fault
#define T_RES15     15      // Reserved
#define T_FPERR     16      // x87 FPU error
#define T_ALIGN     17      // Alignment check
#define T_MCHK      18      // Machine check
#define T_SIMDERR   19      // SIMD floating-point exception
#define T_VIRTEX    20      // Virtualization exception
#define T_CTLPROT   21      // Control protection exception

#define T_SYSCALL   64      // System call

#define IRQ0        32      // Hardware IRQs start here

// Kernel code selector (SEG_KCODE << 3)
#define KERN_CODE_SEL (SEG_KCODE << 3)

// PIC ports
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

// IDT with 256 entries
struct gatedesc idt[256];

// External vectors from trap.asm
extern void vector0(void);
extern void vector1(void);
extern void vector2(void);
extern void vector3(void);
extern void vector4(void);
extern void vector5(void);
extern void vector6(void);
extern void vector7(void);
extern void vector8(void);
extern void vector9(void);
extern void vector10(void);
extern void vector11(void);
extern void vector12(void);
extern void vector13(void);
extern void vector14(void);
extern void vector15(void);
extern void vector16(void);
extern void vector17(void);
extern void vector18(void);
extern void vector19(void);
extern void vector20(void);
extern void vector21(void);
extern void vector22(void);
extern void vector23(void);
extern void vector24(void);
extern void vector25(void);
extern void vector26(void);
extern void vector27(void);
extern void vector28(void);
extern void vector29(void);
extern void vector30(void);
extern void vector31(void);
extern void vector32(void);
extern void vector33(void);
extern void vector34(void);
extern void vector35(void);
extern void vector36(void);
extern void vector37(void);
extern void vector38(void);
extern void vector39(void);
extern void vector40(void);
extern void vector41(void);
extern void vector42(void);
extern void vector43(void);
extern void vector44(void);
extern void vector45(void);
extern void vector46(void);
extern void vector47(void);
extern void vector64(void);

// Vector table for easy iteration
static void (*vectors[])(void) = {
    vector0,  vector1,  vector2,  vector3,
    vector4,  vector5,  vector6,  vector7,
    vector8,  vector9,  vector10, vector11,
    vector12, vector13, vector14, vector15,
    vector16, vector17, vector18, vector19,
    vector20, vector21, vector22, vector23,
    vector24, vector25, vector26, vector27,
    vector28, vector29, vector30, vector31,
    vector32, vector33, vector34, vector35,
    vector36, vector37, vector38, vector39,
    vector40, vector41, vector42, vector43,
    vector44, vector45, vector46, vector47,
};

// Exception names for debugging
static const char *exception_names[] = {
    "Divide Error",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization Exception",
    "Control Protection",
};

// Remap the PIC to avoid conflicts with CPU exceptions
static void pic_remap(void) {
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DATA, 32);    // IRQ 0-7  -> vectors 32-39
    outb(PIC2_DATA, 40);    // IRQ 8-15 -> vectors 40-47
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);
    outb(PIC1_DATA, 1);
    outb(PIC2_DATA, 1);
    outb(PIC1_DATA, 0xFF);  // Mask all IRQs
    outb(PIC2_DATA, 0xFF);
}

void idt_init(void) {
    pic_remap();

    // Set up exception and IRQ gates (vectors 0-47)
    for (int i = 0; i < 48; i++) {
        SETGATE(idt[i], 0, KERN_CODE_SEL, vectors[i], 0);
    }

    // Set up syscall gate with DPL=3 so user code can invoke it
    SETGATE(idt[T_SYSCALL], 1, KERN_CODE_SEL, vector64, DPL_USER);

    // Load the IDT
    lidt(idt, sizeof(idt));
}

// C trap handler called from trap.asm
void trap(struct trapframe *tf) {
    switch (tf->trapno) {
    case T_PGFLT:
        printf("\n=== PAGE FAULT ===\n");
        printf("Faulting address: 0x%x\n", rcr2());
        printf("Error code: 0x%x\n", tf->err);
        printf("EIP: 0x%x\n", tf->eip);
        printf("CS: 0x%x\n", tf->cs);
        break;

    case T_GPFLT:
        printf("\n=== GENERAL PROTECTION FAULT ===\n");
        printf("Error code: 0x%x\n", tf->err);
        printf("EIP: 0x%x\n", tf->eip);
        printf("CS: 0x%x\n", tf->cs);
        break;

    case T_DBLFLT:
        printf("\n=== DOUBLE FAULT ===\n");
        printf("EIP: 0x%x\n", tf->eip);
        break;

    case T_SYSCALL:
        syscall();
        return;

    default:
        if (tf->trapno < 22) {
            printf("\n=== EXCEPTION: %s (#%d) ===\n",
                   exception_names[tf->trapno], tf->trapno);
        } else if (tf->trapno >= IRQ0 && tf->trapno < IRQ0 + 16) {
            printf("IRQ%d\n", tf->trapno - IRQ0);
            if (tf->trapno >= 40) {
                outb(PIC2_CMD, 0x20);
            }
            outb(PIC1_CMD, 0x20);
            return;
        } else {
            printf("\n=== UNKNOWN TRAP %d ===\n", tf->trapno);
        }
        printf("EIP: 0x%x\n", tf->eip);
        printf("Error: 0x%x\n", tf->err);
        break;
    }

    printf("System halted.\n");
    for (;;) {
        hlt();
    }
}
