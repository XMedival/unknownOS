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
LIMINE_DIR := limine

# All outputs go to build/
ISO := $(OUTDIR)/kernel.iso
KERNEL := $(OUTDIR)/kernel.elf
INIT := $(OUTDIR)/init.elf
ISODIR := $(OUTDIR)/iso
DISK := disk.img
DISKSIZE := 2G

$(LIMINE_DIR)/limine:
	make -C $(LIMINE_DIR)

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

$(ISO): $(KERNEL) $(INIT) limine.conf
	@mkdir -p $(ISODIR)/boot $(ISODIR)/EFI/BOOT
	@cp $(KERNEL) $(ISODIR)/boot/
	@cp $(INIT) $(ISODIR)/boot/
	@cp limine.conf $(ISODIR)/
	@cp $(LIMINE_DIR)/limine-bios.sys $(ISODIR)/
	@cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISODIR)/
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISODIR)/
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISODIR)/EFI/BOOT/
	@cp $(LIMINE_DIR)/BOOTIA32.EFI $(ISODIR)/EFI/BOOT/
	@xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISODIR) -o $@
	@$(LIMINE_DIR)/limine bios-install $@

$(DISK):
	@qemu-img create $@ $(DISKSIZE)

run: $(ISO) $(DISK)
	@qemu-system-x86_64 -m 512 -cdrom $(ISO) -machine acpi=on -device e1000 -hda $(DISK) $(QEMUEXTRA)

clean:
	@rm -rf $(OUTDIR)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/libc:
	@mkdir -p $(OUTDIR)/libc
