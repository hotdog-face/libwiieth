// wiieth-hello: libwiieth's example. Brings up a USB LAN adapter (AX88772 family) with DHCP, prints the address,
// and serves a status page on TCP 80 and an echo service on TCP 7. HOME (Wii Remote) or START (GameCube pad) exits.
//
//   curl http://<address>/        nc <address> 7
//
// Copyright (c) 2026 Mitchell Vizensky. SPDX-License-Identifier: MIT
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <network.h>                     // (only for sockaddr_in, htons, AF_INET...: wiieth.h coexists with it)
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wiieth.h>

static void *xfb;
static GXRModeObj *rmode;
static char ip[16] = "?";
static u64 start_ticks;

static void video_init(void) {
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

static int quit_pressed(void) {
    WPAD_ScanPads();
    PAD_ScanPads();
    return (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START);
}

static void log_line(const char *line) { printf("  %s\n", line); }

static int listen_on(int port) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY;
    int s = wiieth_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return s;
    int r;
    if ((r = wiieth_bind(s, &a, sizeof a)) < 0 || (r = wiieth_listen(s, 4)) < 0) { wiieth_close(s); return r; }
    return s;
}

static int send_all(int s, const char *p, int n) {
    while (n > 0) {
        int r = wiieth_send(s, p, n, 0);
        if (r <= 0) return -1;
        p += r;
        n -= r;
    }
    return 0;
}

static void *http_thread(void *unused) {
    int ls = listen_on(80);
    if (ls < 0) { printf("http: listen failed (%d)\n", ls); return NULL; }
    for (;;) {
        int c = wiieth_accept(ls, NULL, NULL);
        if (c < 0) { usleep(100 * 1000); continue; }
        wiieth_set_timeout(c, 2000, 5000);
        char req[1024];
        wiieth_recv(c, req, sizeof req, 0);  // (the request itself doesn't matter: one page)
        char st[400], body[1024], head[160];
        wiieth_status(st, sizeof st);
        int n = snprintf(body, sizeof body,
                         "<!doctype html><title>libwiieth</title><h1>Hello from a Wii on the cable</h1>"
                         "<p>Address %s, up %u s, libwiieth " WIIETH_VERSION ".</p><pre>%s</pre>\n",
                         ip, (unsigned)ticks_to_secs(gettime() - start_ticks), st);
        int h = snprintf(head, sizeof head, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n"
                         "Connection: close\r\n\r\n", n);
        if (send_all(c, head, h) == 0) send_all(c, body, n);
        wiieth_close(c);
    }
    return NULL;
}

static void *echo_thread(void *unused) {
    int ls = listen_on(7);
    if (ls < 0) { printf("echo: listen failed (%d)\n", ls); return NULL; }
    for (;;) {                                // one client at a time
        int c = wiieth_accept(ls, NULL, NULL);
        if (c < 0) { usleep(100 * 1000); continue; }
        static char buf[4096];
        int n;
        while ((n = wiieth_recv(c, buf, sizeof buf, 0)) > 0 && send_all(c, buf, n) == 0) ;
        wiieth_close(c);
    }
    return NULL;
}

int main(int argc, char **argv) {
    video_init();
    WPAD_Init();
    PAD_Init();
    start_ticks = gettime();
    printf("\n\n  libwiieth %s example - HOME / START to exit\n\n", WIIETH_VERSION);
    if (IOS_GetVersion() != 58) printf("  note: running on IOS%d; the driver needs /dev/usb/ven (IOS58)\n", IOS_GetVersion());

    wiieth_set_log(log_line);
    wiieth_set_hostname("wiieth-hello");

    printf("  Looking for an ASIX USB LAN adapter...\n");
    while (!wiieth_present()) {
        if (quit_pressed()) return 0;
        VIDEO_WaitVSync();
    }
    char msg[700];
    if (wiieth_start(msg, sizeof msg) < 0) {
        printf("  Couldn't start the adapter: %s\n", msg);
        while (!quit_pressed()) VIDEO_WaitVSync();
        return 0;
    }
    printf("  Waiting for DHCP");
    unsigned int a;
    int frames = 0;
    while (!(a = wiieth_ip())) {
        if (quit_pressed()) return 0;
        VIDEO_WaitVSync();
        if (++frames % 60 == 0) { printf("."); fflush(stdout); }
    }
    snprintf(ip, sizeof ip, "%u.%u.%u.%u", a >> 24, (a >> 16) & 255, (a >> 8) & 255, a & 255);
    printf("\n\n  Address: %s  (after %u s)\n", ip, (unsigned)ticks_to_secs(gettime() - start_ticks));
    printf("  Try:  curl http://%s/    or    nc %s 7\n\n", ip, ip);

    lwp_t t;
    LWP_CreateThread(&t, http_thread, NULL, NULL, 32 * 1024, 50);
    LWP_CreateThread(&t, echo_thread, NULL, NULL, 32 * 1024, 50);
    while (!quit_pressed()) VIDEO_WaitVSync();
    return 0;
}
