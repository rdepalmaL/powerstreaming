#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <AMF/core/Context.h>
#include <AMF/core/Factory.h>
#include <AMF/components/VideoEncoderVCE.h>

struct EncodedAccessUnit {
    std::vector<std::uint8_t> annexB;
    std::int64_t timestamp100ns = 0;
    bool keyFrame = false;
};

class AmfH264Encoder {
public:
    AmfH264Encoder(std::uint32_t width, std::uint32_t height, unsigned fps, unsigned bitrateKbps);
    ~AmfH264Encoder();
    void encode(const std::vector<std::uint8_t>& bgra, std::int64_t timestamp100ns,
                std::vector<EncodedAccessUnit>& output);

private:
    amf::AMFFactoryHelper factory_;
    amf::AMFContextPtr context_;
    amf::AMFComponentPtr encoder_;
    std::uint32_t width_, height_;
    unsigned fps_;
};
