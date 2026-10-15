# Mini Project 2

Mini-project 2 from CS3.301 Operating Systems and Networks, Monsoon 2026

## networking

### Build

```bash
cd networking
make all      # builds tempest/tempest and mastermind/mastermind
make clean
```

### tempest

```bash
tempest <city>
tempest <city> --raw
```

#### Design

- everything is in `getWeather.c`.
- city name is url-encoded using `%XX` for special chars.
- sends `GET /<city>?0T HTTP/1.1` with `Host: wttr.is` and `Connection: close` headers.
- used non-blocking sockets (`O_NONBLOCK`) and `poll()` to handle reads and writes.
- reads response into buffers and looks for `\r\n\r\n` to separate headers from body.
- normal mode just prints body, while `--raw` prints headers too.
- 10s timeout handled by `alarm(10)` and calculating time left using `CLOCK_MONOTONIC` in `poll()`.
- error handling:
    - prints `tempest: invalid location` if `getaddrinfo` fails or status is not 200.
    - prints `tempest: request timed out` if it takes > 10s.
    - prints `missing argument` or `too many arguments` for wrong usage.

#### Changed defaults

none

#### Known bugs

none known

### mastermind

```bash
mastermind [--cost-cutting] [--log]
```

#### Design

<!-- how you split the code into files, and what each file is responsible for -->

#### Discovery

<!-- the 4-byte magic and the broadcast payload layout; the sender's ip comes from the packet source, not the payload -->

#### Message verbs

<!-- every verb, its payload, and when it is sent; must match your .h -->

#### Reliable transfer over UDP (`--cost-cutting`)

<!-- the chunk struct, sequence numbering, how the total chunk count is sent, and the ack/retransmit scheme -->

#### Disconnection handling

<!-- how you detect that the other player is gone -->

- TCP:
- UDP:

#### Changed defaults

<!-- defaults: broadcast every 2 s, entries expire after 5 s, retransmit after 0.1 s; write "none" if unchanged -->

#### Assumptions

<!-- anything you took for granted that the spec did not state -->

#### Known bugs

<!-- what does not work, and when; write "none known" if there are none -->

## xv6

### Build

```bash
cd xv6
make qemu     # boot xv6; Ctrl-A X to quit
make clean
```

### Run

```bash
# in the xv6 shell
cowtest
alarmtest
usertests -q

# or from outside
./test-xv6.py cowtest
./test-xv6.py alarmtest
./test-xv6.py -q usertests
```

### Copy-on-write fork

- ref counts are in `kalloc.c` using `refcount[NPAGES]` array, and `kmem.lock` is used so it doesn't crash.
- in `vm.c`, used `PTE_COW` bit. in `fork`, `uvmcopy` just shares pages between parent and child and marks them read-only.
- `usertrap` in `trap.c` catches the fault when someone tries to write. if `PTE_COW` is set, it calls `cowalloc` in `vm.c`.
- `cowalloc` gets a new page from `kalloc`, copies the old stuff using `memmove`, and makes the pte writable.
- then it calls `kfree` on the old page to drop the count; it only actually frees the page when count is 0.
- called `sfence_vma()` in `vm.c` to clear the tlb.

### Alarms

- added some stuff to `struct proc` in `proc.h` like `alarm_interval`, `alarm_ticks`, `alarm_handler`, `alarm_active`, and `alarm_trapframe`.
- in `trap.c`, timer interrupt increments `alarm_ticks`. once it hits `alarm_interval` and `alarm_active` is 0, it saves the `trapframe` and jumps to `alarm_handler`.
- `sys_sigreturn` in `sysproc.c` is what the handler calls. it just copies `p->alarm_trapframe` back to `p->trapframe` with `memmove` to restore everything.
- `alarm_active` is set to 1 when handler starts and 0 when `sys_sigreturn` is called, so you don't get alarms inside alarms.

### Assumptions

none known

### Known bugs

none known




