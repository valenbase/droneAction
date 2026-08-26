#include "VideoService.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QUrl>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <utility>

namespace {
constexpr guint kNetworkLatencyMs = 50;
constexpr qint64 kConnectTimeoutMs = 5000;
constexpr qint64 kFrameTimeoutMs = 3000;
constexpr int kReconnectDelayMs = 1000;

QString cleanGStreamerMessage(const GError *error)
{
    if (!error || !error->message) {
        return QStringLiteral("stream unavailable");
    }
    return QString::fromUtf8(error->message).trimmed();
}
}

class VideoService::Backend {
public:
    explicit Backend(VideoService *owner)
        : m_owner(owner)
    {
    }

    ~Backend()
    {
        stop();
    }

    bool start(const QString &url, QString *errorMessage)
    {
        stop();

        GError *initError = nullptr;
        if (!gst_init_check(nullptr, nullptr, &initError)) {
            if (errorMessage) {
                *errorMessage = cleanGStreamerMessage(initError);
            }
            g_clear_error(&initError);
            return false;
        }

        m_pipeline = gst_pipeline_new("video-pipeline");
        m_source = gst_element_factory_make("rtspsrc", "source");
        m_queue = gst_element_factory_make("queue", "frame-queue");
        m_decoder = gst_element_factory_make("decodebin", "decoder");
        m_converter = gst_element_factory_make("videoconvert", "converter");
        m_sink = gst_element_factory_make("appsink", "video-sink");

        if (!m_pipeline || !m_source || !m_queue || !m_decoder || !m_converter || !m_sink) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("required video component is unavailable");
            }
            releaseUnparentedElements();
            return false;
        }

        const QByteArray encodedUrl = QUrl::fromUserInput(url).toEncoded();
        g_object_set(m_source,
                     "location", encodedUrl.constData(),
                     "latency", kNetworkLatencyMs,
                     "drop-on-latency", TRUE,
                     "do-retransmission", FALSE,
                     "udp-reconnect", TRUE,
                     nullptr);
        gst_util_set_object_arg(G_OBJECT(m_source), "protocols", "udp");
        gst_util_set_object_arg(G_OBJECT(m_source), "buffer-mode", "none");

        g_object_set(m_queue,
                     "max-size-buffers", 1u,
                     "max-size-bytes", 0u,
                     "max-size-time", guint64(0),
                     "flush-on-eos", TRUE,
                     nullptr);
        gst_util_set_object_arg(G_OBJECT(m_queue), "leaky", "downstream");

        GstCaps *outputCaps = gst_caps_new_simple("video/x-raw",
                                                  "format", G_TYPE_STRING, "BGRA",
                                                  nullptr);
        gst_app_sink_set_caps(GST_APP_SINK(m_sink), outputCaps);
        gst_caps_unref(outputCaps);
        gst_app_sink_set_max_buffers(GST_APP_SINK(m_sink), 1);
        gst_app_sink_set_drop(GST_APP_SINK(m_sink), TRUE);
        g_object_set(m_sink,
                     "sync", FALSE,
                     "async", FALSE,
                     "enable-last-sample", FALSE,
                     "wait-on-eos", FALSE,
                     nullptr);

        GstAppSinkCallbacks callbacks{};
        callbacks.new_sample = &Backend::onNewSample;
        gst_app_sink_set_callbacks(GST_APP_SINK(m_sink), &callbacks, this, nullptr);

        gst_bin_add_many(GST_BIN(m_pipeline),
                         m_source, m_queue, m_decoder, m_converter, m_sink, nullptr);
        if (!gst_element_link_many(m_converter, m_queue, m_sink, nullptr)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("video pipeline could not be linked");
            }
            stop();
            return false;
        }

        g_signal_connect(m_source, "pad-added", G_CALLBACK(&Backend::onSourcePadAdded), this);
        g_signal_connect(m_decoder, "pad-added", G_CALLBACK(&Backend::onDecoderPadAdded), this);

        const GstStateChangeReturn stateResult = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (stateResult == GST_STATE_CHANGE_FAILURE) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("video pipeline failed to start");
            }
            stop();
            return false;
        }
        return true;
    }

    void stop()
    {
        if (!m_pipeline) {
            return;
        }

        if (m_sink) {
            GstAppSinkCallbacks callbacks{};
            gst_app_sink_set_callbacks(GST_APP_SINK(m_sink), &callbacks, nullptr, nullptr);
        }
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_element_get_state(m_pipeline, nullptr, nullptr, GST_SECOND);
        gst_object_unref(m_pipeline);
        resetPointers();
    }

    GstBus *bus() const
    {
        return m_pipeline ? gst_element_get_bus(m_pipeline) : nullptr;
    }

    GstElement *pipeline() const
    {
        return m_pipeline;
    }

private:
    static void onSourcePadAdded(GstElement *, GstPad *pad, gpointer userData)
    {
        auto *self = static_cast<Backend *>(userData);
        if (!self || !self->m_decoder) {
            return;
        }

        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (!caps) {
            caps = gst_pad_query_caps(pad, nullptr);
        }

        bool isVideo = false;
        if (caps && !gst_caps_is_empty(caps)) {
            for (guint index = 0; index < gst_caps_get_size(caps); ++index) {
                const GstStructure *structure = gst_caps_get_structure(caps, index);
                const gchar *name = gst_structure_get_name(structure);
                const gchar *media = gst_structure_get_string(structure, "media");
                if (g_str_has_prefix(name, "video/") || g_strcmp0(media, "video") == 0) {
                    isVideo = true;
                    break;
                }
            }
        }
        if (caps) {
            gst_caps_unref(caps);
        }
        if (!isVideo) {
            return;
        }

        GstPad *sinkPad = gst_element_get_static_pad(self->m_decoder, "sink");
        if (sinkPad && !gst_pad_is_linked(sinkPad)) {
            gst_pad_link(pad, sinkPad);
        }
        if (sinkPad) {
            gst_object_unref(sinkPad);
        }
    }

    static void onDecoderPadAdded(GstElement *, GstPad *pad, gpointer userData)
    {
        auto *self = static_cast<Backend *>(userData);
        if (!self || !self->m_converter) {
            return;
        }

        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (!caps) {
            caps = gst_pad_query_caps(pad, nullptr);
        }
        const bool isRawVideo = caps && !gst_caps_is_empty(caps)
            && g_str_has_prefix(gst_structure_get_name(gst_caps_get_structure(caps, 0)), "video/x-raw");
        if (caps) {
            gst_caps_unref(caps);
        }
        if (!isRawVideo) {
            return;
        }

        GstPad *sinkPad = gst_element_get_static_pad(self->m_converter, "sink");
        if (sinkPad && !gst_pad_is_linked(sinkPad)) {
            gst_pad_link(pad, sinkPad);
        }
        if (sinkPad) {
            gst_object_unref(sinkPad);
        }
    }

    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData)
    {
        auto *self = static_cast<Backend *>(userData);
        if (!self || !self->m_owner->m_acceptFrames.load(std::memory_order_acquire)) {
            return GST_FLOW_FLUSHING;
        }

        GstSample *sample = gst_app_sink_pull_sample(sink);
        if (!sample) {
            return GST_FLOW_EOS;
        }

        GstCaps *caps = gst_sample_get_caps(sample);
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstVideoInfo info{};
        GstVideoFrame videoFrame{};
        QImage image;

        if (caps && buffer && gst_video_info_from_caps(&info, caps)
            && gst_video_frame_map(&videoFrame, &info, buffer, GST_MAP_READ)) {
            const auto *data = static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&videoFrame, 0));
            const int width = GST_VIDEO_FRAME_WIDTH(&videoFrame);
            const int height = GST_VIDEO_FRAME_HEIGHT(&videoFrame);
            const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&videoFrame, 0);
            if (data && width > 0 && height > 0 && stride >= width * 4) {
                image = QImage(data, width, height, stride, QImage::Format_ARGB32).copy();
            }
            gst_video_frame_unmap(&videoFrame);
        }
        gst_sample_unref(sample);

        if (!image.isNull() && self->m_owner->m_acceptFrames.load(std::memory_order_acquire)) {
            self->m_owner->queueFrame(std::move(image));
        }
        return GST_FLOW_OK;
    }

    void releaseUnparentedElements()
    {
        GstElement *elements[] = {m_source, m_queue, m_decoder, m_converter, m_sink};
        for (GstElement *element : elements) {
            if (element) {
                gst_object_unref(element);
            }
        }
        if (m_pipeline) {
            gst_object_unref(m_pipeline);
        }
        resetPointers();
    }

    void resetPointers()
    {
        m_pipeline = nullptr;
        m_source = nullptr;
        m_queue = nullptr;
        m_decoder = nullptr;
        m_converter = nullptr;
        m_sink = nullptr;
    }

    VideoService *m_owner = nullptr;
    GstElement *m_pipeline = nullptr;
    GstElement *m_source = nullptr;
    GstElement *m_queue = nullptr;
    GstElement *m_decoder = nullptr;
    GstElement *m_converter = nullptr;
    GstElement *m_sink = nullptr;
};

VideoService::VideoService(QObject *parent)
    : QObject(parent)
    , m_backend(std::make_unique<Backend>(this))
{
    m_busTimer.setInterval(100);
    m_healthTimer.setInterval(500);
    m_reconnectTimer.setSingleShot(true);

    connect(&m_busTimer, &QTimer::timeout, this, &VideoService::pollBus);
    connect(&m_healthTimer, &QTimer::timeout, this, &VideoService::checkStreamHealth);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &VideoService::startPipeline);
}

VideoService::~VideoService()
{
    stop();
}

void VideoService::start(const QString &rtspUrl)
{
    stop();
    m_url = rtspUrl.trimmed();
    if (m_url.isEmpty()) {
        setStatus(QStringLiteral("disabled"));
        return;
    }

    m_runningRequested = true;
    startPipeline();
}

void VideoService::stop()
{
    m_runningRequested = false;
    m_acceptFrames.store(false, std::memory_order_release);
    m_busTimer.stop();
    m_healthTimer.stop();
    m_reconnectTimer.stop();
    if (m_backend) {
        m_backend->stop();
    }
    clearPendingFrame();
    emit frameReady(QImage());
    setStatus(QStringLiteral("offline"));
}

QString VideoService::url() const
{
    return m_url;
}

QString VideoService::status() const
{
    return m_status;
}

void VideoService::startPipeline()
{
    if (!m_runningRequested) {
        return;
    }

    m_acceptFrames.store(false, std::memory_order_release);
    m_backend->stop();
    clearPendingFrame();
    m_receivedFrame = false;
    m_connectionClock.restart();
    m_acceptFrames.store(true, std::memory_order_release);

    QString error;
    setStatus(QStringLiteral("connecting"));
    if (!m_backend->start(m_url, &error)) {
        m_acceptFrames.store(false, std::memory_order_release);
        scheduleReconnect(error);
        return;
    }

    m_busTimer.start();
    m_healthTimer.start();
}

void VideoService::scheduleReconnect(const QString &reason)
{
    if (!m_runningRequested || m_reconnectTimer.isActive()) {
        return;
    }

    m_acceptFrames.store(false, std::memory_order_release);
    m_busTimer.stop();
    m_healthTimer.stop();
    m_backend->stop();
    clearPendingFrame();
    const QString detail = reason.trimmed();
    setStatus(detail.isEmpty()
                  ? QStringLiteral("reconnecting")
                  : QStringLiteral("reconnecting: %1").arg(detail));
    m_reconnectTimer.start(kReconnectDelayMs);
}

void VideoService::pollBus()
{
    GstBus *bus = m_backend->bus();
    if (!bus) {
        return;
    }

    QString failure;
    const GstMessageType messageTypes = static_cast<GstMessageType>(
        GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);
    while (GstMessage *message = gst_bus_pop_filtered(bus, messageTypes)) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            failure = cleanGStreamerMessage(error);
            g_clear_error(&error);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            failure = QStringLiteral("stream ended");
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_backend->pipeline())) {
                GstState oldState = GST_STATE_NULL;
                GstState newState = GST_STATE_NULL;
                GstState pendingState = GST_STATE_NULL;
                gst_message_parse_state_changed(message, &oldState, &newState, &pendingState);
                Q_UNUSED(oldState)
                Q_UNUSED(pendingState)
                if (newState == GST_STATE_PLAYING && !m_receivedFrame) {
                    setStatus(QStringLiteral("connecting"));
                }
            }
            break;
        default:
            break;
        }
        gst_message_unref(message);
        if (!failure.isEmpty()) {
            break;
        }
    }
    gst_object_unref(bus);

    if (!failure.isEmpty()) {
        scheduleReconnect(failure);
    }
}

void VideoService::checkStreamHealth()
{
    if (!m_runningRequested) {
        return;
    }

    if (!m_receivedFrame && m_connectionClock.isValid()
        && m_connectionClock.elapsed() > kConnectTimeoutMs) {
        scheduleReconnect(QStringLiteral("connection timeout"));
        return;
    }
    if (m_receivedFrame && m_lastFrameClock.isValid()
        && m_lastFrameClock.elapsed() > kFrameTimeoutMs) {
        scheduleReconnect(QStringLiteral("stream stalled"));
    }
}

void VideoService::queueFrame(QImage frame)
{
    bool scheduleDelivery = false;
    {
        QMutexLocker lock(&m_frameMutex);
        if (!m_acceptFrames.load(std::memory_order_relaxed)) {
            return;
        }
        m_pendingFrame = std::move(frame);
        if (!m_frameDeliveryScheduled) {
            m_frameDeliveryScheduled = true;
            scheduleDelivery = true;
        }
    }

    if (scheduleDelivery) {
        QMetaObject::invokeMethod(this, &VideoService::deliverPendingFrame, Qt::QueuedConnection);
    }
}

void VideoService::deliverPendingFrame()
{
    QImage frame;
    {
        QMutexLocker lock(&m_frameMutex);
        frame = std::move(m_pendingFrame);
        m_pendingFrame = QImage();
        m_frameDeliveryScheduled = false;
    }

    if (!m_runningRequested || !m_acceptFrames.load(std::memory_order_acquire) || frame.isNull()) {
        return;
    }

    m_receivedFrame = true;
    m_lastFrameClock.restart();
    setStatus(QStringLiteral("live"));
    emit frameReady(frame);
}

void VideoService::clearPendingFrame()
{
    QMutexLocker lock(&m_frameMutex);
    m_pendingFrame = QImage();
    m_frameDeliveryScheduled = false;
}

void VideoService::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged(m_status);
}
