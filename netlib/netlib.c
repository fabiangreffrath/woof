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

#if defined(_WIN32) || defined(_WIN64)
  #define __USE_W32_SOCKETS
  #define _WINSOCK_DEPRECATED_NO_WARNINGS
  #ifdef _WIN64
    #include <winsock2.h>
    #include <ws2tcpip.h>
  #else
    #include <winsock.h>
    /* NOTE: windows socklen_t is signed
     * and is defined only for winsock2. */
    typedef int socklen_t;
  #endif /* W64 */
  #include <iphlpapi.h>
#else /* UNIX */
  #include <sys/types.h>
  #ifdef __FreeBSD__
    #include <sys/socket.h>
  #endif
  #include <fcntl.h>
  #include <netinet/in.h>
  #include <sys/ioctl.h>
  #include <sys/time.h>
  #include <unistd.h>
  #ifndef __BEOS__
    #include <arpa/inet.h>
  #endif
  #include <net/if.h>
  #include <netdb.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
#endif /* WIN32 */

#ifndef __USE_W32_SOCKETS
  #ifdef __OS2__
    #define closesocket soclose
  #else /* !__OS2__ */
    #define closesocket close
  #endif /* __OS2__ */
  #define SOCKET         int
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR   -1
#endif /* __USE_W32_SOCKETS */

#ifdef __USE_W32_SOCKETS
  #define netlib_get_last_error WSAGetLastError
  #define netlib_set_last_error WSASetLastError
  #ifndef EINTR
    #define EINTR WSAEINTR
  #endif
#else
  #include <signal.h>
  #include <errno.h>
  static int netlib_get_last_error(void)
  {
      return errno;
  }
  static void netlib_set_last_error(int err)
  {
      errno = err;
  }
#endif

#include "netlib.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined(__GNUC__) || defined(__clang__)
  #define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#else
  #define PRINTF_ATTR(fmt, first)
#endif

static char errorbuf[1024];

static PRINTF_ATTR(1, 2) void netlib_set_error(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vsnprintf(errorbuf, sizeof(errorbuf), fmt, argp);
    va_end(argp);
}

const char *netlib_get_error()
{
    return errorbuf;
}

static int netlib_started;

int netlib_init(void)
{
    if (!netlib_started)
    {
#ifdef __USE_W32_SOCKETS
        // Start up the windows networking
        WORD version_wanted = MAKEWORD(1, 1);
        WSADATA wsaData;

        if (WSAStartup(version_wanted, &wsaData) != 0)
        {
            netlib_set_error("Couldn't initialize Winsock 1.1\n");
            return -1;
        }
#else
        // SIGPIPE is generated when a remote socket is closed
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_IGN);
        if (handler != SIG_DFL)
        {
            signal(SIGPIPE, handler);
        }
#endif
    }

    ++netlib_started;
    return 0;
}

void netlib_quit(void)
{
    if (netlib_started == 0)
    {
        return;
    }

    if (--netlib_started == 0)
    {
#ifdef __USE_W32_SOCKETS
        // Clean up windows networking
        if (WSACleanup() == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINPROGRESS)
            {
  #ifndef _WIN32_WCE
                WSACancelBlockingCall();
  #endif
                WSACleanup();
            }
        }
#else
        // Restore the SIGPIPE handler
        void (*handler)(int);
        handler = signal(SIGPIPE, SIG_DFL);
        if (handler != SIG_IGN)
        {
            signal(SIGPIPE, handler);
        }
#endif
    }
}

int netlib_resolve_host(ip_address_t *address, const char *host, uint16_t port)
{
    int retval = 0;

    // Perform the actual host resolution
    if (host == NULL)
    {
        address->host = INADDR_ANY;
    }
    else
    {
        address->host = inet_addr(host);
        if (address->host == INADDR_NONE)
        {
            struct hostent *hp;
            hp = gethostbyname(host);
            if (hp)
            {
                memcpy(&address->host, hp->h_addr, hp->h_length);
            }
            else
            {
                retval = -1;
                netlib_set_error("failed to get host from '%s'", host);
            }
        }
    }
    address->port = netlib_read16(&port);
    return retval;
}

typedef struct
{
    int numbound;
    ip_address_t address[NETLIB_MAX_UDPADDRESSES];
} udp_channel_t;

struct udp_socket_s
{
    int ready;
    SOCKET channel;
    ip_address_t address;
    udp_channel_t binding[NETLIB_MAX_UDPCHANNELS];
};

void netlib_udp_close(udp_socket_t sock)
{
    if (sock != NULL)
    {
        if (sock->channel != INVALID_SOCKET)
        {
            closesocket(sock->channel);
        }
        free(sock);
    }
}

udp_socket_t netlib_udp_open(uint16_t port)
{
    udp_socket_t sock;
    struct sockaddr_in sock_addr;
    socklen_t sock_len;

    // Allocate a UDP socket structure
    sock = (udp_socket_t)malloc(sizeof(*sock));

    if (sock == NULL)
    {
        netlib_set_error("Out of memory");
        goto error_return;
    }

    memset(sock, 0, sizeof(*sock));
    memset(&sock_addr, 0, sizeof(sock_addr));

    // Open the socket
    sock->channel = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->channel == INVALID_SOCKET)
    {
        netlib_set_error("Couldn't create socket");
        goto error_return;
    }

    // Bind locally, if appropriate
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = netlib_read16(&port);

    // Bind the socket for listening
    if (bind(sock->channel, (struct sockaddr *)&sock_addr, sizeof(sock_addr))
        == SOCKET_ERROR)
    {
        netlib_set_error("Couldn't bind to local port");
        goto error_return;
    }

    // Get the bound address and port
    sock_len = sizeof(sock_addr);
    if (getsockname(sock->channel, (struct sockaddr *)&sock_addr, &sock_len)
        < 0)
    {
        netlib_set_error("Couldn't get socket address");
        goto error_return;
    }

    // Fill in the channel host address
    sock->address.host = sock_addr.sin_addr.s_addr;
    sock->address.port = sock_addr.sin_port;

#ifdef SO_BROADCAST
    // Allow LAN broadcasts with the socket
    {
        int yes = 1;
        setsockopt(sock->channel, SOL_SOCKET, SO_BROADCAST, (char *)&yes,
                   sizeof(yes));
    }
#endif

#ifdef IP_ADD_MEMBERSHIP
    // Receive LAN multicast packets on 224.0.0.1
    // This automatically works on Mac OS X, Linux and BSD, but needs
    // this code on Windows.

    // A good description of multicast can be found here:
    // http://www.docs.hp.com/en/B2355-90136/ch05s05.html

    // FIXME: Add support for joining arbitrary groups to the API
    {
        struct ip_mreq g;

        g.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
        g.imr_interface.s_addr = INADDR_ANY;
        setsockopt(sock->channel, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&g,
                   sizeof(g));
    }
#endif

    // The socket is ready
    return sock;

error_return:
    netlib_udp_close(sock);

    return NULL;
}

int netlib_udp_send(udp_socket_t sock, int channel, udp_packet_t *packet)
{
    if (sock == NULL)
    {
        netlib_set_error("Passed a NULL socket");
        return 0;
    }

    udp_channel_t *binding;
    struct sockaddr_in sock_addr;
    int status;

    // Set up the variables to send packets
    int sock_len = sizeof(sock_addr);
    
    int numsent = 0;

    // if channel is < 0, then use channel specified in sock
    if (channel < 0)
    {
        sock_addr.sin_addr.s_addr = packet->address.host;
        sock_addr.sin_port = packet->address.port;
        sock_addr.sin_family = AF_INET;
        status = sendto(sock->channel, (const char *)packet->data, packet->len,
                        0, (struct sockaddr *)&sock_addr, sock_len);
        if (status >= 0)
        {
            packet->status = status;
            ++numsent;
        }
    }
    else
    {
        // Send to each of the bound addresses on the channel
        binding = &sock->binding[channel];

        for (int i = binding->numbound - 1; i >= 0; --i)
        {
            sock_addr.sin_addr.s_addr = binding->address[i].host;
            sock_addr.sin_port = binding->address[i].port;
            sock_addr.sin_family = AF_INET;
            status = sendto(sock->channel, (const char *)packet->data,
                            packet->len, 0, (struct sockaddr *)&sock_addr,
                            sock_len);
            if (status >= 0)
            {
                packet->status = status;
                ++numsent;
            }
        }
    }

    return numsent;
}

// Returns true if a socket is has data available for reading right now
static int socket_ready(SOCKET sock)
{
    int retval = 0;
    struct timeval tv;
    fd_set mask;

    // Check the file descriptors for available data
    do
    {
        netlib_set_last_error(0);

        // Set up the mask of file descriptors
        FD_ZERO(&mask);
        FD_SET(sock, &mask);

        // Set up the timeout
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        // Look!
        retval = select(sock + 1, &mask, NULL, NULL, &tv);
    } while (netlib_get_last_error() == EINTR);

    return retval == 1;
}

int netlib_udp_recv(udp_socket_t sock, udp_packet_t *packet)
{
    if (sock == NULL)
    {
        return 0;
    }

    udp_channel_t *binding;
    socklen_t sock_len;
    struct sockaddr_in sock_addr;

    int numrecv = 0;

    while (numrecv == 0 && socket_ready(sock->channel))
    {
        sock_len = sizeof(sock_addr);
        packet->status =
            recvfrom(sock->channel, (char *)packet->data, packet->maxlen, 0,
                     (struct sockaddr *)&sock_addr, &sock_len);

        if (packet->status >= 0)
        {
            packet->len = packet->status;
            packet->address.host = sock_addr.sin_addr.s_addr;
            packet->address.port = sock_addr.sin_port;
            packet->channel = -1;

            for (int i = (NETLIB_MAX_UDPCHANNELS - 1); i >= 0; --i)
            {
                binding = &sock->binding[i];

                for (int j = binding->numbound - 1; j >= 0; --j)
                {
                    if ((packet->address.host == binding->address[j].host)
                        && (packet->address.port == binding->address[j].port))
                    {
                        packet->channel = i;
                        goto foundit; // break twice
                    }
                }
            }
        foundit:
            ++numrecv;
        }
        else
        {
            packet->len = 0;
        }
    }

    sock->ready = 0;

    return numrecv;
}

udp_packet_t *netlib_alloc_packet(int size)
{
    udp_packet_t *packet;
    int error;

    error = 1;
    packet = (udp_packet_t *)malloc(sizeof(*packet));

    if (packet != NULL)
    {
        packet->maxlen = size;
        packet->data = (uint8_t *)malloc(size);
        if (packet->data != NULL)
        {
            error = 0;
        }
    }

    if (error)
    {
        netlib_set_error("Out of memory");
        netlib_free_packet(packet);
        packet = NULL;
    }
    return packet;
}

void netlib_free_packet(udp_packet_t *packet)
{
    if (packet)
    {
        free(packet->data);
        free(packet);
    }
}
