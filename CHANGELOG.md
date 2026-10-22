# Changelog

## 0.1.0 - unreleased

First standalone release, extracted from a Wii arcade-cabinet launcher where the driver has been in daily use.

- ASIX AX88772 / AX88772A / AX88772B driver over IOS58's `/dev/usb/ven` (`wiieth_asix.h`): Linux-style reset with
  the internal PHY powered up, autonegotiation, medium mode from the negotiated link, bulk framing both ways,
  counters and debug requests. Matches 0b95:7720, 772a, 772b, 7e2b (tested on 772b).
- lwIP 2.2.1 netif with DHCP and a link watcher; BSD socket calls with libogc's conventions (`wiieth.h`).
- lwIP OS layer for libogc LWP threads.
- netx: optional `net_*` router with wired-first, Wi-Fi-fallback `netx_init` (`wiieth_netx.h`).
- Logging through a callback (`wiieth_set_log`), configurable DHCP hostname.
- Makefile (`lib/libwiieth.a`, `make examples`, `make install`), Docker build script, examples `hello` and `netx`.
