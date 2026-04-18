Here is a clean rewrite. Same substance, tighter, no fluff, no corporate tone.

---

# Platinum (/p/OS)

64-bit OS built from scratch in C. Runs only on QEMU x86_64 under OVMF. GPLv3.

## Stack

|                     |                                   |
| ------------------- | --------------------------------- |
| Kernel              | C99/C11                           |
| Userspace (planned) | Rust                              |
| Compiler            | Clang/LLVM                        |
| LibC                | musl                              |
| Target              | QEMU x86_64 (`q35`, OVMF)         |
| Drivers             | VirtIO (net, block, console, rng) |

## What it is

A small Unix-shaped kernel built to run in a controlled environment. No real hardware support. No shortcuts through existing kernels.

The goal is simple: build the core pieces cleanly and understand them fully.

## Current state

Phase 1 is done.

The system boots through UEFI, sets up paging, runs a preemptive scheduler, and executes user programs through a syscall interface.

Working parts:

* UEFI boot, ExitBootServices, GDT, IDT, paging
* Physical page allocator and basic kernel heap
* Timer-based preemptive scheduler
* Kernel thread switching
* `int 0x80` syscall interface:
  `putc`, `yield`, `get_ticks`, `exit`, `read`, `write`, `close`, `dup`, `open`, `exec`
* Ring 3 execution for:

  * direct C user task
  * embedded static ELF64 binaries
* Small VFS with in-memory namespace:
  `/dev/console`, `/etc/banner`, `/bin`
* Path-based `open` with basic access checks
* Embedded executable registry under `/bin`:
  `/bin/pulse`, `/bin/echo`
* Per-task file descriptor table with console-backed stdio
* Syscall boundary checks user address ranges and mapped pages before access
* Regression suite covering syscall, exec, fd, namespace, and failure cases

What is not done:

* No general ELF loader
* No fault-contained user memory access
* No disk, filesystem, or networking
* No shell

This is still proof-stage userspace.

## Language split

Kernel stays in C.

* boot, traps, interrupts
* scheduler, paging, allocators
* drivers and low-level state

Userspace will use Rust where it helps:

* tools, services, anything string-heavy

No ideology behind it. Just use the right tool.

## AI policy

AI is fine as a tool.

All code that goes in must be understood and reviewed by a human. No blind generation.

If you cannot explain it, it does not belong here.

## Build and run

Requirements: Clang, lld, Python 3, QEMU, OVMF.

```bash
python3 manage.py doctor
python3 manage.py build
python3 manage.py boot
python3 manage.py verify
python3 manage.py verify --mode elf
python3 manage.py stress --mode yield-stress --loops 25 --timeout 5
python3 manage.py clean
```

Optional builds:

```bash
python3 manage.py build --user-init off
python3 manage.py build --user-init c
python3 manage.py build --user-init elf
python3 manage.py build --user-init elf --user-program pulse
```

Default is `--user-init off`.

The ELF path works for embedded static binaries with normal `PT_LOAD` segments. It is not a general loader.

## Roadmap

### Phase 1: bring-up (done)

Working kernel, scheduler, syscall layer, basic VFS.

### Phase 2: memory and process

Make the user boundary real.

* [ ] General ELF loading
* [ ] User address space cleanup
* [ ] Virtual memory improvements
* [ ] Buddy allocator
* [ ] Slab allocator

### Phase 3: kernel interfaces

* [ ] Syscall cleanup and expansion
* [ ] Better fd handling
* [ ] VFS growth
* [ ] Executable loading beyond embedded images

### Phase 4: storage

* [ ] VirtIO block
* [ ] Minimal filesystem
* [ ] `mkfs`
* [ ] Mount root

### Phase 5: base userspace

* [ ] `pinit`
* [ ] `psup`
* [ ] shell
* [ ] `login` / `getty`
* [ ] core tools (`cat`, `ls`, `cp`, `rm`)

### Phase 6: networking

* [ ] VirtIO net
* [ ] userspace TCP/IP
* [ ] DHCP
* [ ] basic network tools
* [ ] SSH
* [ ] simple HTTP client

### Phase 7: polish

* [ ] logging
* [ ] cron
* [ ] process tools
* [ ] man viewer
* [ ] size reduction
* [ ] documentation cleanup

## License

GPLv3. See `LICENSE`.
