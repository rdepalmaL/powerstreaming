#include "amf_encoder.h"
#include <stdexcept>
#include <cstring>

static void amfCheck(amf::AMF_RESULT result, const char* what) {
    if (result != AMF_OK) throw std::runtime_error(what);
}

AmfH264Encoder::AmfH264Encoder(std::uint32_t width, std::uint32_t height, unsigned fps, unsigned bitrateKbps)
    : width_(width), height_(height), fps_(fps) {
    amfCheck(factory_.Init(), "AMF runtime could not be loaded; install an AMD driver");
    amfCheck(factory_.GetFactory()->CreateContext(&context_), "AMF CreateContext failed");
    amfCheck(context_->InitDX11(nullptr), "AMF InitDX11 failed");
    amfCheck(factory_.GetFactory()->CreateComponent(context_, AMFVideoEncoderVCE_AVC, &encoder_),
             "AMF H.264 encoder is not available on this GPU/driver");

    encoder_->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_MAIN);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, AMFConstructSize(width_, height_));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, AMFConstructRate(fps_, 1));
    encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, static_cast<amf_int64>(bitrateKbps) * 1000);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, true);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
    encoder_->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR);
    amfCheck(encoder_->Init(AMF_SURFACE_BGRA, width_, height_), "AMF encoder does not accept BGRA input");
}

AmfH264Encoder::~AmfH264Encoder() {
    if (encoder_) encoder_->Terminate();
    if (context_) context_->Terminate();
    factory_.Terminate();
}

void AmfH264Encoder::encode(const std::vector<std::uint8_t>& bgra, std::int64_t timestamp100ns,
                            std::vector<EncodedAccessUnit>& output) {
    amf::AMFSurfacePtr surface;
    amfCheck(context_->AllocSurface(AMF_MEMORY_HOST, AMF_SURFACE_BGRA, width_, height_, &surface),
             "AMF surface allocation failed");
    amf::AMFPlane* plane = surface->GetPlaneAt(0);
    auto* dst = static_cast<std::uint8_t*>(plane->GetNative());
    const auto pitch = static_cast<size_t>(plane->GetHPitch());
    for (std::uint32_t y = 0; y < height_; ++y)
        std::memcpy(dst + y * pitch, bgra.data() + static_cast<size_t>(y) * width_ * 4, width_ * 4);
    surface->SetPts(timestamp100ns);
    auto result = encoder_->SubmitInput(surface);
    if (result != AMF_OK && result != AMF_NEED_MORE_INPUT) throw std::runtime_error("AMF SubmitInput failed");

    for (;;) {
        amf::AMFDataPtr data;
        result = encoder_->QueryOutput(&data);
        if (result == AMF_REPEAT || result == AMF_NEED_MORE_INPUT) break;
        amfCheck(result, "AMF QueryOutput failed");
        amf::AMFBufferPtr buffer(data);
        EncodedAccessUnit au;
        au.timestamp100ns = timestamp100ns;
        au.annexB.assign(static_cast<const std::uint8_t*>(buffer->GetNative()),
                         static_cast<const std::uint8_t*>(buffer->GetNative()) + buffer->GetSize());
        amf_int64 frameType = 0;
        buffer->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_FRAME_TYPE, &frameType);
        au.keyFrame = frameType != AMF_VIDEO_ENCODER_OUTPUT_FRAME_TYPE_P;
        output.push_back(std::move(au));
    }
}
