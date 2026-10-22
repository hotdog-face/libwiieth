// libwiieth - wired Ethernet for Wii homebrew through a USB LAN adapter, with its own driver (wiieth_asix.h) and
// TCP/IP stack (lwIP 2.2.1, DHCP). Works with adapters IOS's network stack can't run.
//
// This header uses plain C types only, so it can be included next to libogc's <network.h> (lwIP's own headers
// can't be). Socket calls follow libogc's net_* convention: the result is >= 0, or a negative errno value.
// Addresses are struct sockaddr_in as usual (pass them as void *).
//
// SPDX-License-Identifier: MIT
#ifndef WIIETH_H
#define WIIETH_H

#ifdef __cplusplus
extern "C" {
#endif

#define WIIETH_VERSION "0.1.0"
#define WIIETH_FD_BASE 1000                  // wiieth socket fds are >= this (IOS ones are small): see wiieth_netx.h

typedef void (*wiieth_log_fn)(const char *line);

// Optional, before wiieth_start: where diagnostics go (driver reset steps, lwIP messages and assertions; one line
// per call, no newline). Default: nowhere. And the DHCP hostname (default "wii"; the string must stay valid).
void wiieth_set_log(wiieth_log_fn fn);
void wiieth_set_hostname(const char *name);

int wiieth_present(void);                    // 1 = a supported (AX88772-family) adapter is plugged in
int wiieth_start(char *msg, int size);       // open the adapter, start lwIP and DHCP (once); 0 = started.
                                             // DHCP runs in the background: poll wiieth_ip(). msg says what happened.
unsigned int wiieth_ip(void);                // our IPv4 address once DHCP is bound (network byte order), else 0
unsigned int wiieth_gateway(void);           // the default gateway (network byte order), 0 until bound
int wiieth_link(void);                       // 1 = cable link up
void wiieth_status(char *out, int size);     // one line: address, gateway, DHCP state, link, driver counters
void wiieth_sniff_hook(void (*fn)(const unsigned char *frame, int len));   // every received frame (debugging)

// Call before leaving the app (exit to the loader, launching another DOL, SYS_ResetSystem): stops all USB traffic
// to the adapter so IOS's reload doesn't meet a pending request. Waits up to timeout_ms (200 is plenty) for the
// receive thread; 1 = clean. Wired sockets fail from then on; the stack can't be started again in this run.
int wiieth_stop(int timeout_ms);

// BSD sockets on the wired stack (lwIP). TCP and UDP over IPv4. Blocking by default.
int wiieth_socket(int domain, int type, int protocol);
int wiieth_bind(int s, const void *addr, int addrlen);
int wiieth_listen(int s, int backlog);
int wiieth_accept(int s, void *addr, unsigned int *addrlen);
int wiieth_connect(int s, const void *addr, int addrlen);
int wiieth_send(int s, const void *buf, int len, int flags);
int wiieth_recv(int s, void *buf, int len, int flags);
int wiieth_sendto(int s, const void *buf, int len, int flags, const void *to, int tolen);
int wiieth_recvfrom(int s, void *buf, int len, int flags, void *from, unsigned int *fromlen);
int wiieth_close(int s);
int wiieth_get_nonblock(int s);              // 1 = non-blocking, 0 = blocking
int wiieth_set_nonblock(int s, int on);
int wiieth_set_timeout(int s, int recv_ms, int send_ms);   // SO_RCVTIMEO / SO_SNDTIMEO; 0 = wait forever

#ifdef __cplusplus
}
#endif
#endif
