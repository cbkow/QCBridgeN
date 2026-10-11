// SPDX-License-Identifier: MIT
// BusSink — QCView's output rings in, NDI® out (QCBridgeN/PLAN.md P3).
//
// One worker thread owns everything: it waits for the rings, attaches,
// writes the request, keeps the heartbeat, takes each published frame and
// hands the slot pointer to the NDI sender while the claim is held (the
// synchronous send copies before it returns; the async variant would need
// a second claim and is a later step), drains the audio ring into planar
// NDI audio frames, and reports the receiver count back as the status line
// QCView shows. The O13 table as the sink sees it: rings retired or
// producer dead → back to waiting, or quit if QCView started us.
//
// No Qt in here, so the same file serves a headless run and the tray app.

#pragma once

#include "qcbae/output_desc.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class NdiLib;

struct SinkConfig {
    std::string id         = "ndi";      // the manifest id: rings are /qcview-out-<id>
    std::string sourceName = "QCView";   // the NDI source name
    uint32_t    width      = 1920;
    uint32_t    height     = 1080;
    uint32_t    layers     = qcbae::kOutputLayerGuides | qcbae::kOutputLayerAnnotations;
    qcbae::OutputContent content = qcbae::OutputContent::Composite;
};

struct SinkStats {
    std::string state;          // "waiting for QCView", "attached", "sending", "refused: ...", "quitting"
    std::string reply;          // the producer's message
    int         receivers = 0;  // NDI connections
    uint64_t    frames = 0, audioPackets = 0, audioGaps = 0, audioDropped = 0;
    uint32_t    width = 0, height = 0;
    double      fps = 0.0;      // frames sent per second, last second
    bool        producerAlive = false;
    bool        launchedByProducer = false;
};

class BusSink {
public:
    BusSink(NdiLib &ndi, SinkConfig cfg);
    ~BusSink();

    void start();
    void stop();   // joins

    SinkStats stats() const;
    // Called on the worker when the producer retired the rings or died and
    // the sink was started by it: the app should quit.
    void setQuitRequest(std::function<void()> fn) { m_quit = std::move(fn); }

private:
    void run();
    void setState(const std::string &s);

    NdiLib               &m_ndi;
    SinkConfig            m_cfg;
    std::thread           m_thread;
    std::atomic<bool>     m_stop{false};
    mutable std::mutex    m_statsMutex;
    SinkStats             m_stats;
    std::function<void()> m_quit;
};
