#pragma once
#include <cstdint>
#include <string>
#include <vector>

class RtpH264Sender {
public:
    RtpH264Sender(const std::string& address, std::uint16_t port);
    ~RtpH264Sender();
    void send(const std::vector<std::uint8_t>& annexB, std::uint32_t timestamp90k);

private:
    std::uintptr_t socket_ = ~std::uintptr_t(0);
    std::uint16_t sequence_ = 0;
    std::uint32_t ssrc_ = 0x504F5752;
    std::string address_;
    std::uint16_t port_;
};
