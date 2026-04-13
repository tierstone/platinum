# Platinum (/p/OS)
64-bit OS written in C. Runs exclusively on QEMU x86_64. GPLv3.

## Stack

| | |
|---|---|
| **Lang** | C99/C11 (Strict) |
| **Compiler** | Clang/LLVM |
| **LibC** | musl |
| **License** | GPLv3 |
| **Target** | QEMU x86_64 (`q35`, OVMF) |
| **Drivers** | VirtIO only (Net, Block, Console, RNG) |

## Design Philosophy

/p/OS is a pragmatic hybrid. We take the usability of Linux (GNU tools, ELF binaries), the security mindset of OpenBSD (privilege separation, clean code), and the architectural elegance of FreeBSD (userspace networking, simple init). It’s built on standard Unix principles, stripped of hardware baggage to run purely on QEMU. The goal isn’t to reinvent the wheel, but to build a smaller, cleaner one from scratch.

## AI Policy

AI tools are allowed for reference, exploration, and boilerplate generation.

All code committed to the repository must be:
- understood,
- reviewed,
- and intentionally written or adapted by a human.

Automated or agentic workflows that generate and publish code without human review are not permitted.

The standard is simple: if you cannot explain the code, it does not belong in the tree.

## Build & Run

**Prerequisites:** Clang/LLVM (`clang`, `lld-link`), Python 3, QEMU, OVMF (`edk2-ovmf`).

```bash
# Build the EFI binary
python3 build.py all

# Launch in QEMU
python3 boot.py
```

## Roadmap

### Phase 1: Kernel Bring-up

Focus: reach stable usermode execution.

* [x] UEFI entry — x86_64 COFF entry point (`boot.S`)
* [x] Early serial output — UART `0x3F8` (`serial.c`)
* [x] Build pipeline — Clang + `lld-link`
* [x] QEMU/OVMF launch (`boot.py`)
* [x] ExitBootServices — leave EFI and enter kernel context
* [x] GDT setup
* [x] IDT setup + basic exception handlers
* [x] UEFI memory map parsing
* [x] Paging — 4KB pages, initial identity map
* [x] Physical page allocator — reusable free-list
* [x] Timer interrupt (PIT via QEMU)
* [ ] Minimal preemptive scheduler
* [ ] Syscall entry (syscall/sysret or int)
* [ ] ELF loader (static binaries)
* [ ] Run first usermode program

### Phase 2: Core Kernel

Focus: make the kernel usable, not just alive.

* [ ] Virtual memory refinement (move beyond pure identity map)
* [ ] Kernel heap
* [ ] Buddy allocator
* [ ] Slab allocator
* [ ] File descriptor layer
* [ ] Basic VFS (inodes, dentries)

### Phase 3: Storage

Focus: persistent state.

* [ ] VirtIO-block driver
* [ ] Minimal filesystem (simple, stable baseline)
* [ ] `mkfs` utility
* [ ] Mount root filesystem

*(Optional, later)*

* [ ] `/p/fs` — log-structured, CoW design

### Phase 4: Userspace

Focus: a usable system.

* [ ] `pinit` — PID 1
* [ ] `psup` — service supervisor
* [ ] `sh` — basic shell
* [ ] `login` / `getty`
* [ ] Core utilities (`cat`, `ls`, `cp`, `rm`, etc.)

### Phase 5: Networking

Focus: connectivity.

* [ ] VirtIO-net driver
* [ ] `netd` — userspace TCP/IP stack
* [ ] DHCP client
* [ ] Basic tools (`ping`, `ip/ifconfig`)
* [ ] SSH (Dropbear/OpenSSH, musl-linked)
* [ ] Minimal HTTP client (`curl`/`wget`)

### Phase 6: System Polish

* [ ] `logd` — logging daemon
* [ ] `cron`
* [ ] Process tools (`ps`, `top`, `kill`, `nice`)
* [ ] `man` viewer
* [ ] Size optimization (`-Os`, LTO)
* [ ] Documentation cleanup

## License
GPLv3. See [`LICENSE`](LICENSE).
