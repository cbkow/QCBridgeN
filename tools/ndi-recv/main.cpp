// SPDX-License-Identifier: MIT
// ndi-recv — a receiver for measuring the sink: finds the NDI® source by
// name, connects, and reports what arrives (frames per second, raster,
// the centre pixel, audio samples) for N seconds. The consumed property,
// one hop further than the ring.
//
//   ndi-recv --name "QCView" [--seconds 10] [--dump last.ppm]
//
// Each video frame's age is the receiver's clock against the frame's
// `timestamp`, which the sender's SDK stamps at send: on one machine
// that is the NDI hop itself (encode, loopback, decode, the receive
// queue), with no receiver display in the way.

#include "ndi_lib.h"

#include <Processing.NDI.Lib.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

int main(int argc, char **argv)
{
    std::string name = "QCView", dump;
    double seconds = 10.0;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--name" && i + 1 < argc) name = argv[++i];
        else if (k == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (k == "--dump" && i + 1 < argc) dump = argv[++i];
    }
    NdiLib ndi;
    std::string err;
    if (!ndi.load(&err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const NDIlib_v6_3 *api = ndi.api();

    // Find the source. Local sources announce within a second or two.
    NDIlib_find_create_t fc {};
    fc.show_local_sources = true;
    NDIlib_find_instance_t finder = api->find_create_v2(&fc);
    if (!finder) { std::fprintf(stderr, "find_create failed\n"); return 1; }
    const NDIlib_source_t *match = nullptr;
    NDIlib_source_t chosen {};
    std::string chosenName, chosenUrl;
    const auto t0 = std::chrono::steady_clock::now();
    while (!match && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(15)) {
        api->find_wait_for_sources(finder, 1000);
        uint32_t n = 0;
        const NDIlib_source_t *src = api->find_get_current_sources(finder, &n);
        for (uint32_t i = 0; i < n; ++i) {
            const std::string full = src[i].p_ndi_name ? src[i].p_ndi_name : "";
            // "<machine> (<name>)"
            if (full.find("(" + name + ")") != std::string::npos) {
                chosenName = full;
                chosenUrl  = src[i].p_url_address ? src[i].p_url_address : "";
                match = &src[i];
                break;
            }
        }
    }
    if (!match) { std::fprintf(stderr, "no NDI source named '%s' found in 15 s\n", name.c_str()); api->find_destroy(finder); return 2; }
    chosen.p_ndi_name = chosenName.c_str();
    chosen.p_url_address = chosenUrl.empty() ? nullptr : chosenUrl.c_str();
    std::printf("found '%s' at %s\n", chosenName.c_str(), chosenUrl.c_str());

    NDIlib_recv_create_v3_t rc {};
    rc.source_to_connect_to = chosen;
    rc.color_format = NDIlib_recv_color_format_BGRX_BGRA;
    rc.bandwidth    = NDIlib_recv_bandwidth_highest;
    rc.allow_video_fields = false;
    rc.p_ndi_recv_name = "ndi-recv";
    NDIlib_recv_instance_t recv = api->recv_create_v3(&rc);
    if (!recv) { std::fprintf(stderr, "recv_create failed\n"); return 1; }

    uint64_t frames = 0, audioFrames = 0, audioSamples = 0;
    double ageSum = 0, ageMax = 0, secAgeSum = 0, secAgeMax = 0;   // ms
    uint64_t ageN = 0, secAgeN = 0;
    // A frame whose pixels fingerprint like the previous one is a repeat the
    // sender published as a new frame (64 samples across each of 16 rows).
    uint64_t prevFp = 0, dups = 0, secDups = 0;
    int w = 0, h = 0, fpsN = 0, fpsD = 0;
    unsigned char centre[4] = {0, 0, 0, 0};
    const auto start = std::chrono::steady_clock::now();
    auto lastReport = start;
    uint64_t framesAtReport = 0;
    std::string lastDump;
    while (std::chrono::steady_clock::now() - start < std::chrono::duration<double>(seconds)) {
        NDIlib_video_frame_v2_t v {};
        NDIlib_audio_frame_v3_t a {};
        const NDIlib_frame_type_e t = api->recv_capture_v3(recv, &v, &a, nullptr, 200);
        if (t == NDIlib_frame_type_video) {
            ++frames;
            w = v.xres; h = v.yres; fpsN = v.frame_rate_N; fpsD = v.frame_rate_D;
            if (v.timestamp != NDIlib_recv_timestamp_undefined) {
                using hns = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
                const int64_t now100 = std::chrono::duration_cast<hns>(std::chrono::system_clock::now().time_since_epoch()).count();
                const double age = static_cast<double>(now100 - v.timestamp) / 10000.0;
                ageSum += age; secAgeSum += age; ++ageN; ++secAgeN;
                if (age > ageMax) ageMax = age;
                if (age > secAgeMax) secAgeMax = age;
            }
            if (v.p_data && v.xres > 0 && v.yres > 0) {
                if (v.line_stride_in_bytes >= 1024) {
                    uint64_t fp = 1469598103934665603ULL;
                    for (int i = 0; i < 16; ++i) {
                        const int y = static_cast<int>((static_cast<int64_t>(v.yres) * (2 * i + 1)) / 32);
                        // 64 words spread across the row, not a strip in the middle:
                        // a side-by-side picture's middle is its seam.
                        const unsigned char *row = v.p_data + static_cast<size_t>(y) * v.line_stride_in_bytes;
                        const size_t usable = static_cast<size_t>(v.xres) * 4 - 8;
                        uint64_t word = 0;
                        for (int k = 0; k < 64; ++k) { std::memcpy(&word, row + ((usable * k / 64) & ~size_t(3)), 8); fp ^= word; fp *= 1099511628211ULL; }
                    }
                    if (prevFp != 0 && fp == prevFp) { ++dups; ++secDups; }
                    prevFp = fp;
                }
                const unsigned char *px = v.p_data + static_cast<size_t>(v.yres / 2) * v.line_stride_in_bytes
                                        + static_cast<size_t>(v.xres / 2) * 4;
                std::memcpy(centre, px, 4);
                if (!dump.empty()) {
                    lastDump.assign(reinterpret_cast<const char *>(v.p_data), static_cast<size_t>(v.line_stride_in_bytes) * v.yres);
                }
            }
            api->recv_free_video_v2(recv, &v);
        } else if (t == NDIlib_frame_type_audio) {
            ++audioFrames;
            audioSamples += static_cast<uint64_t>(a.no_samples);
            api->recv_free_audio_v3(recv, &a);
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport >= std::chrono::seconds(1)) {
            std::printf("t=%4.1f s  video %llu (+%llu, %llu repeats)  %dx%d @ %d/%d  age ms mean %.1f max %.1f  centre BGRA %02x %02x %02x %02x  audio frames %llu samples %llu  connections %d\n",
                        std::chrono::duration<double>(now - start).count(),
                        static_cast<unsigned long long>(frames), static_cast<unsigned long long>(frames - framesAtReport),
                        static_cast<unsigned long long>(secDups),
                        w, h, fpsN, fpsD, secAgeN ? secAgeSum / static_cast<double>(secAgeN) : 0.0, secAgeMax,
                        centre[0], centre[1], centre[2], centre[3],
                        static_cast<unsigned long long>(audioFrames), static_cast<unsigned long long>(audioSamples),
                        api->recv_get_no_connections(recv));
            secAgeSum = 0; secAgeMax = 0; secAgeN = 0; secDups = 0;
            std::fflush(stdout);
            framesAtReport = frames;
            lastReport = now;
        }
    }
    if (!dump.empty() && !lastDump.empty() && w > 0 && h > 0) {
        if (FILE *f = std::fopen(dump.c_str(), "wb")) {
            std::fprintf(f, "P6\n%d %d\n255\n", w, h);
            const size_t stride = lastDump.size() / static_cast<size_t>(h);
            for (int y = 0; y < h; ++y) {
                const auto *row = reinterpret_cast<const unsigned char *>(lastDump.data()) + static_cast<size_t>(y) * stride;
                for (int x = 0; x < w; ++x) {
                    const unsigned char rgb[3] = { row[x * 4 + 2], row[x * 4 + 1], row[x * 4 + 0] };
                    std::fwrite(rgb, 1, 3, f);
                }
            }
            std::fclose(f);
            std::printf("dumped the last frame to %s\n", dump.c_str());
        }
    }
    std::printf("done: %llu video frames in %.1f s = %.2f fps, %llu repeats, age ms mean %.1f max %.1f over %llu stamped, %llu audio frames, %llu samples\n",
                static_cast<unsigned long long>(frames), seconds, static_cast<double>(frames) / seconds, static_cast<unsigned long long>(dups),
                ageN ? ageSum / static_cast<double>(ageN) : 0.0, ageMax, static_cast<unsigned long long>(ageN),
                static_cast<unsigned long long>(audioFrames), static_cast<unsigned long long>(audioSamples));
    api->recv_destroy(recv);
    api->find_destroy(finder);
    return 0;
}
