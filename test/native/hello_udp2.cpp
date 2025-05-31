// hello_multicast_send.cpp
//
// Minimal C++17 example for sending a “Hello multicast” message on the loopback interface
// so you can verify it in Wireshark (capture on “lo”).
//
// Build with:
//   g++ -std=c++17 -Wall -Wextra hello_multicast_send.cpp -o hello_multicast_send

#include <arpa/inet.h>   // inet_pton, htonl, htons
#include <errno.h>
#include <fcntl.h>       // fcntl, O_NONBLOCK
#include <stdint.h>      // uint32_t, uint16_t, uint8_t
#include <stdio.h>       // printf, fprintf, strerror
#include <string.h>      // memset, strlen
#include <sys/socket.h>  // socket, setsockopt, bind, sendto
#include <unistd.h>      // close

#define OVERRIDE_TTL 16

struct UDPTxHandle { int fd; };

/// Initialize a UDP transmit socket on `local_iface_address_nbo`.
/// Steps:
///   1) socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
///   2) bind to <local_iface_address>:0
///   3) fcntl(fd, F_SETFL, O_NONBLOCK)
///   4) setsockopt(IPPROTO_IP, IP_MULTICAST_TTL, OVERRIDE_TTL)
///   5) setsockopt(IPPROTO_IP, IP_MULTICAST_IF, local_iface_address)
///
/// Returns 0 on success or -errno on failure.
int16_t udpTxInit(UDPTxHandle* self, uint32_t local_iface_address_nbo)
{
    if (self == nullptr || local_iface_address_nbo == 0) {
        return -EINVAL;
    }

    self->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (self->fd < 0) {
        return -(int16_t)errno;
    }

    bool ok = true;

    // 1) Bind to <local_iface_address>:0
    sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(local_iface_address_nbo);
    bind_addr.sin_port        = htons(0);
    ok = ok && (::bind(self->fd,
                       reinterpret_cast<sockaddr*>(&bind_addr),
                       sizeof(bind_addr)) == 0);

    // 2) Non‐blocking
    ok = ok && (fcntl(self->fd, F_SETFL, O_NONBLOCK) == 0);

    // 3) Multicast TTL
    int ttl = OVERRIDE_TTL;
    ok = ok && (setsockopt(self->fd,
                           IPPROTO_IP,
                           IP_MULTICAST_TTL,
                           &ttl,
                           sizeof(ttl)) == 0);

    // 4) Egress interface for multicast
    uint32_t iface_be = htonl(local_iface_address_nbo);
    ok = ok && (setsockopt(self->fd,
                           IPPROTO_IP,
                           IP_MULTICAST_IF,
                           &iface_be,
                           sizeof(iface_be)) == 0);

    if (!ok) {
        int e = errno;
        close(self->fd);
        self->fd = -1;
        return (int16_t)-e;
    }
    return 0;
}

/// Send one UDP datagram. Returns +1 on success, 0 if would block, or -errno otherwise.
int16_t udpTxSend(UDPTxHandle* self,
                  uint32_t     remote_address_nbo,
                  uint16_t     remote_port,
                  uint8_t      dscp,
                  const void*  payload,
                  size_t       payload_size)
{
    if (self == nullptr || self->fd < 0
        || remote_address_nbo == 0 || remote_port == 0
        || payload == nullptr || dscp > 63U)
    {
        return -EINVAL;
    }

    // Set DSCP in TOS field
    int tos = (dscp << 2);
    setsockopt(self->fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

    sockaddr_in to_addr;
    memset(&to_addr, 0, sizeof(to_addr));
    to_addr.sin_family      = AF_INET;
    to_addr.sin_addr.s_addr = htonl(remote_address_nbo);
    to_addr.sin_port        = htons(remote_port);

    ssize_t sent = sendto(self->fd,
                          payload,
                          payload_size,
                          MSG_DONTWAIT,
                          reinterpret_cast<sockaddr*>(&to_addr),
                          sizeof(to_addr));
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return (int16_t)-errno;
    }
    return (sent == (ssize_t)payload_size) ? +1 : -EIO;
}

void udpTxClose(UDPTxHandle* self)
{
    if (self && self->fd >= 0) {
        close(self->fd);
        self->fd = -1;
    }
}

int main()
{
    const char *iface_str = "127.0.0.1";
    const char *mcast_str = "239.0.0.42";
    const uint16_t MCAST_PORT = 12345;

    // 1) Convert “127.0.0.1” → in_addr, then to host-order uint32_t
    in_addr tmp_addr;
    if (inet_pton(AF_INET, iface_str, &tmp_addr) != 1) {
        fprintf(stderr, "inet_pton(%s) failed: %s\n", iface_str, strerror(errno));
        return 1;
    }
    uint32_t local_iface_be = ntohl(tmp_addr.s_addr);

    // 2) Convert “239.0.0.42” → host-order uint32_t
    if (inet_pton(AF_INET, mcast_str, &tmp_addr) != 1) {
        fprintf(stderr, "inet_pton(%s) failed: %s\n", mcast_str, strerror(errno));
        return 1;
    }
    uint32_t mcast_be = ntohl(tmp_addr.s_addr);

    // 3) Init TX
    UDPTxHandle tx{ .fd = -1 };
    if (udpTxInit(&tx, local_iface_be) < 0) {
        fprintf(stderr, "udpTxInit failed: %s\n", strerror(errno));
        return 1;
    }

    // 4) Send “Hello multicast”

    while (1) {
        const char *message = "Hello multicast";
        int16_t send_res = udpTxSend(&tx,
                                    mcast_be,
                                    MCAST_PORT,
                                    /*dscp=*/0,
                                    message,
                                    strlen(message));
        if (send_res < 0) {
            fprintf(stderr, "udpTxSend failed: %s\n", strerror(-send_res));
            udpTxClose(&tx);
            return 1;
        }
        printf("Sent: “%s” → %s:%u\n", message, mcast_str, (unsigned)MCAST_PORT);

        // 5) Give kernel a moment to emit the packet
        ::usleep(100 * 1000);
    }

    // 6) Clean up and exit
    udpTxClose(&tx);
    return 0;
}
