#include "desktop_capture.h"
#include <Windows.h>
#include <stdexcept>
#include <chrono>
#include <cstring>

using Microsoft::WRL::ComPtr;

static void check(HRESULT hr, const char* what) {
    if (FAILED(hr)) throw std::runtime_error(what);
}

DesktopCapture::DesktopCapture(unsigned outputIndex, std::uint32_t maxWidth, std::uint32_t maxHeight)
    : outputIndex_(outputIndex), maxWidth_(maxWidth), maxHeight_(maxHeight) {}

void DesktopCapture::start() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL level{};
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION, &device_, &level, &context_),
        "D3D11CreateDevice failed");

    ComPtr<IDXGIDevice> dxgiDevice;
    check(device_.As(&dxgiDevice), "Query IDXGIDevice failed");
    ComPtr<IDXGIAdapter> adapter;
    check(dxgiDevice->GetAdapter(&adapter), "GetAdapter failed");
    ComPtr<IDXGIOutput> output;
    check(adapter->EnumOutputs(outputIndex_, &output), "Requested monitor was not found");
    ComPtr<IDXGIOutput1> output1;
    check(output.As(&output1), "DXGI 1.2 is required");
    check(output1->DuplicateOutput(device_.Get(), &duplication_), "DuplicateOutput failed");

    DXGI_OUTPUT_DESC desc{};
    check(output->GetDesc(&desc), "GetDesc failed");
    width_ = static_cast<std::uint32_t>(desc.DesktopCoordinates.right - desc.DesktopCoordinates.left);
    height_ = static_cast<std::uint32_t>(desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
    const double scale = std::min(1.0, std::min(static_cast<double>(maxWidth_) / width_,
                                                static_cast<double>(maxHeight_) / height_));
    outputWidth_ = static_cast<std::uint32_t>(width_ * scale) & ~1u;
    outputHeight_ = static_cast<std::uint32_t>(height_ * scale) & ~1u;

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width_;
    stagingDesc.Height = height_;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    check(device_->CreateTexture2D(&stagingDesc, nullptr, &staging_), "Create staging texture failed");
}

bool DesktopCapture::next(CapturedFrame& frame, unsigned timeoutMs) {
    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = duplication_->AcquireNextFrame(timeoutMs, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (hr == DXGI_ERROR_ACCESS_LOST) {
        throw std::runtime_error("Desktop duplication access was lost; restart the host after display change");
    }
    check(hr, "AcquireNextFrame failed");

    ComPtr<ID3D11Texture2D> desktopTexture;
    check(resource.As(&desktopTexture), "Captured resource is not a D3D11 texture");
    context_->CopyResource(staging_.Get(), desktopTexture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map staging texture failed");
    frame.width = outputWidth_;
    frame.height = outputHeight_;
    frame.bgra.resize(static_cast<size_t>(outputWidth_) * outputHeight_ * 4);
    const auto* source = static_cast<const std::uint8_t*>(mapped.pData);
    if (outputWidth_ == width_ && outputHeight_ == height_) {
        for (std::uint32_t y = 0; y < height_; ++y)
            std::memcpy(frame.bgra.data() + static_cast<size_t>(y) * width_ * 4,
                        source + static_cast<size_t>(y) * mapped.RowPitch, static_cast<size_t>(width_) * 4);
    } else {
        // Nearest-neighbour scaling keeps the host predictable and avoids a second GPU pipeline.
        // It only runs when the desktop is larger than the requested 1080p ceiling.
        for (std::uint32_t y = 0; y < outputHeight_; ++y) {
            const auto sy = static_cast<std::uint64_t>(y) * height_ / outputHeight_;
            auto* dst = frame.bgra.data() + static_cast<size_t>(y) * outputWidth_ * 4;
            const auto* src = source + static_cast<size_t>(sy) * mapped.RowPitch;
            for (std::uint32_t x = 0; x < outputWidth_; ++x) {
                const auto sx = static_cast<std::uint64_t>(x) * width_ / outputWidth_;
                std::memcpy(dst + static_cast<size_t>(x) * 4, src + sx * 4, 4);
            }
        }
    }
    context_->Unmap(staging_.Get(), 0);
    duplication_->ReleaseFrame();
    frame.timestamp100ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 100;
    return true;
}
