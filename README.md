# libwiieth

Wired Ethernet for Wii homebrew, over a USB LAN adapter, with its own driver and TCP/IP stack.

The Wii's own network stack (IOS) only runs the few USB LAN adapters it knows. Plug in another one and `net_init`
fails (we saw -24) and the adapter's link light never comes on: IOS never powers up its PHY. libwiieth leaves IOS's
network alone and drives the adapter itself:

- **a driver for ASIX AX88772 / AX88772A / AX88772B** USB 2.0 Fast Ethernet chips, talking to the chip through IOS58's
  raw USB interface (`/dev/usb/ven`, libogc's `USB_*` calls), with the reset sequence of the Linux `asix` driver;
- **lwIP 2.2.1** on top: an Ethernet netif, DHCP, a link watcher, TCP and UDP sockets;
- **an lwIP OS layer for libogc** (LWP threads, mutexes, condition variables);
- **netx**, an optional router for libogc's `net_*` calls, so existing network code runs on the cable when an adapter
  is plugged in and on Wi-Fi when it isn't, without changing its calls.

As far as we know it is the only public homebrew USB LAN driver for the Wii. MIT licence (lwIP keeps its own BSD
licence).

## Adapters

| Adapter | Chip | USB id | Status |
|---|---|---|---|
| TRENDnet TU2-ET100 | AX88772B | 0b95:772b | **Tested**: link at 100 Mbit/s full duplex, DHCP, TCP both ways |
| Nintendo RVL-015 (official Wii LAN adapter) | AX88772 | 0b95:7720 | Expected to work (same family; IOS runs it anyway) |
| Other AX88772 / AX88772A / AX88772B adapters with ASIX's own USB ids | AX88772x | 0b95:7720, 0b95:772a, 0b95:772b, 0b95:7e2b | Expected to work, untested |

Not supported: AX88772-based adapters sold under another vendor id (the driver only matches VID 0b95; adding an id
is a one-line change in `source/asix.c` - please report what works), other ASIX chips (AX88178, AX88179 gigabit:
different protocol), Realtek RTL8152/8153 and anything else. Only one adapter is used.

## Requirements

- **IOS58** (or another IOS with the new USB modules): the driver needs `/dev/usb/ven`. The Homebrew Channel starts
  apps on IOS58 unless their meta.xml says `<no_ios_reload/>`.
- **AHBPROT**: everything was tested in an app started with AHBPROT disabled (`<ahb_access/>` in meta.xml, as the
  examples do). The driver itself only makes IOS calls, so it may well work without it, but that is untested.
- devkitPPC + libogc (tested with the current `devkitpro/devkitppc` Docker image, libogc 3.1.0).
- Memory: lwIP is configured for throughput (`MEM_SIZE` 512 KiB plus pools, see `source/port/lwipopts.h`), and the
  driver takes a small IPC heap in MEM2 for its USB buffers.

## Building

With devkitPro installed:

    make              # lib/libwiieth.a
    make examples     # and examples/hello/wiieth-hello.dol, examples/netx/wiieth-netx.dol
    make install      # optional: lib + headers into $DEVKITPRO/portlibs/wii (or DESTDIR=...)

Without it, in Docker (Docker Desktop, Colima, ...):

    ./build.sh                       # = make all examples, in devkitpro/devkitppc
    ./build.sh clean all examples
    BUILD_HOST=user@host ./build.sh  # on another machine with Docker; results are copied back

Link your app with `-lwiieth` before libogc: `-L<libwiieth>/lib -lwiieth -lwiiuse -lbte -logc -lm`, and add
`-I<libwiieth>/include`.

## Using it

### Directly

```c
#include <network.h>          // only if you want libogc's sockaddr_in etc.; wiieth.h works with or without it
#include <wiieth.h>

static void log_line(const char *line) { printf("%s\n", line); }

wiieth_set_log(log_line);                  // optional: driver + lwIP diagnostics
wiieth_set_hostname("my-wii");             // optional: DHCP hostname (default "wii")
if (wiieth_present()) {
    char msg[700];
    if (wiieth_start(msg, sizeof msg) == 0) {          // adapter reset, lwIP, DHCP in the background
        unsigned int ip;
        while (!(ip = wiieth_ip())) usleep(100 * 1000); // usually a few seconds
        int s = wiieth_socket(AF_INET, SOCK_STREAM, 0); // then BSD sockets as usual
        ...
    }
}
```

Socket calls follow libogc's `net_*` convention: a result >= 0, or a negative errno. Blocking by default;
`wiieth_set_nonblock`, `wiieth_set_timeout` (receive/send timeouts) are there for the rest. Sockets are thread-safe
in the usual lwIP way (one socket per thread, or your own locking). `wiieth_status()` gives a one-line summary (address,
gateway, DHCP state, link, PHY registers, frame and error counters) that is the first thing to look at when something
doesn't work.

### Through netx (existing net_* code, Wi-Fi fallback)

```c
#include <network.h>
#include <wiieth_netx.h>      // after <network.h>, in every file that makes net_* calls

if (netx_init(20000) == 0)    // cable if an adapter gets an address within 20 s, else IOS's net_init (Wi-Fi)
    printf("on %s\n", netx_wired ? "the cable" : "Wi-Fi");
s32 s = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);   // unchanged code: goes to the network in use
```

netx routes `net_socket bind listen accept connect send sendto recv recvfrom close fcntl gethostip`. New sockets go
to the network in use (`netx_wired`, which you can also set yourself); every later call on a socket goes to the stack
that owns its fd (wiieth's fds start at 1000, IOS's are small), so both networks can be used side by side. For
`net_fcntl`, the non-blocking flag is IOS's 0x04 (`NETX_O_NONBLOCK`). Everything else (`net_setsockopt`, `net_select`,
`net_poll`, `net_gethostbyname`, ...) still goes to IOS only.

### Low level

`wiieth_asix.h` is the bare driver: open, link state, send a frame, a receive callback per frame, MII and raw vendor
requests for debugging. Use it to bring your own stack.

## Examples

- `examples/hello`: DHCP on the adapter, prints the address, serves a status page on TCP 80 and an echo service on
  TCP 7 (`curl http://<wii>/`, `nc <wii> 7`).
- `examples/netx`: plain `net_*` code serving one page on TCP 80, on the cable or on Wi-Fi.

Copy `wiieth-<name>.dol` to `sd:/apps/wiieth-<name>/boot.dol` with its `meta.xml`, or send it with wiiload. HOME (Wii
Remote) or START (GameCube pad) exits.

## Performance

Measured on an original Wii (RVL-001) with the TRENDnet TU2-ET100 on a 100 Mbit/s switch port:

- link up and DHCP address about 7 s after the app starts;
- about **0.7 MB/s** in both directions, measured as file transfers to and from the SD card - so most likely limited
  by the SD card and the app, not by the driver. Raw socket throughput hasn't been measured separately.

## Known limits

- IPv4 only; no DNS on the wired stack (`LWIP_DNS` is off: use addresses, or resolve over Wi-Fi), no multicast
  receive (the multicast filter is left empty), no IGMP.
- No `select`/`poll` wrapper: use blocking sockets in threads, non-blocking sockets, or timeouts.
- No shutdown: once started, the driver and lwIP run until the app exits (an IOS reload, as on every exit to the
  loader, resets the USB device). Unplugging and replugging the adapter while running isn't handled.
- Link state is polled every 500 ms.
- lwIP's symbols (`tcp_*`, `pbuf_*`, `netif_*`, `sys_*`, ...) are global: don't link a second copy of lwIP.
- One adapter, AX88772 family, VID 0b95 only (see above).

## Notes for driver authors

Things that cost us days, in case you write a driver for another chip on `/dev/usb/ven`:

- **USB buffers must live in an IPC heap in MEM2** (`iosCreateHeap` + `iosAlloc`), 32-byte aligned. Control-transfer
  replies into a MEM1 buffer never arrived: every read returned what the buffer held before.
- **Flush and invalidate the data cache yourself**, around control transfers too: `DCFlushRange` before handing a
  buffer to IOS, `DCInvalidateRange` before and after reading what the device wrote. Without the invalidate after a
  control read we read stale bytes.
- **The ven device handle can be negative.** `USB_OpenDevice` hands back the device id as the handle, and ids are
  often negative numbers: test `USB_OpenDevice`'s return value, not the sign of the handle.
- **The AX88772's PHY is internal, at address 0x10, whatever `READ_PHY_ID` says.** The TU2-ET100 answers 0x03, and
  treating that as an external PHY powers the internal one down: no link light, ever. The chip also comes out of reset
  with its PHY powered down (IPPD): that's why IOS's driver gets no link on these adapters. Follow Linux's
  `ax88772_hw_reset` (GPIOs, PHY select, IPPD -> clear -> IPRL|PRL, IPG, node id, PHY reset + autonegotiation, medium
  mode, RX control), then program the medium mode again from what autonegotiation agreed.
- **MII registers need software MII mode** around each access (`SET_SW_MII` / `SET_HW_MII`), and BMSR's link bit is
  latched low: read it twice.
- **Bulk-in transfers carry several frames back to back**, each behind a 4-byte header (11-bit length, then its
  complement), padded to an even length, and a frame can continue in the next transfer: parse a byte stream. Outgoing
  frames get the same header, plus 4 bytes of padding when the transfer would end exactly on a packet boundary.
- **Keep IOS's own network connection on Wi-Fi.** If the console's selected connection (System Settings) is a wired
  one, IOS's wired mode disables Wi-Fi and still can't run the adapter: you lose the fallback and gain nothing.
  libwiieth never touches IOS's network settings.
- lwIP's and libogc's `<network.h>` can't be included in the same file (both define the BSD socket types): keep lwIP
  behind a plain-C interface, as `wiieth.h` does.

## Layout

    include/    wiieth.h (stack + sockets), wiieth_netx.h (net_* router), wiieth_asix.h (bare driver)
    source/     asix.c (driver), wiieth.c (netif, DHCP, link watcher, sockets), netx.c, sys_arch.c (lwIP OS layer),
                port/ (lwipopts.h, arch/cc.h, arch/sys_arch.h)
    lwip/       lwIP 2.2.1, unmodified subset (see lwip/VERSION)
    examples/   hello, netx

## Credits

- The Linux kernel's `asix` USB Ethernet driver (`drivers/net/usb/asix_devices.c`, `asix_common.c`), the reference for
  the AX88772 init sequence, registers and framing.
- [lwIP](https://savannah.nongnu.org/projects/lwip/) by Adam Dunkels and the lwIP contributors (BSD licence,
  `lwip/COPYING`).
- [libogc](https://github.com/devkitPro/libogc) and devkitPro, and everyone who documented IOS's USB interfaces on
  WiiBrew.

## Licence

MIT, copyright (c) 2026 Mitchell Vizensky: see `LICENSE`. The files in `lwip/` are lwIP's, under its BSD licence
(`lwip/COPYING`); a binary that contains libwiieth contains lwIP and must carry lwIP's notice.
