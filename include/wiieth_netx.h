// libwiieth - optional net_* router: lets existing libogc network code use the wired adapter when it's there and
// IOS's own network (Wi-Fi) when it isn't, without changing its calls.
//
// Include it after <network.h> in the files that make net_* calls: the macros below send those calls here. New
// sockets go to the network in use (netx_wired); calls on an existing socket go by its fd (wiieth's start at
// WIIETH_FD_BASE), so sockets opened before a switch keep working on their own stack.
//
//   #include <network.h>
//   #include <wiieth_netx.h>
//   ...
//   if (netx_init(20000) < 0) ...           // wired if an adapter gets an address within 20 s, else Wi-Fi
//   s32 s = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);   // -> whichever is up
//
// Routed: socket bind listen accept connect send sendto recv recvfrom close fcntl (F_GETFL/F_SETFL with
// NETX_O_NONBLOCK) gethostip. Everything else (setsockopt, select/poll, gethostbyname, ...) still goes to IOS.
//
// SPDX-License-Identifier: MIT
#ifndef WIIETH_NETX_H
#define WIIETH_NETX_H
#include <network.h>
#include "wiieth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETX_O_NONBLOCK 0x04                 // the non-blocking flag IOS's net_fcntl takes (not newlib's O_NONBLOCK)

extern volatile int netx_wired;              // 1 = new sockets go to the wired network (set by netx_init)

// Brings a network up: the wired adapter first if one is plugged in (waits up to wired_timeout_ms for DHCP; it
// keeps trying in the background either way), then IOS's net_init (Wi-Fi, the console's own settings).
// Returns 0 with netx_wired set to the one in use, or < 0 (net_init's error) if neither came up.
s32 netx_init(int wired_timeout_ms);

s32 netx_socket(u32 domain, u32 type, u32 protocol);
s32 netx_bind(s32 s, struct sockaddr *name, socklen_t namelen);
s32 netx_listen(s32 s, u32 backlog);
s32 netx_accept(s32 s, struct sockaddr *addr, socklen_t *addrlen);
s32 netx_connect(s32 s, struct sockaddr *addr, socklen_t addrlen);
s32 netx_send(s32 s, const void *data, s32 size, u32 flags);
s32 netx_sendto(s32 s, const void *data, s32 len, u32 flags, struct sockaddr *to, socklen_t tolen);
s32 netx_recv(s32 s, void *mem, s32 len, u32 flags);
s32 netx_recvfrom(s32 s, void *mem, s32 len, u32 flags, struct sockaddr *from, socklen_t *fromlen);
s32 netx_close(s32 s);
s32 netx_fcntl(s32 s, u32 cmd, u32 flags);
u32 netx_gethostip(void);                    // the address of the network in use, like net_gethostip

#ifndef WIIETH_NETX_IMPL
#define net_socket netx_socket
#define net_bind netx_bind
#define net_listen netx_listen
#define net_accept netx_accept
#define net_connect netx_connect
#define net_send netx_send
#define net_sendto netx_sendto
#define net_recv netx_recv
#define net_recvfrom netx_recvfrom
#define net_close netx_close
#define net_fcntl netx_fcntl
#define net_gethostip netx_gethostip
#endif

#ifdef __cplusplus
}
#endif
#endif
