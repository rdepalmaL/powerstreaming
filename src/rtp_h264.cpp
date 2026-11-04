#include "rtp_h264.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>
#include <stdexcept>
#include <algorithm>

static constexpr size_t MTU_PAYLOAD = 1200;
static size_t startCode(const std::vector<std::uint8_t>& b, size_t p) {
    while (p + 3 < b.size()) {
        if (b[p] == 0 && b[p+1] == 0 && b[p+2] == 1) return p;
        if (p + 4 < b.size() && b[p] == 0 && b[p+1] == 0 && b[p+2] == 0 && b[p+3] == 1) return p;
        ++p;
    }
    return b.size();
}

RtpH264Sender::RtpH264Sender(const std::string& address, std::uint16_t port) : address_(address), port_(port) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw std::runtime_error("WSAStartup failed");
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) throw std::runtime_error("UDP socket failed");
    socket_ = reinterpret_cast<std::uintptr_t>(s);
    std::random_device rd; sequence_ = static_cast<std::uint16_t>(rd());
}

RtpH264Sender::~RtpH264Sender() {
    if (socket_ != ~std::uintptr_t(0)) closesocket(reinterpret_cast<SOCKET>(socket_));
    WSACleanup();
}

void RtpH264Sender::send(const std::vector<std::uint8_t>& annexB, std::uint32_t timestamp90k) {
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port_);
    if (InetPtonA(AF_INET, address_.c_str(), &destination.sin_addr) != 1)
        throw std::runtime_error("Invalid destination IPv4 address");
    size_t p = startCode(annexB, 0);
    while (p < annexB.size()) {
        size_t sc = (p + 3 < annexB.size() && annexB[p+2] == 1) ? 3 : 4;
        size_t n = p + sc;
        size_t next = startCode(annexB, n);
        size_t end = next == annexB.size() ? annexB.size() : next;
        if (end <= n) break;
        const auto* nal = annexB.data() + n;
        size_t nalSize = end - n;
        bool lastNal = next == annexB.size();
        if (nalSize <= MTU_PAYLOAD) {
            std::vector<std::uint8_t> packet(12 + nalSize);
            packet[0] = 0x80; packet[1] = static_cast<std::uint8_t>((lastNal ? 0x80 : 0) | 96);
            packet[2] = static_cast<std::uint8_t>(sequence_ >> 8); packet[3] = static_cast<std::uint8_t>(sequence_++);
            packet[4] = static_cast<std::uint8_t>(timestamp90k >> 24); packet[5] = static_cast<std::uint8_t>(timestamp90k >> 16);
            packet[6] = static_cast<std::uint8_t>(timestamp90k >> 8); packet[7] = static_cast<std::uint8_t>(timestamp90k);
            packet[8] = static_cast<std::uint8_t>(ssrc_ >> 24); packet[9] = static_cast<std::uint8_t>(ssrc_ >> 16);
            packet[10] = static_cast<std::uint8_t>(ssrc_ >> 8); packet[11] = static_cast<std::uint8_t>(ssrc_);
            std::copy(nal, nal + nalSize, packet.begin() + 12);
            ::sendto(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                      reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
        } else {
            const size_t chunk = MTU_PAYLOAD - 2;
            for (size_t offset = 1; offset < nalSize; offset += chunk) {
                size_t count = std::min(chunk, nalSize - offset);
                bool start = offset == 1, endFragment = offset + count == nalSize;
                std::vector<std::uint8_t> packet(14 + count);
                packet[0] = 0x80; packet[1] = static_cast<std::uint8_t>(((endFragment && lastNal) ? 0x80 : 0) | 96);
                packet[2] = static_cast<std::uint8_t>(sequence_ >> 8); packet[3] = static_cast<std::uint8_t>(sequence_++);
                packet[4] = static_cast<std::uint8_t>(timestamp90k >> 24); packet[5] = static_cast<std::uint8_t>(timestamp90k >> 16);
                packet[6] = static_cast<std::uint8_t>(timestamp90k >> 8); packet[7] = static_cast<std::uint8_t>(timestamp90k);
                packet[8] = static_cast<std::uint8_t>(ssrc_ >> 24); packet[9] = static_cast<std::uint8_t>(ssrc_ >> 16);
                packet[10] = static_cast<std::uint8_t>(ssrc_ >> 8); packet[11] = static_cast<std::uint8_t>(ssrc_);
                packet[12] = (nal[0] & 0xE0) | 28; packet[13] = static_cast<std::uint8_t>((start ? 0x80 : 0) | (endFragment ? 0x40 : 0) | (nal[0] & 0x1F));
                std::copy(nal + offset, nal + offset + count, packet.begin() + 14);
                ::sendto(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                          reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
            }
        }
        p = next;
    }
}
