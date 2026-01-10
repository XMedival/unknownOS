CC := gcc
CFLAGS := -m32 -ggdb3 -ffreestanding -Iinclude -nostdlib -fno-stack-protector -fno-pie -no-pie -fno-omit-frame-pointer

AS := nasm
ASFLAGS := -f elf32 -g

LD := ld
LDFLAGS := -melf_i386 -T linker.ld

OUTDIR := build
SRCDIR := src

ISO := kernel.iso
KERNEL := kernel.elf

OBJS := $(patsubst $(SRCDIR)/%.c,$(OUTDIR)/%.o,$(wildcard $(SRCDIR)/*.c)) \
        $(patsubst $(SRCDIR)/%.asm,$(OUTDIR)/%.o,$(wildcard $(SRCDIR)/*.asm))

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/%.o: $(SRCDIR)/%.asm | $(OUTDIR)
	@$(AS) $(ASFLAGS) -o $@ $<

$(KERNEL): $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $^

$(ISO): $(KERNEL) grub.cfg
	@mkdir -p iso/boot/grub/
	@cp grub.cfg iso/boot/grub/
	@cp $(KERNEL) iso/boot/
	@grub-mkrescue iso -o $@

run: $(ISO)
	@qemu-system-i386 -m 512 -cdrom $< $(QEMUEXTRA)

clean:
	@rm -rf $(OUTDIR) $(ISO) $(KERNEL)

$(OUTDIR):
	@mkdir -p $(OUTDIR)
