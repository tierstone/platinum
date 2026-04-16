# Platinum (/p/OS)
64-bit OS written in C. Runs exclusively on QEMU x86_64. GPLv3.

## Stack

| | |
|---|---|
| **Kernel** | C99/C11 (Strict) |
| **Userspace utilities** | Rust |
| **Compiler** | Clang/LLVM, Rust toolchain |
| **LibC** | musl |
| **License** | GPLv3 |
| **Target** | QEMU x86_64 (`q35`, OVMF) |
| **Drivers** | VirtIO only (Net, Block, Console, RNG) |

## Design Philosophy

/p/OS is a pragmatic hybrid. It takes the usability of Linux (GNU tools, ELF binaries), the security mindset of OpenBSD (privilege separation, clean code), and the architectural simplicity of FreeBSD (userspace networking, simple init). Built on standard Unix principles, stripped of hardware baggage to run purely on QEMU. The goal isn't to reinvent the wheel, just build a smaller, cleaner one from scratch.

The kernel stays close to the metal and stays in C. Userspace tools that benefit from safer string handling, parsing, and state management will be written in Rust.

## AI Policy

AI tools are allowed for reference, exploration, and boilerplate generation.

All code committed to the repository must be:
- understood,
- reviewed,
- and intentionally written or adapted by a human.

Automated or agentic workflows that generate and publish code without human review are not permitted.

The rule is simple: if you can't explain the code, it doesn't belong in the tree.

## Build & Run

**Prerequisites:** Clang/LLVM (`clang`, `lld-link`), Python 3, QEMU, OVMF (`edk2-ovmf`).

```bash
# Build the EFI binary
python3 build.py all

# Launch in QEMU
python3 boot.py

# Run the current boot-mode regression check
python3 test_boot_modes.py

# Repeat the boot-mode regression check
python3 test_boot_modes.py --loops 10 --timeout 5

# Hammer one path repeatedly
python3 test_boot_modes.py --mode elf --loops 25 --timeout 5

# Stress the yield-heavy user path
python3 test_boot_modes.py --mode yield-stress --loops 25 --timeout 5
```

Optional test builds:

```bash
# Default known-good worker boot path
python3 build.py all --user-init off

# Direct in-kernel C user task bootstrap
python3 build.py all --user-init c

# Embedded static ELF user task bootstrap
python3 build.py all --user-init elf

# Same ELF path, second embedded user program source
python3 build.py all --user-init elf --user-program pulse
```

The default remains `--user-init off`. The direct C user task path is simpler proof path. Embedded ELF path is build-selectable and proven end-to-end for embedded static ELF64 ET_EXEC test programs. Loader handles normal `PT_LOAD` segment loading for static x86_64 ELF64 inputs, and build tooling can choose between multiple embedded user ELF sources, but whole path is still proof-stage and not a general file-backed userspace loading system.

## Language Plan

The split is straightforward:

### C

C is for the kernel and low-level system code:

* early boot
* traps / IRQs / syscall entry
* scheduler
* paging / address spaces
* allocators
* drivers
* low-level storage bring-up
* other code that sits directly on hardware or CPU state

### Rust

Rust is for userspace programs that benefit from stronger safety around strings, buffers, parsing, and service logic:

* `putils`
* `psup`
* `logd`
* likely parts of networking tools
* possibly more userspace daemons later

This is a practical split, not a religion.

## Roadmap

### Phase 1: Kernel Bring-up

Focus: reach stable scheduled usermode execution.

Current status: timer-driven preemptive kernel threads and basic `int 0x80` syscalls (`putc`, `yield`, `get_ticks`, `exit`) work under QEMU. Small kernel heap is live and exercised during bring-up. Disabled-by-default first user task path exists and works through direct in-kernel C bootstrap. Proof-stage embedded static ELF64 path also works end-to-end for multiple embedded test user programs, handles normal `PT_LOAD` segment loading by ELF virtual address layout, and is still not a general file-backed program loading system.

* [x] UEFI entry (x86_64 COFF entry point, `boot.S`)
* [x] Early serial output (UART `0x3F8`, `serial.c`)
* [x] Build pipeline (Clang + `lld-link`)
* [x] QEMU/OVMF launch (`boot.py`)
* [x] ExitBootServices (leave EFI and enter kernel context)
* [x] GDT setup
* [x] IDT setup + basic exception handlers
* [x] UEFI memory map parsing
* [x] Paging (4KB pages, initial identity map)
* [x] Physical page allocator (reusable free-list)
* [x] Small kernel heap
* [x] Timer interrupt (PIT via QEMU)
* [x] Minimal preemptive scheduler
* [x] Kernel thread context switching
* [x] Syscall entry via `int 0x80`
* [x] Shared syscall number definitions
* [x] First scheduled user task bootstrap
* [x] Ring3 syscall return + task done state
* [x] Persistent first user C task loop
* [x] Run first usermode program

### Phase 2: Memory and Process Foundations

Focus: turn proof-stage userspace execution into a real kernel/userspace boundary.

* [ ] Static ELF loading beyond embedded proof-stage test programs
* [ ] User address-space refinement
* [ ] Virtual memory refinement
* [ ] Buddy allocator
* [ ] Slab allocator

### Phase 3: Kernel Interfaces

Focus: provide the basic kernel abstractions userspace will need.

* [ ] Syscall interface cleanup and expansion
* [ ] File descriptor layer
* [ ] Basic VFS (inodes, dentries)

### Phase 4: Storage

Focus: persistent state.

* [ ] VirtIO-block driver
* [ ] Minimal filesystem (simple, stable baseline)
* [ ] `mkfs` utility
* [ ] Mount root filesystem

*(Optional, later)*

* [ ] `/p/FS` (log-structured, CoW design)

### Phase 5: Base Userspace

Focus: first usable system.

Base userspace will be split by job:

* low-level kernel-facing pieces stay simple
* utility-heavy and service-heavy programs move to Rust where it pays off

- [ ] `pinit` (PID 1, likely C at first)
- [ ] `psup` (service supervisor, Rust)
- [ ] `sh` (basic shell, language undecided)
- [ ] `login` / `getty` (likely C at first)
- [ ] `putils` (Rust core utilities: `cat`, `ls`, `cp`, `rm`, etc.)

### Phase 6: Networking

Focus: connectivity.

* [ ] VirtIO-net driver
* [ ] `netd` (userspace TCP/IP stack, language undecided)
* [ ] DHCP client (likely Rust)
* [ ] Basic tools (`ping`, `ip/ifconfig`)
* [ ] SSH (Dropbear/OpenSSH, musl-linked)
* [ ] Minimal HTTP client (likely Rust)

### Phase 7: System Polish

* [ ] `logd` (logging daemon, Rust)
* [ ] `cron`
* [ ] Process tools (`ps`, `top`, `kill`, `nice`) via `putils` / userspace tools
* [ ] `man` viewer
* [ ] Size optimization (`-Os`, LTO)
* [ ] Documentation cleanup

## License

GPLv3. See [`LICENSE`](LICENSE).
