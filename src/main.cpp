#include "desktop_capture.h"
#include "amf_encoder.h"
#include "rtp_h264.h"
#include <Windows.h>
#include <iostream>
#include <thread>
#include <string>
#include <chrono>

static unsigned argU(int argc, char** argv, const char* name, unsigned fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == name) return std::stoul(argv[i + 1]);
    return fallback;
}
static std::string argS(int argc, char** argv, const char* name, const char* fallback) {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == name) return argv[i + 1];
    return fallback;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        int argc = __argc; char** argv = __argv;
        unsigned output = argU(argc, argv, "--monitor", 0);
        unsigned fps = argU(argc, argv, "--fps", 60);
        unsigned bitrate = argU(argc, argv, "--bitrate-kbps", 12000);
        unsigned resolution = argU(argc, argv, "--resolution", 1080);
        if (resolution != 720 && resolution != 1080) throw std::runtime_error("--resolution must be 720 or 1080");
        auto address = argS(argc, argv, "--ip", "127.0.0.1");
        unsigned port = argU(argc, argv, "--port", 5000);
        const unsigned maxWidth = resolution == 720 ? 1280 : 1920;
        DesktopCapture capture(output, maxWidth, resolution); capture.start();
        CapturedFrame first;
        while (!capture.next(first, 1000)) {}
        AmfH264Encoder encoder(first.width, first.height, fps, bitrate);
        RtpH264Sender sender(address, static_cast<std::uint16_t>(port));
        const auto interval = std::chrono::milliseconds(1000 / fps);
        std::cout << "Streaming " << first.width << "x" << first.height << " to " << address << ":" << port << "\n";
        auto nextFrame = std::chrono::steady_clock::now();
        for (;;) {
            CapturedFrame frame;
            if (capture.next(frame, 1000)) {
                std::vector<EncodedAccessUnit> accessUnits;
                encoder.encode(frame.bgra, frame.timestamp100ns, accessUnits);
                for (const auto& au : accessUnits)
                    sender.send(au.annexB, static_cast<std::uint32_t>((au.timestamp100ns / 100) * 90 / 1000));
            }
            nextFrame += interval;
            std::this_thread::sleep_until(nextFrame);
        }
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "PowerStreamingHost", MB_ICONERROR | MB_OK);
        return 1;
    }
}
