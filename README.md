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

Optional test builds:

```bash
# Default known-good worker boot path
python3 build.py all --user-init off

# Direct in-kernel C user task bootstrap
python3 build.py all --user-init c

# Embedded static ELF user task bootstrap
python3 build.py all --user-init elf
```

The default remains `--user-init off`. The direct C user task path is the simpler proof path. The embedded ELF path is now build-selectable and proven end-to-end for one embedded static ELF64 ET_EXEC test program, but it is still a proof-stage loader rather than a general userspace loading system.

## Roadmap

### Phase 1: Kernel Bring-up

Focus: reach stable usermode execution.

Current status: timer-driven preemptive kernel threads and basic `int 0x80` syscalls (`putc`, `yield`, `get_ticks`, `exit`) work under QEMU. A disabled-by-default first user task path exists and works through the direct in-kernel C bootstrap. A proof-stage embedded static ELF64 path also works end-to-end for one test user program. User tasks now carry their own page-table root, while still inheriting a shared kernel mapping base.

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
* [x] Minimal preemptive scheduler
* [x] Kernel thread context switching
* [x] Syscall entry via `int 0x80`
* [x] Shared syscall number definitions
* [x] First scheduled user task bootstrap
* [x] Ring3 syscall return + task done state
* [x] Persistent first user C task loop
* [x] Tiny embedded static ELF64 build scaffolding
* [x] Tiny embedded static ELF64 user loader scaffolding
* [x] Embedded static ELF64 boot path proven end-to-end
* [x] Run first usermode program
* [ ] ELF loader (static binaries, beyond current proof-stage path)

User task bootstrap note: enabling the current first-user-task path builds a task-owned page-table root from a shared kernel mapping base, then maps a small task-owned user virtual layout for trampoline, code, and stack before entering ring 3. The user task is scheduler-owned and timer-preempted. The current ELF path is still a temporary proof-stage loader for one embedded static ELF64 ET_EXEC image with PT_LOAD segments in a fixed physical load window and a bootstrap-provided user virtual layout, and it is not yet a finished userspace loading path. The paging model remains a temporary proof step, not memory isolation.

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

* [ ] `/p/FS` — log-structured, CoW design

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
