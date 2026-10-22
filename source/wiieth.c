// libwiieth: the wired network - the ASIX driver (asix.c) as an lwIP netif, DHCP, a link watcher, and socket calls
// with libogc's conventions (see wiieth.h).
//
// Copyright (c) 2026 Mitchell Vizensky. SPDX-License-Identifier: MIT
#include <stdarg.h>
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/sockets.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"
#include "wiieth_asix.h"
#include "wiieth.h"
#include "wiieth_internal.h"

static struct netif nif;
static volatile int started, link_up;
static void (*sniff)(const unsigned char *frame, int len);
static wiieth_log_fn log_fn;
static const char *hostname = "wii";

void wiieth_set_log(wiieth_log_fn fn) { log_fn = fn; }
void wiieth_set_hostname(const char *name) { if (name && *name) hostname = name; }
void wiieth_sniff_hook(void (*fn)(const unsigned char *frame, int len)) { sniff = fn; }

void wiieth_logf(const char *fmt, ...) {
    if (!log_fn) return;
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    size_t n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    if (n) log_fn(line);
}

static err_t low_output(struct netif *n, struct pbuf *p) {
    static u8_t frame[1536] __attribute__((aligned(32)));
    u16_t len = pbuf_copy_partial(p, frame, sizeof frame, 0);
    if (len < 60) { memset(frame + len, 0, 60 - len); len = 60; }   // minimum Ethernet frame
    return asix_send(frame, len) ? ERR_IF : ERR_OK;
}

static void rx(const u8 *frame, int len) {
    if (sniff) sniff(frame, len);
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p) return;
    pbuf_take(p, frame, len);
    if (nif.input(p, &nif) != ERR_OK) pbuf_free(p);
}

static err_t nif_init(struct netif *n) {
    n->name[0] = 'e';
    n->name[1] = 'n';
    n->output = etharp_output;
    n->linkoutput = low_output;
    n->mtu = 1500;
    n->hwaddr_len = 6;
    asix_mac(n->hwaddr);
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    n->hostname = hostname;
    return ERR_OK;
}

// The PHY's link state, polled: on a change, the MAC gets the negotiated speed/duplex and lwIP hears about it
// (DHCP restarts on link up).
static void link_watch(void *unused) {
    for (;;) {
        int mbps = 0, full = 0, up = asix_link(&mbps, &full);
        if (up != link_up) {
            if (up) asix_apply_link();
            link_up = up;
            if (up) wiieth_logf("wiieth: link up, %d Mbit/s %s duplex", mbps, full ? "full" : "half");
            else wiieth_logf("wiieth: link down");
            LOCK_TCPIP_CORE();
            if (up) netif_set_link_up(&nif); else netif_set_link_down(&nif);
            UNLOCK_TCPIP_CORE();
        }
        sys_msleep(500);
    }
}

static void tcpip_ready(void *sem) { sys_sem_signal((sys_sem_t *)sem); }

int wiieth_present(void) { return asix_present(NULL); }

int wiieth_start(char *msg, int size) {
    if (started) { snprintf(msg, size, "already running"); return 0; }
    char dmsg[640];
    if (asix_open(dmsg, sizeof dmsg) < 0) {
        snprintf(msg, size, "adapter: %s", dmsg);
        wiieth_logf("wiieth: %s", msg);
        return -1;
    }
    wiieth_logf("wiieth: adapter up: %s", dmsg);
    sys_sem_t ready;
    sys_sem_new(&ready, 0);
    tcpip_init(tcpip_ready, &ready);
    sys_arch_sem_wait(&ready, 0);
    sys_sem_free(&ready);
    LOCK_TCPIP_CORE();
    netif_add(&nif, IP4_ADDR_ANY4, IP4_ADDR_ANY4, IP4_ADDR_ANY4, NULL, nif_init, tcpip_input);
    netif_set_default(&nif);
    netif_set_up(&nif);
    dhcp_start(&nif);
    UNLOCK_TCPIP_CORE();
    asix_start_rx(rx);
    sys_thread_new("wiieth-link", link_watch, NULL, 8 * 1024, 55);
    started = 1;
    snprintf(msg, size, "%s", dmsg);
    return 0;
}

unsigned int wiieth_ip(void) {
    if (!started || !link_up || !dhcp_supplied_address(&nif)) return 0;
    return ip4_addr_get_u32(netif_ip4_addr(&nif));
}

unsigned int wiieth_gateway(void) {
    if (!wiieth_ip()) return 0;
    return ip4_addr_get_u32(netif_ip4_gw(&nif));
}

int wiieth_link(void) { return link_up; }

void wiieth_status(char *out, int size) {
    char st[256];
    asix_stats(st, sizeof st);
    if (!started) { snprintf(out, size, "not started; %s", st); return; }
    char a[16], g[16];
    ip4addr_ntoa_r(netif_ip4_addr(&nif), a, sizeof a);
    ip4addr_ntoa_r(netif_ip4_gw(&nif), g, sizeof g);
    snprintf(out, size, "ip=%s gw=%s dhcp=%s %s", a, g, dhcp_supplied_address(&nif) ? "bound" : "waiting", st);
}

static int res(int r) { return r < 0 ? (errno ? -errno : -EIO) : r; }

int wiieth_socket(int domain, int type, int protocol) { return res(lwip_socket(domain, type, protocol)); }
int wiieth_bind(int s, const void *addr, int addrlen) { return res(lwip_bind(s, addr, addrlen)); }
int wiieth_listen(int s, int backlog) { return res(lwip_listen(s, backlog)); }
int wiieth_accept(int s, void *addr, unsigned int *addrlen) {
    socklen_t l = addrlen ? *addrlen : 0;
    int r = res(lwip_accept(s, addr, addrlen ? &l : NULL));
    if (addrlen) *addrlen = l;
    return r;
}
int wiieth_connect(int s, const void *addr, int addrlen) { return res(lwip_connect(s, addr, addrlen)); }
int wiieth_send(int s, const void *buf, int len, int flags) { return res(lwip_send(s, buf, len, flags)); }
int wiieth_recv(int s, void *buf, int len, int flags) { return res(lwip_recv(s, buf, len, flags)); }
int wiieth_sendto(int s, const void *buf, int len, int flags, const void *to, int tolen) {
    return res(lwip_sendto(s, buf, len, flags, to, tolen));
}
int wiieth_recvfrom(int s, void *buf, int len, int flags, void *from, unsigned int *fromlen) {
    socklen_t l = fromlen ? *fromlen : 0;
    int r = res(lwip_recvfrom(s, buf, len, flags, from, fromlen ? &l : NULL));
    if (fromlen) *fromlen = l;
    return r;
}
int wiieth_close(int s) { return res(lwip_close(s)); }
int wiieth_get_nonblock(int s) { int f = lwip_fcntl(s, F_GETFL, 0); return f < 0 ? res(f) : (f & O_NONBLOCK) != 0; }
int wiieth_set_nonblock(int s, int on) {
    int f = lwip_fcntl(s, F_GETFL, 0);
    if (f < 0) return res(f);
    return res(lwip_fcntl(s, F_SETFL, on ? (f | O_NONBLOCK) : (f & ~O_NONBLOCK)));
}
int wiieth_set_timeout(int s, int recv_ms, int send_ms) {
    struct timeval rt = { recv_ms / 1000, (recv_ms % 1000) * 1000 }, st = { send_ms / 1000, (send_ms % 1000) * 1000 };
    int r = res(lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof rt));
    return r < 0 ? r : res(lwip_setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof st));
}
