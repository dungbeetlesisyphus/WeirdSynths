# ═══════════════════════════════════════════════════════════
# WeirdOS v0.1.0 — Top-level build orchestration
# ═══════════════════════════════════════════════════════════

.PHONY: all build flash test clean help

SCRIPTS := scripts

all: build

build:
	@$(SCRIPTS)/build.sh

flash:
	@echo "Usage: make flash DEVICE=/dev/sdX"
	@echo "Example: make flash DEVICE=/dev/disk4"
ifdef DEVICE
	@$(SCRIPTS)/flash.sh build/output/images/weirdos-v0.1.0-rpi5.img $(DEVICE)
endif

test:
	@$(SCRIPTS)/test-qemu.sh

clean:
	@echo "Cleaning build artifacts..."
	rm -rf build/output
	@echo "Done. Run 'make distclean' to also remove Buildroot sources."

distclean:
	@echo "Removing entire build directory..."
	rm -rf build
	@echo "Done."

help:
	@echo "WeirdOS v0.1.0 Build System"
	@echo ""
	@echo "  make            Build the SD card image (first run: 1-3 hours)"
	@echo "  make flash      Flash image to SD card (set DEVICE=)"
	@echo "  make test       Test boot in QEMU"
	@echo "  make clean      Remove build output (keep Buildroot)"
	@echo "  make distclean  Remove everything (full rebuild needed)"
	@echo ""
	@echo "Requirements:"
	@echo "  - Linux x86_64 build host (Ubuntu 22.04+ recommended)"
	@echo "  - 20GB free disk space"
	@echo "  - build-essential, wget, bc, python3, cpio, unzip, rsync"
