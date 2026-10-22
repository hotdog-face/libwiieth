// libwiieth - low-level driver for ASIX AX88772 / AX88772A / AX88772B USB 2.0 Fast Ethernet adapters on the Wii,
// talking to the chip over IOS58's /dev/usb/ven (libogc's USB_* calls).
//
// Most programs want wiieth.h instead (driver + lwIP TCP/IP stack + DHCP). Use this header directly only to bring
// your own stack or to debug the hardware: it moves raw Ethernet frames.
//
// SPDX-License-Identifier: MIT
#ifndef WIIETH_ASIX_H
#define WIIETH_ASIX_H
#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASIX_VID 0x0b95

typedef void (*asix_rx_fn)(const u8 *frame, int len);

int asix_present(u16 *pid);                 // 1 = a supported adapter (0b95:7720/772a/772b/7e2b) is on USB; *pid = its id
int asix_open(char *msg, int size);         // open + reset + PHY up + autonegotiation; 0 = ok. msg: what happened
int asix_is_open(void);
void asix_mac(u8 mac[6]);                   // the adapter's MAC address (valid after asix_open)
int asix_link(int *mbps, int *full);        // 1 = PHY link up, with the negotiated speed (10/100) and duplex
int asix_apply_link(void);                  // program the MAC's medium mode to what the PHY negotiated; 1 = link up
int asix_send(const void *frame, int len);  // one Ethernet frame, 14..1514 bytes, no FCS; 0 = sent
void asix_start_rx(asix_rx_fn fn);          // start the receive thread: fn(frame, len) for every frame received
void asix_stats(char *out, int size);       // one line: link, PHY registers, medium mode, frame/error counters

// Debugging: MII register read on the internal PHY, and a raw vendor control request (len <= 32;
// in = 1 reads into data, in = 0 writes data). Both return < 0 on failure.
int asix_mdio_read(int reg);
int asix_vendor(int in, int cmd, int value, int index, int len, u8 *data);

#ifdef __cplusplus
}
#endif
#endif
