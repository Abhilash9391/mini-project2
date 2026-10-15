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

- ref counts are in `kalloc.c` using `refcount[NPAGES]` array; used `kmem.lock` so multiple cores don't mess up the counts at the same time.
- used `PTE_COW` bit in `vm.c` to mark shared pages. in `fork`, `uvmcopy` just shares pages and marks them read-only so we don't waste time and ram copying everything until it's actually needed.
- `usertrap` in `trap.c` catches the write fault. if it sees `PTE_COW`, it calls `cowalloc` because the process is trying to change a shared page and needs its own private copy.
- `cowalloc` in `vm.c` uses `kalloc` and `memmove` to make that copy and makes it writable so the write operation can finally succeed.
- called `kfree` on the old page to decrement ref count; this is done so the page is only actually deleted when no one else is using it.
- used `sfence_vma()` in `vm.c` to flush tlb because otherwise the cpu might still think the page is read-only.

### Alarms

- added `alarm_interval`, `alarm_ticks`, `alarm_handler`, `alarm_active`, and `alarm_trapframe` to `struct proc` in `proc.h` to store the timer state and where to jump.
- in `trap.c`, timer interrupt increments `alarm_ticks`. once it hits `alarm_interval`, it saves the current `trapframe` into `p->alarm_trapframe` so we can come back to the exact same spot later.
- set `alarm_active` to 1 before jumping to `alarm_handler` so that another alarm doesn't trigger and interrupt the current handler.
- `sys_sigreturn` in `sysproc.c` copies `p->alarm_trapframe` back to `p->trapframe` using `memmove` to restore all the registers and the pc, and then resets `alarm_active` to 0 to allow new alarms.

### Assumptions

none known

### Known bugs

none known

### Assumptions

none known

### Known bugs

none known




