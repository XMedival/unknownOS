CC := gcc

# Display mode: 0=auto, 1=text, 2=framebuffer
FB_MODE ?= 0

# 64-bit kernel flags
CFLAGS := -m64 -ggdb3 -ffreestanding -Iinclude -nostdlib -fno-stack-protector \
          -fno-pie -no-pie -fno-omit-frame-pointer -mno-red-zone \
          -mcmodel=kernel -DFB_FORCE_MODE=$(FB_MODE)

AS := nasm
ASFLAGS := -f elf64 -g

LD := ld
LDFLAGS := -melf_x86_64 -T linker.ld -z max-page-size=0x1000

OUTDIR := build
SRCDIR := src
LIBCDIR := src/libc
USERDIR := user

ISO := kernel.iso
KERNEL := kernel.elf
INIT := init.elf
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

# User program (64-bit)
$(OUTDIR)/user/%.o: $(USERDIR)/%.asm | $(OUTDIR)/user
	@$(AS) $(ASFLAGS) -o $@ $<

$(INIT): $(OUTDIR)/user/init.o
	@$(LD) -melf_x86_64 -T $(USERDIR)/user.ld -o $@ $^

$(OUTDIR)/user:
	@mkdir -p $(OUTDIR)/user

$(ISO): $(KERNEL) $(INIT) grub.cfg
	@mkdir -p iso/boot/grub/
	@cp grub.cfg iso/boot/grub/
	@cp $(KERNEL) iso/boot/
	@cp $(INIT) iso/boot/
	@grub-mkrescue iso -o $@

$(DISK):
	@qemu-img create $@ $(DISKSIZE)

run: $(ISO) $(DISK)
	@qemu-system-x86_64 -m 512 -cdrom $(ISO) -machine acpi=on -device e1000 -hda $(DISK) $(QEMUEXTRA)

clean:
	@rm -rf $(OUTDIR) $(ISO) $(KERNEL) $(INIT)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/libc:
	@mkdir -p $(OUTDIR)/libc
