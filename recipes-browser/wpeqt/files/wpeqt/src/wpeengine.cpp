#include "wpeengine.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QtGlobal>

#include <cmath>
#include <cstdio>

namespace {

const char *const kDefaultUserAgent =
    "Mozilla/5.0 (X11; Linux) AppleWebKit/605.1.15 (KHTML, like Gecko) "
    "Version/16.4 Safari/605.1.15";

} // namespace

const wpe_view_backend_exportable_fdo_egl_client WpeEngine::s_exportClient = {
    WpeEngine::onExportEglImage,
    WpeEngine::onExportFdoEglImage,
    WpeEngine::onExportShmBuffer,
    nullptr,
    nullptr,
};

WpeEngine::WpeEngine(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

WpeEngine::~WpeEngine()
{
    if (m_pumpTimer) {
        m_pumpTimer->stop();
        delete m_pumpTimer;
        m_pumpTimer = nullptr;
    }

    if (m_glContext) {
        m_glContext->makeCurrent(m_offscreenSurface);
        if (m_textureId) {
            QOpenGLFunctions f(QOpenGLContext::currentContext());
            f.glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
        m_glContext->doneCurrent();
        delete m_glContext;
        m_glContext = nullptr;
    }

    if (m_offscreenSurface) {
        delete m_offscreenSurface;
        m_offscreenSurface = nullptr;
    }

    if (m_glibContext) {
        g_main_context_unref(m_glibContext);
        m_glibContext = nullptr;
    }

    if (m_webView) {
        g_object_unref(m_webView);
        m_webView = nullptr;
    } else if (m_exportable) {
        wpe_view_backend_exportable_fdo_destroy(m_exportable);
        m_exportable = nullptr;
    }
}

void WpeEngine::attachToWindow(QQuickWindow *window)
{
    m_window = window;
    QTimer::singleShot(0, this, [this] { tryInitialize(); });
}

void WpeEngine::tryInitialize()
{
    if (m_webView)
        return;

    if (!m_window) {
        QTimer::singleShot(16, this, [this] { tryInitialize(); });
        return;
    }

    QSGRendererInterface *rendererInterface = m_window->rendererInterface();
    if (!rendererInterface) {
        QTimer::singleShot(16, this, [this] { tryInitialize(); });
        return;
    }

    auto *sceneGraphContext = static_cast<QOpenGLContext *>(
        rendererInterface->getResource(m_window, QSGRendererInterface::OpenGLContextResource));
    if (!m_window->isSceneGraphInitialized() || !sceneGraphContext) {
        QTimer::singleShot(16, this, [this] { tryInitialize(); });
        return;
    }

    initialize();
}

void WpeEngine::initialize()
{
    QSGRendererInterface *rendererInterface = m_window->rendererInterface();
    auto *sceneGraphContext = static_cast<QOpenGLContext *>(
        rendererInterface->getResource(m_window, QSGRendererInterface::OpenGLContextResource));
    if (!sceneGraphContext) {
        qWarning("WpeEngine: scene graph OpenGL context not available");
        return;
    }

    m_glContext = new QOpenGLContext(this);
    m_glContext->setShareContext(sceneGraphContext);
    if (!m_glContext->create()) {
        qWarning("WpeEngine: unable to create shared OpenGL context");
        return;
    }

    m_offscreenSurface = new QOffscreenSurface();
    m_offscreenSurface->setFormat(m_glContext->format());
    m_offscreenSurface->create();
    if (!m_glContext->makeCurrent(m_offscreenSurface)) {
        qWarning("WpeEngine: unable to make OpenGL context current");
        return;
    }

    m_eglImageTarget = reinterpret_cast<void (*)(unsigned int, void *)>(
        eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!m_eglImageTarget)
        qWarning("WpeEngine: glEGLImageTargetTexture2DOES unavailable");

    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (!wpe_fdo_initialize_for_egl_display(eglDisplay)) {
        qWarning("WpeEngine: wpe_fdo_initialize_for_egl_display failed");
        m_glContext->doneCurrent();
        return;
    }

    m_exportable = wpe_view_backend_exportable_fdo_egl_create(
        &s_exportClient, this,
        static_cast<uint32_t>(m_viewWidth),
        static_cast<uint32_t>(m_viewHeight));
    if (!m_exportable) {
        qWarning("WpeEngine: failed to create exportable view backend");
        m_glContext->doneCurrent();
        return;
    }

    m_viewBackend = wpe_view_backend_exportable_fdo_get_view_backend(m_exportable);
    m_wkBackend = webkit_web_view_backend_new(
        m_viewBackend,
        reinterpret_cast<GDestroyNotify>(wpe_view_backend_exportable_fdo_destroy),
        m_exportable);

    m_glibContext = g_main_context_new();
    g_main_context_push_thread_default(m_glibContext);

    m_webView = webkit_web_view_new(m_wkBackend);
    g_object_ref_sink(m_webView);

    applySettings();
    connectWebSignals();

    wpe_view_backend_set_fullscreen_handler(m_viewBackend,
                                             WpeEngine::onFullscreenHandler, this);
    wpe_view_backend_dispatch_set_size(m_viewBackend,
                                       static_cast<uint32_t>(m_viewWidth),
                                       static_cast<uint32_t>(m_viewHeight));
    wpe_view_backend_dispatch_set_device_scale_factor(m_viewBackend,
                                                      static_cast<float>(m_scaleFactor));
    wpe_view_backend_add_activity_state(
        m_viewBackend,
        (m_pendingWindowed ? wpe_view_activity_state_in_window : 0) |
            (m_pendingVisible ? wpe_view_activity_state_visible : 0) |
            (m_pendingFocused ? wpe_view_activity_state_focused : 0));

    m_glContext->doneCurrent();

    m_pumpTimer = new QTimer(this);
    m_pumpTimer->setTimerType(Qt::PreciseTimer);
    connect(m_pumpTimer, &QTimer::timeout, this, &WpeEngine::pumpGlib);
    m_pumpTimer->start(1);

    if (!m_pendingUrl.isEmpty()) {
        loadUrl(m_pendingUrl);
        m_pendingUrl.clear();
    } else if (!m_initialUrl.isEmpty()) {
        loadUrl(m_initialUrl);
    }
}

void WpeEngine::applySettings()
{
    WebKitSettings *settings = webkit_web_view_get_settings(m_webView);

    webkit_settings_set_enable_javascript(settings, true);
    webkit_settings_set_enable_webgl(settings, true);
    webkit_settings_set_enable_webrtc(settings, true);
    webkit_settings_set_enable_media(settings, true);
    webkit_settings_set_enable_mediasource(settings, true);
    webkit_settings_set_enable_media_stream(settings, true);
    webkit_settings_set_enable_media_capabilities(settings, true);
    webkit_settings_set_enable_encrypted_media(settings, true);
    webkit_settings_set_enable_webaudio(settings, true);
    webkit_settings_set_enable_2d_canvas_acceleration(settings, true);
    webkit_settings_set_enable_fullscreen(settings, true);
    webkit_settings_set_enable_smooth_scrolling(settings, true);
    webkit_settings_set_enable_page_cache(settings, true);
    webkit_settings_set_auto_load_images(settings, true);
    webkit_settings_set_media_playback_allows_inline(settings, true);
    webkit_settings_set_media_playback_requires_user_gesture(settings, false);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, true);
    webkit_settings_set_media_content_types_requiring_hardware_support(
        settings, "video/mp4;video/x-h264;video/h264");

    const char *envUserAgent = qgetenv("WPEQT_USER_AGENT").constData();
    if (envUserAgent && envUserAgent[0] != '\0')
        webkit_settings_set_user_agent(settings, envUserAgent);
    else
        webkit_settings_set_user_agent(settings, kDefaultUserAgent);

    WebKitNetworkSession *session = webkit_network_session_get_default();
    WebKitCookieManager *cookies = webkit_network_session_get_cookie_manager(session);
    webkit_cookie_manager_set_accept_policy(cookies, WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS);
    webkit_network_session_set_tls_errors_policy(session, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    WebKitWebContext *context = webkit_web_context_get_default();
    webkit_web_context_set_cache_model(context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
}

void WpeEngine::connectWebSignals()
{
    g_signal_connect(m_webView, "notify::uri",
                     G_CALLBACK(WpeEngine::onUriChanged), this);
    g_signal_connect(m_webView, "notify::title",
                     G_CALLBACK(WpeEngine::onTitleChanged), this);
    g_signal_connect(m_webView, "notify::estimated-load-progress",
                     G_CALLBACK(WpeEngine::onProgressChanged), this);
    g_signal_connect(m_webView, "notify::can-go-back",
                     G_CALLBACK(WpeEngine::onCanGoBackChanged), this);
    g_signal_connect(m_webView, "notify::can-go-forward",
                     G_CALLBACK(WpeEngine::onCanGoForwardChanged), this);
    g_signal_connect(m_webView, "notify::is-loading",
                     G_CALLBACK(WpeEngine::onLoadingChanged), this);
    g_signal_connect(m_webView, "load-changed",
                     G_CALLBACK(WpeEngine::onLoadChanged), this);
}

void WpeEngine::pumpGlib()
{
    if (!m_glibContext)
        return;

    int processed = 0;
    while (processed < 16 && g_main_context_iteration(m_glibContext, FALSE))
        ++processed;

    if (m_pumpTimer)
        m_pumpTimer->start(processed > 0 ? 0 : 3);
}

quint32 WpeEngine::eventTime()
{
    return static_cast<quint32>(m_clock.elapsed());
}

void WpeEngine::loadUrl(const QString &url)
{
    if (!m_webView) {
        m_pendingUrl = url;
        return;
    }

    QString address = url.trimmed();
    if (address.isEmpty())
        return;

    if (!address.startsWith(QStringLiteral("http://")) &&
        !address.startsWith(QStringLiteral("https://")) &&
        !address.startsWith(QStringLiteral("about:")) &&
        !address.startsWith(QStringLiteral("data:")))
        address.prepend(QStringLiteral("https://"));

    QByteArray utf8 = address.toUtf8();
    webkit_web_view_load_uri(m_webView, utf8.constData());
}

void WpeEngine::goBack()
{
    if (m_webView && webkit_web_view_can_go_back(m_webView))
        webkit_web_view_go_back(m_webView);
}

void WpeEngine::goForward()
{
    if (m_webView && webkit_web_view_can_go_forward(m_webView))
        webkit_web_view_go_forward(m_webView);
}

void WpeEngine::reload()
{
    if (m_webView)
        webkit_web_view_reload(m_webView);
}

void WpeEngine::stop()
{
    if (m_webView)
        webkit_web_view_stop_loading(m_webView);
}

void WpeEngine::setViewSize(int width, int height)
{
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    m_viewWidth = width;
    m_viewHeight = height;
    if (m_viewBackend) {
        wpe_view_backend_dispatch_set_size(m_viewBackend,
                                           static_cast<uint32_t>(m_viewWidth),
                                           static_cast<uint32_t>(m_viewHeight));
    }
}

void WpeEngine::setScaleFactor(qreal scale)
{
    m_scaleFactor = scale;
    if (m_viewBackend)
        wpe_view_backend_dispatch_set_device_scale_factor(m_viewBackend,
                                                          static_cast<float>(scale));
}

void WpeEngine::notifyActivity(bool visible, bool focused, bool windowed)
{
    m_pendingVisible = visible;
    m_pendingFocused = focused;
    m_pendingWindowed = windowed;

    if (!m_viewBackend)
        return;

    uint32_t active = 0;
    uint32_t inactive = 0;

    if (windowed)
        active |= wpe_view_activity_state_in_window;
    else
        inactive |= wpe_view_activity_state_in_window;

    if (visible)
        active |= wpe_view_activity_state_visible;
    else
        inactive |= wpe_view_activity_state_visible;

    if (focused)
        active |= wpe_view_activity_state_focused;
    else
        inactive |= wpe_view_activity_state_focused;

    wpe_view_backend_remove_activity_state(m_viewBackend, inactive);
    wpe_view_backend_add_activity_state(m_viewBackend, active);
}

void WpeEngine::dispatchMouse(int type, int x, int y, int button,
                              bool pressed, quint32 buttons, quint32 modifiers)
{
    if (!m_viewBackend)
        return;

    wpe_input_pointer_event event = {};
    event.type = static_cast<wpe_input_pointer_event_type>(type);
    event.time = eventTime();
    event.x = x;
    event.y = y;
    event.button = static_cast<uint32_t>(button);
    event.state = pressed ? 1 : 0;
    event.modifiers = modifiers | buttons;

    wpe_view_backend_dispatch_pointer_event(m_viewBackend, &event);
}

void WpeEngine::dispatchAxis(int x, int y, double dx, double dy, quint32 modifiers)
{
    if (!m_viewBackend)
        return;

    wpe_input_axis_2d_event event = {};
    event.base.type = static_cast<wpe_input_axis_event_type>(
        wpe_input_axis_event_type_motion_smooth | wpe_input_axis_event_type_mask_2d);
    event.base.time = eventTime();
    event.base.x = x;
    event.base.y = y;
    event.base.modifiers = modifiers;
    event.x_axis = dx;
    event.y_axis = dy;

    wpe_view_backend_dispatch_axis_event(m_viewBackend, &event.base);
}

void WpeEngine::dispatchKey(quint32 keyCode, quint32 hardwareKeyCode,
                            bool pressed, quint32 modifiers)
{
    if (!m_viewBackend)
        return;

    wpe_input_keyboard_event event = {};
    event.time = eventTime();
    event.key_code = keyCode;
    event.hardware_key_code = hardwareKeyCode;
    event.pressed = pressed;
    event.modifiers = modifiers;

    wpe_view_backend_dispatch_keyboard_event(m_viewBackend, &event);
}

bool WpeEngine::takeFrame(FrameInfo *frame)
{
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (frame)
        *frame = m_frame;
    return m_frameChanged;
}

void WpeEngine::acceptFullscreen(bool enter)
{
    if (!m_viewBackend)
        return;

    if (enter)
        wpe_view_backend_dispatch_did_enter_fullscreen(m_viewBackend);
    else
        wpe_view_backend_dispatch_did_exit_fullscreen(m_viewBackend);
}

void WpeEngine::presentImage(wpe_fdo_egl_exported_image *image)
{
    const uint32_t imageWidth = wpe_fdo_egl_exported_image_get_width(image);
    const uint32_t imageHeight = wpe_fdo_egl_exported_image_get_height(image);

    if (!m_glContext->makeCurrent(m_offscreenSurface)) {
        qWarning("WpeEngine: makeCurrent failed during frame export");
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
        return;
    }

    QOpenGLFunctions gl(QOpenGLContext::currentContext());

    if (m_textureId) {
        gl.glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }

    quint32 texture = 0;
    gl.glGenTextures(1, &texture);
    gl.glBindTexture(GL_TEXTURE_2D, texture);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (m_eglImageTarget)
        m_eglImageTarget(GL_TEXTURE_2D, wpe_fdo_egl_exported_image_get_egl_image(image));

    gl.glBindTexture(GL_TEXTURE_2D, 0);
    m_glContext->doneCurrent();

    m_textureId = texture;
    m_textureSize = QSize(static_cast<int>(imageWidth), static_cast<int>(imageHeight));

    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_frame.textureId = m_textureId;
        m_frame.width = m_textureSize.width();
        m_frame.height = m_textureSize.height();
        m_frameChanged = true;
    }

    if (m_exportedImage)
        wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image(
            m_exportable, m_exportedImage);
    m_exportedImage = image;

    wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);

    Q_EMIT frameReady();
}

void WpeEngine::discardShmBuffer(wpe_fdo_shm_exported_buffer *buffer)
{
    qWarning("WpeEngine: software-rendered SHM buffer rejected; "
             "hardware acceleration is mandatory");
    if (m_exportable) {
        wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer(
            m_exportable, buffer);
        wpe_view_backend_exportable_fdo_dispatch_frame_complete(m_exportable);
    }
}

void WpeEngine::onExportEglImage(void *data, EGLImageKHR image)
{
    Q_UNUSED(data)
    Q_UNUSED(image)
}

void WpeEngine::onExportFdoEglImage(void *data, wpe_fdo_egl_exported_image *image)
{
    if (image)
        reinterpret_cast<WpeEngine *>(data)->presentImage(image);
}

void WpeEngine::onExportShmBuffer(void *data, wpe_fdo_shm_exported_buffer *buffer)
{
    if (buffer)
        reinterpret_cast<WpeEngine *>(data)->discardShmBuffer(buffer);
}

void WpeEngine::onUriChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const char *value = webkit_web_view_get_uri(view);
    const QString updated = value ? QString::fromUtf8(value) : QString();
    if (updated != self->m_url) {
        self->m_url = updated;
        Q_EMIT self->urlChanged(self->m_url);
    }
}

void WpeEngine::onTitleChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const char *value = webkit_web_view_get_title(view);
    const QString updated = value ? QString::fromUtf8(value) : QString();
    if (updated != self->m_title) {
        self->m_title = updated;
        Q_EMIT self->titleChanged(self->m_title);
    }
}

void WpeEngine::onProgressChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const double progress = webkit_web_view_get_estimated_load_progress(view);
    int percent = static_cast<int>(std::lround(progress * 100.0));
    percent = std::max(0, std::min(100, percent));
    if (percent != self->m_progress) {
        self->m_progress = percent;
        Q_EMIT self->progressChanged(self->m_progress);
    }
}

void WpeEngine::onCanGoBackChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const bool updated = webkit_web_view_can_go_back(view);
    if (updated != self->m_canGoBack) {
        self->m_canGoBack = updated;
        Q_EMIT self->canGoBackChanged(self->m_canGoBack);
    }
}

void WpeEngine::onCanGoForwardChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const bool updated = webkit_web_view_can_go_forward(view);
    if (updated != self->m_canGoForward) {
        self->m_canGoForward = updated;
        Q_EMIT self->canGoForwardChanged(self->m_canGoForward);
    }
}

void WpeEngine::onLoadingChanged(WebKitWebView *view, GParamSpec *spec, void *data)
{
    Q_UNUSED(spec)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    const bool updated = webkit_web_view_is_loading(view);
    if (updated != self->m_isLoading) {
        self->m_isLoading = updated;
        if (!updated) {
            self->m_progress = 100;
            Q_EMIT self->progressChanged(100);
        }
        Q_EMIT self->isLoadingChanged(self->m_isLoading);
    }
}

void WpeEngine::onLoadChanged(WebKitWebView *view, WebKitLoadEvent loadEvent, void *data)
{
    Q_UNUSED(view)
    auto *self = reinterpret_cast<WpeEngine *>(data);
    if (loadEvent == WEBKIT_LOAD_STARTED && self->m_progress != 0) {
        self->m_progress = 0;
        Q_EMIT self->progressChanged(0);
    }
}

bool WpeEngine::onFullscreenHandler(void *data, bool enter)
{
    auto *self = reinterpret_cast<WpeEngine *>(data);
    Q_EMIT self->fullscreenRequested(enter);
    return TRUE;
}