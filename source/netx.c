// libwiieth: net_* calls routed to IOS's network (Wi-Fi) or the wired one (see wiieth_netx.h).
//
// Copyright (c) 2026 Mitchell Vizensky. SPDX-License-Identifier: MIT
#define WIIETH_NETX_IMPL
#include <gccore.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "wiieth_netx.h"
#include "wiieth_internal.h"

#define LW(s) ((s) >= WIIETH_FD_BASE)

volatile int netx_wired;

s32 netx_init(int wired_timeout_ms) {
    if (wiieth_present()) {
        char m[700];
        if (wiieth_start(m, sizeof m) == 0) {
            for (int t = 0; t < wired_timeout_ms && !wiieth_ip(); t += 100) usleep(100 * 1000);
            if (wiieth_ip()) { netx_wired = 1; return 0; }
            wiieth_logf("netx: no wired address after %d ms; trying IOS (Wi-Fi)", wired_timeout_ms);
        }
    }
    netx_wired = 0;
    s32 r;
    while ((r = net_init()) == -EAGAIN) usleep(100 * 1000);
    if (r < 0) return r;
    // DHCP can finish well after net_init returns; meanwhile the cable may still come up.
    for (int t = 0; t < 30000; t += 100) {
        if (net_gethostip()) return 0;
        if (wiieth_ip()) { netx_wired = 1; return 0; }
        usleep(100 * 1000);
    }
    return -ETIMEDOUT;
}

u32 netx_gethostip(void) { return netx_wired ? wiieth_ip() : net_gethostip(); }

s32 netx_socket(u32 domain, u32 type, u32 protocol) {
    return netx_wired ? wiieth_socket(domain, type, protocol) : net_socket(domain, type, protocol);
}
s32 netx_bind(s32 s, struct sockaddr *name, socklen_t namelen) {
    return LW(s) ? wiieth_bind(s, name, namelen) : net_bind(s, name, namelen);
}
s32 netx_listen(s32 s, u32 backlog) { return LW(s) ? wiieth_listen(s, backlog) : net_listen(s, backlog); }
s32 netx_accept(s32 s, struct sockaddr *addr, socklen_t *addrlen) {
    if (!LW(s)) return net_accept(s, addr, addrlen);
    unsigned int l = addrlen ? *addrlen : 0;
    s32 r = wiieth_accept(s, addr, addrlen ? &l : NULL);
    if (addrlen) *addrlen = l;
    return r;
}
s32 netx_connect(s32 s, struct sockaddr *addr, socklen_t addrlen) {
    return LW(s) ? wiieth_connect(s, addr, addrlen) : net_connect(s, addr, addrlen);
}
s32 netx_send(s32 s, const void *data, s32 size, u32 flags) {
    return LW(s) ? wiieth_send(s, data, size, flags) : net_send(s, data, size, flags);
}
s32 netx_sendto(s32 s, const void *data, s32 len, u32 flags, struct sockaddr *to, socklen_t tolen) {
    return LW(s) ? wiieth_sendto(s, data, len, flags, to, tolen) : net_sendto(s, data, len, flags, to, tolen);
}
s32 netx_recv(s32 s, void *mem, s32 len, u32 flags) {
    return LW(s) ? wiieth_recv(s, mem, len, flags) : net_recv(s, mem, len, flags);
}
s32 netx_recvfrom(s32 s, void *mem, s32 len, u32 flags, struct sockaddr *from, socklen_t *fromlen) {
    if (!LW(s)) return net_recvfrom(s, mem, len, flags, from, fromlen);
    unsigned int l = fromlen ? *fromlen : 0;
    s32 r = wiieth_recvfrom(s, mem, len, flags, from, fromlen ? &l : NULL);
    if (fromlen) *fromlen = l;
    return r;
}
s32 netx_close(s32 s) { return LW(s) ? wiieth_close(s) : net_close(s); }
s32 netx_fcntl(s32 s, u32 cmd, u32 flags) {
    if (!LW(s)) return net_fcntl(s, cmd, flags);
    if (cmd == F_GETFL) { s32 r = wiieth_get_nonblock(s); return r < 0 ? r : r ? NETX_O_NONBLOCK : 0; }
    if (cmd == F_SETFL) return wiieth_set_nonblock(s, (flags & NETX_O_NONBLOCK) != 0);
    return -EINVAL;
}
