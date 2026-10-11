// SPDX-License-Identifier: MIT
#include "bus_sink.h"

#include "ndi_lib.h"
#include "qcbae/audio_ring.h"
#include "qcbae/output_ring.h"

#include <Processing.NDI.Lib.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace qcbae;

namespace {
void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
}

BusSink::BusSink(NdiLib &ndi, SinkConfig cfg) : m_ndi(ndi), m_cfg(std::move(cfg)) {}

BusSink::~BusSink() { stop(); }

void BusSink::start()
{
    if (m_thread.joinable()) return;
    m_stop.store(false);
    m_thread = std::thread([this] { run(); });
}

void BusSink::stop()
{
    m_stop.store(true);
    if (m_thread.joinable()) m_thread.join();
}

SinkStats BusSink::stats() const
{
    std::lock_guard<std::mutex> lk(m_statsMutex);
    return m_stats;
}

void BusSink::setState(const std::string &s)
{
    std::lock_guard<std::mutex> lk(m_statsMutex);
    if (m_stats.state != s) std::printf("QCBridgeN: %s\n", s.c_str());
    m_stats.state = s;
}

void BusSink::run()
{
    const NDIlib_v6_3 *ndi = m_ndi.api();
    const std::string ringName  = OutputRing::name_for(m_cfg.id);
    const std::string audioName = OutputRing::audio_name_for(m_cfg.id);
    if (ringName.empty()) { setState("bad output id '" + m_cfg.id + "'"); return; }

    // The NDI source exists for as long as the sink runs, so a receiver's
    // routing holds across QCView restarts (O13's slate without the slate).
    NDIlib_send_create_t create {};
    create.p_ndi_name  = m_cfg.sourceName.c_str();
    create.p_groups    = nullptr;
    create.clock_video = false;   // QCView paces us: publish on change
    create.clock_audio = false;
    NDIlib_send_instance_t sender = ndi->send_create(&create);
    if (!sender) { setState("NDI sender could not be created"); return; }
    std::printf("QCBridgeN: NDI source '%s' (runtime %s at %s)\n", m_cfg.sourceName.c_str(),
                m_ndi.version().c_str(), m_ndi.path().c_str());

    std::vector<float> planes(static_cast<size_t>(kMaxAudioChannels) * kDefaultAudioSlotFrames);

    while (!m_stop.load()) {
        // ---- wait for QCView's rings ----
        OutputRing ring;
        AudioRing  audio;
        setState("waiting for QCView (" + ringName + ")");
        while (!m_stop.load() && !ring.open(ringName)) sleep_ms(250);
        if (m_stop.load()) break;
        if (ring.state() == OutputState::Retired || !ring.producer_alive()) {
            // A leftover from a QCView that is gone: wait for a live one.
            sleep_ms(500);
            continue;
        }
        const bool haveAudio = audio.open(audioName);
        {
            std::lock_guard<std::mutex> lk(m_statsMutex);
            m_stats.launchedByProducer = ring.launched_by_producer();
            m_stats.producerAlive = true;
            m_stats.frames = 0; m_stats.audioPackets = 0; m_stats.audioGaps = 0; m_stats.audioDropped = 0;
        }
        std::printf("QCBridgeN: attached to %s (producer pid %u, %s, audio ring %s)\n", ringName.c_str(),
                    ring.header()->producer_pid, ring.launched_by_producer() ? "started by QCView" : "user-started",
                    haveAudio ? "open" : "absent");

        ring.attach(OutputRing::now_ns());
        OutputRequest req {};
        req.protocol_version = kOutputDescVersion;
        req.width = m_cfg.width;  req.height = m_cfg.height;
        req.pixel_format = static_cast<uint32_t>(OutputPixelFormat::BGRA8);
        req.transfer     = static_cast<uint32_t>(OutputTransfer::SdrRec709);
        req.fit          = static_cast<uint32_t>(OutputFit::Letterbox);
        req.content      = static_cast<uint32_t>(m_cfg.content);
        req.layers       = m_cfg.layers;
        ring.write_request(req);
        ring.write_status(("NDI '" + m_cfg.sourceName + "': starting").c_str());
        setState("attached, waiting for the reply");

        uint64_t replyGen = 0, lastSeen = 0, aSeen = 0, aKnownGen = 0, dropped = 0;
        bool accepted = false;
        int64_t lastBeat = 0, lastSecond = OutputRing::now_ns();
        uint64_t framesAtSecond = 0;
        int lastReceivers = -1;
        bool haveExpected = false;
        uint64_t expectedFirst = 0;
        AudioSession session {};
        if (haveAudio) audio.skip_to_latest(&aSeen);
        std::string why;

        while (!m_stop.load()) {
            const int64_t now = OutputRing::now_ns();
            if (now - lastBeat > 250000000LL) { ring.heartbeat(now); lastBeat = now; }
            if (ring.state() == OutputState::Retired) { why = "QCView retired the output"; break; }
            if (!ring.producer_alive())               { why = "QCView is gone"; break; }

            OutputReply reply {};
            if (ring.read_reply(&reply, &replyGen)) {
                accepted = reply.accepted != 0;
                std::lock_guard<std::mutex> lk(m_statsMutex);
                m_stats.reply = reply.message;
                m_stats.width = reply.width; m_stats.height = reply.height;
                if (accepted) {
                    std::printf("QCBridgeN: accepted %ux%u \"%s\"\n", reply.width, reply.height, reply.message);
                } else {
                    std::printf("QCBridgeN: REFUSED \"%s\"\n", reply.message);
                    m_stats.state = std::string("refused: ") + reply.message;
                }
            }

            // ---- video: the newest frame, straight off the slot ----
            OutputFrameDesc d {};
            const void *px = nullptr;
            if (accepted && ring.acquire_latest(&lastSeen, &d, &px)) {
                if (d.pixel_format == static_cast<uint32_t>(OutputPixelFormat::BGRA8)) {
                    NDIlib_video_frame_v2_t v {};
                    v.xres = static_cast<int>(d.width);
                    v.yres = static_cast<int>(d.height);
                    v.FourCC = NDIlib_FourCC_video_type_BGRA;
                    if (d.source_fps_num > 0 && d.source_fps_den > 0) {
                        v.frame_rate_N = static_cast<int>(d.source_fps_num);
                        v.frame_rate_D = static_cast<int>(d.source_fps_den);
                    } else {
                        v.frame_rate_N = 25; v.frame_rate_D = 1;
                    }
                    v.picture_aspect_ratio = 0.0f;   // from xres/yres
                    v.frame_format_type = NDIlib_frame_format_type_progressive;
                    v.timecode = NDIlib_send_timecode_synthesize;   // O8: real timecode when the stamp carries one, later
                    v.p_data = const_cast<uint8_t *>(static_cast<const uint8_t *>(px));
                    v.line_stride_in_bytes = static_cast<int>(d.bytes_per_row);
                    v.p_metadata = nullptr;
                    ndi->send_send_video_v2(sender, &v);   // copies before it returns: the claim covers it
                    ring.report_seen(lastSeen);
                    std::lock_guard<std::mutex> lk(m_statsMutex);
                    ++m_stats.frames;
                }
                ring.release();
            }

            // ---- audio: every packet, planar float as it came ----
            if (haveAudio) {
                if (audio.read_session(&session, &aKnownGen)) haveExpected = false;
                for (int n = 0; n < 32; ++n) {
                    AudioPacketDesc ad {};
                    const auto r = audio.next_packet(&aSeen, &ad, planes.data(), &dropped);
                    if (r == AudioRing::Next::None) break;
                    if (r == AudioRing::Next::Resynced) { haveExpected = false; continue; }
                    if (haveExpected && ad.first_frame != expectedFirst) {
                        std::lock_guard<std::mutex> lk(m_statsMutex); ++m_stats.audioGaps;
                    }
                    expectedFirst = ad.first_frame + ad.frames; haveExpected = true;
                    NDIlib_audio_frame_v3_t a {};
                    a.sample_rate = static_cast<int>(ad.sample_rate);
                    a.no_channels = static_cast<int>(ad.channels);
                    a.no_samples  = static_cast<int>(ad.frames);
                    a.timecode    = NDIlib_send_timecode_synthesize;
                    a.FourCC      = NDIlib_FourCC_audio_type_FLTP;
                    a.p_data      = reinterpret_cast<uint8_t *>(planes.data());
                    a.channel_stride_in_bytes = static_cast<int>(kDefaultAudioSlotFrames * sizeof(float));
                    a.p_metadata  = nullptr;
                    ndi->send_send_audio_v3(sender, &a);
                    std::lock_guard<std::mutex> lk(m_statsMutex);
                    ++m_stats.audioPackets;
                    m_stats.audioDropped = dropped;
                }
            }

            // ---- once a second: receivers back to QCView, rate for the tray ----
            if (now - lastSecond > 1000000000LL) {
                lastSecond = now;
                const int receivers = ndi->send_get_no_connections(sender, 0);
                uint64_t frames;
                {
                    std::lock_guard<std::mutex> lk(m_statsMutex);
                    frames = m_stats.frames;
                    m_stats.fps = static_cast<double>(frames - framesAtSecond);
                    m_stats.receivers = receivers;
                    m_stats.state = accepted ? (receivers > 0 ? "sending" : "sending (no receivers)") : m_stats.state;
                }
                framesAtSecond = frames;
                if (receivers != lastReceivers) {
                    lastReceivers = receivers;
                    char line[kOutputStatusBytes];
                    std::snprintf(line, sizeof line, "NDI '%s': %d receiver%s", m_cfg.sourceName.c_str(),
                                  receivers, receivers == 1 ? "" : "s");
                    ring.write_status(line);
                    std::printf("QCBridgeN: %s\n", line);
                }
            }
            sleep_ms(1);
        }
        if (m_stop.load()) break;

        const bool launched = ring.launched_by_producer();
        std::printf("QCBridgeN: %s\n", why.c_str());
        {
            std::lock_guard<std::mutex> lk(m_statsMutex);
            m_stats.producerAlive = false;
        }
        // Explicit close so the pid leaves the header before QCView's next poll.
        ring = OutputRing();
        audio = AudioRing();
        if (launched) {
            setState("quitting (started by QCView)");
            if (m_quit) m_quit();
            break;
        }
        // User-started: back to waiting, the NDI source stays up.
    }

    ndi->send_destroy(sender);
}
