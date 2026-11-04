#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

struct CapturedFrame {
    std::vector<std::uint8_t> bgra;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int64_t timestamp100ns = 0;
};

class DesktopCapture {
public:
    explicit DesktopCapture(unsigned outputIndex, std::uint32_t maxWidth = 1920, std::uint32_t maxHeight = 1080);
    void start();
    bool next(CapturedFrame& frame, unsigned timeoutMs);

private:
    using Device = Microsoft::WRL::ComPtr<ID3D11Device>;
    using Context = Microsoft::WRL::ComPtr<ID3D11DeviceContext>;
    using Texture = Microsoft::WRL::ComPtr<ID3D11Texture2D>;

    unsigned outputIndex_;
    Device device_;
    Context context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Texture staging_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t outputWidth_ = 0;
    std::uint32_t outputHeight_ = 0;
    std::uint32_t maxWidth_ = 1920;
    std::uint32_t maxHeight_ = 1080;
};
