// netlib adapted from SDL_Net
//
// Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
// Copyright (C) 2012 Simeon Maxein <smaxein@googlemail.com>
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef NETLIB_H
#define NETLIB_H

#include <stdint.h>

int netlib_init(void);
void netlib_quit(void);
const char* netlib_get_error(void);

typedef struct
{
    uint32_t host; // 32-bit IPv4 host address
    uint16_t port; // 16-bit protocol port
} ip_address_t;

#ifndef INADDR_ANY
  #define INADDR_ANY 0x00000000
#endif
#ifndef INADDR_NONE
  #define INADDR_NONE 0xFFFFFFFF
#endif
#ifndef INADDR_LOOPBACK
  #define INADDR_LOOPBACK 0x7f000001
#endif
#ifndef INADDR_BROADCAST
  #define INADDR_BROADCAST 0xFFFFFFFF
#endif

// Resolve a host name and port to an IP address in network form.
// If the function succeeds, it will return 0.
// If the host couldn't be resolved, the host portion of the returned
// address will be INADDR_NONE, and the function will return -1.
// If 'host' is NULL, the resolved host will be set to INADDR_ANY.
//
int netlib_resolve_host(ip_address_t *address, const char *host, uint16_t port);

// The maximum channels on a a UDP socket
#define NETLIB_MAX_UDPCHANNELS  32
// The maximum addresses bound to a single UDP socket channel
#define NETLIB_MAX_UDPADDRESSES 4

typedef struct
{
    int channel;   // The src/dst channel of the packet
    uint8_t *data; // The packet data
    int len;       // The length of the packet data
    int maxlen;    // The length of the data data
    int status;    // packet status after sending
    ip_address_t address; // The source/dest address of an incoming/outgoing packet
} udp_packet_t;

typedef struct udp_socket_s *udp_socket_t;

// Allocate/free a single UDP packet 'length' bytes long.
// The new packet is returned, or NULL if the function ran out of memory.
//
udp_packet_t *netlib_alloc_packet(int size);
void netlib_free_packet(udp_packet_t *packet);

// Open a UDP network socket
// If 'port' is non-zero, the UDP socket is bound to a local port.
// The 'port' should be given in native byte order, but is used
// internally in network (big endian) byte order, in addresses, etc.
// This allows other systems to send to this socket via a known port.
//
udp_socket_t netlib_udp_open(uint16_t port);

// Close a UDP network socket
void netlib_udp_close(udp_socket_t sock);

// Send a single packet to the specified channel.
// If the channel specified in the packet is -1, the packet will be sent to
// the address in the 'src' member of the packet.
// The packet will be updated with the status of the packet after it has
// been sent.
// This function returns 1 if the packet was sent, or 0 on error.
//
// NOTE:
// The maximum length of the packet is limited by the MTU (Maximum Transfer Unit)
// of the transport medium.  It can be as low as 250 bytes for some PPP links,
// and as high as 1500 bytes for ethernet.
//
int netlib_udp_send(udp_socket_t sock, int channel, udp_packet_t *packet);

// Receive a single packet from the UDP socket.
// The returned packet contains the source address and the channel it arrived
// on.  If it did not arrive on a bound channel, the the channel will be set
// to -1.
// The channels are checked in highest to lowest order, so if an address is
// bound to multiple channels, the highest channel with the source address
// bound will be returned.
// This function returns the number of packets read from the network, or -1
// on error.  This function does not block, so can return 0 packets pending.
//
int netlib_udp_recv(udp_socket_t sock, udp_packet_t *packet);


// Write a 16-bit value to network packet data
inline static uint16_t netlib_read16(const void *areap)
{
    const uint8_t *area = (uint8_t *)areap;
    return ((uint16_t)area[0]) << 8 | ((uint16_t)area[1]);
}

// Write a 32-bit value to network packet data
inline static uint32_t netlib_read32(const void *areap)
{
    const uint8_t *area = (const uint8_t *)areap;
    return ((uint32_t)area[0]) << 24 | ((uint32_t)area[1]) << 16
           | ((uint32_t)area[2]) << 8 | ((uint32_t)area[3]);
}

#endif
