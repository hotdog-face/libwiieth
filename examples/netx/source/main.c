// wiieth-netx: plain libogc net_* code that runs on the cable when an ASIX adapter is plugged in and on Wi-Fi when
// it isn't, through libwiieth's router (wiieth_netx.h). Serves one page on TCP 80 saying which network it's on.
// HOME (Wii Remote) or START (GameCube pad) exits.
//
// Copyright (c) 2026 Mitchell Vizensky. SPDX-License-Identifier: MIT
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <wiieth_netx.h>                     // after <network.h>: net_* below now go through netx
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *xfb;
static GXRModeObj *rmode;

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

// Ordinary libogc network code: nothing here knows about the adapter.
static void *server(void *unused) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(80);
    a.sin_addr.s_addr = INADDR_ANY;
    s32 ls = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (ls < 0 || net_bind(ls, (struct sockaddr *)&a, sizeof a) < 0 || net_listen(ls, 4) < 0) {
        printf("  listen failed (%d)\n", (int)ls);
        return NULL;
    }
    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof peer;
        s32 c = net_accept(ls, (struct sockaddr *)&peer, &plen);
        if (c < 0) { usleep(100 * 1000); continue; }
        char req[1024];
        net_recv(c, req, sizeof req, 0);
        char page[256];
        int n = snprintf(page, sizeof page, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                         "Hello over %s.\n", netx_wired ? "the cable (libwiieth)" : "Wi-Fi (IOS)");
        net_send(c, page, n, 0);
        net_close(c);
    }
    return NULL;
}

int main(int argc, char **argv) {
    video_init();
    WPAD_Init();
    PAD_Init();
    printf("\n\n  libwiieth %s netx example - HOME / START to exit\n\n", WIIETH_VERSION);
    wiieth_set_log(log_line);
    wiieth_set_hostname("wiieth-netx");

    printf("  Bringing the network up (cable first, then Wi-Fi)...\n");
    s32 r = netx_init(20000);
    if (r < 0) {
        printf("  No network (%d)\n", (int)r);
        while (!quit_pressed()) VIDEO_WaitVSync();
        return 0;
    }
    u32 a = net_gethostip();
    printf("  Up on %s: %u.%u.%u.%u - try curl http://%u.%u.%u.%u/\n", netx_wired ? "the cable" : "Wi-Fi",
           a >> 24, (a >> 16) & 255, (a >> 8) & 255, a & 255, a >> 24, (a >> 16) & 255, (a >> 8) & 255, a & 255);
    lwp_t t;
    LWP_CreateThread(&t, server, NULL, NULL, 32 * 1024, 50);
    while (!quit_pressed()) VIDEO_WaitVSync();
    return 0;
}
