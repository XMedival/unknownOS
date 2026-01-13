CC := gcc
CFLAGS := -m32 -ggdb3 -ffreestanding -Iinclude -nostdlib -fno-stack-protector -fno-pie -no-pie -fno-omit-frame-pointer

AS := nasm
ASFLAGS := -f elf32 -g

LD := ld
LDFLAGS := -melf_i386 -T linker.ld

OUTDIR := build
SRCDIR := src
LIBCDIR := src/libc

ISO := kernel.iso
KERNEL := kernel.elf
DISK := disk.img
DISKSIZE := 2G

OBJS := $(patsubst $(SRCDIR)/%.c,$(OUTDIR)/%.o,$(wildcard $(SRCDIR)/*.c)) \
	$(patsubst $(SRCDIR)/%.asm,$(OUTDIR)/%.o,$(wildcard $(SRCDIR)/*.asm)) \
	$(patsubst $(LIBCDIR)/%.c,$(OUTDIR)/libc/%.o,$(wildcard $(LIBCDIR)/*.c)) \
	$(patsubst $(LIBCDIR)/%.asm,$(OUTDIR)/libc/%.o,$(wildcard $(LIBCDIR)/*.asm))

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/%.o: $(SRCDIR)/%.asm | $(OUTDIR)
	@$(AS) $(ASFLAGS) -o $@ $<

$(OUTDIR)/libc/%.o: $(LIBCDIR)/%.c | $(OUTDIR)/libc
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/libc/%.o: $(LIBCDIR)/%.asm | $(OUTDIR)/libc
	@$(AS) $(ASFLAGS) -o $@ $<

$(KERNEL): $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $^

$(ISO): $(KERNEL) grub.cfg
	@mkdir -p iso/boot/grub/
	@cp grub.cfg iso/boot/grub/
	@cp $(KERNEL) iso/boot/
	@grub-mkrescue iso -o $@

$(DISK):
	@qemu-img create $@ $(DISKSIZE)

run: $(ISO) $(DISK)
	@qemu-system-i386 -m 512 -cdrom $(ISO) -hda $(DISK) $(QEMUEXTRA)

clean:
	@rm -rf $(OUTDIR) $(ISO) $(KERNEL)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/libc:
	@mkdir -p $(OUTDIR)/libc
