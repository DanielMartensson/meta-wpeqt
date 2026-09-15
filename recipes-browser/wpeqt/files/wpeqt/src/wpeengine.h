#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QString>
#include <mutex>

#include <wpe/fdo-egl.h>
#include <wpe/fdo.h>
#include <wpe/webkit.h>
#include <wpe/wpe.h>

class QOffscreenSurface;
class QOpenGLContext;
class QQuickWindow;
class QTimer;

class WpeEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

public:
    struct FrameInfo
    {
        quint32 textureId = 0;
        int width = 0;
        int height = 0;
    };

    explicit WpeEngine(QObject *parent = nullptr);
    ~WpeEngine() override;

    void setInitialUrl(const QString &url) { m_initialUrl = url; }
    QString initialUrl() const { return m_initialUrl; }

    void attachToWindow(QQuickWindow *window);
    bool isReady() const { return m_webView != nullptr; }

    int progress() const { return m_progress; }
    QString url() const { return m_url; }
    QString title() const { return m_title; }
    bool canGoBack() const { return m_canGoBack; }
    bool canGoForward() const { return m_canGoForward; }
    bool isLoading() const { return m_isLoading; }

    void loadUrl(const QString &url);
    void goBack();
    void goForward();
    void reload();
    void stop();

    void setViewSize(int width, int height);
    void setScaleFactor(qreal scale);
    void notifyActivity(bool visible, bool focused, bool windowed);

    void dispatchMouse(int type, int x, int y, int button, bool pressed, quint32 buttons, quint32 modifiers);
    void dispatchAxis(int x, int y, double dx, double dy, quint32 modifiers);
    void dispatchKey(quint32 keyCode, quint32 hardwareKeyCode, bool pressed, quint32 modifiers);

    bool takeFrame(FrameInfo *frame);
    void acceptFullscreen(bool enter);

Q_SIGNALS:
    void progressChanged(int progress);
    void urlChanged(const QString &url);
    void titleChanged(const QString &title);
    void canGoBackChanged(bool canGoBack);
    void canGoForwardChanged(bool canGoForward);
    void isLoadingChanged(bool loading);
    void frameReady();
    void fullscreenRequested(bool enter);

private:
    void tryInitialize();
    void initialize();
    void applySettings();
    void connectWebSignals();
    void pumpGlib();
    quint32 eventTime();
    void presentImage(wpe_fdo_egl_exported_image *image);
    void discardShmBuffer(wpe_fdo_shm_exported_buffer *buffer);

    static void onExportEglImage(void *data, EGLImageKHR image);
    static void onExportFdoEglImage(void *data, wpe_fdo_egl_exported_image *image);
    static void onExportShmBuffer(void *data, wpe_fdo_shm_exported_buffer *buffer);
    static void onUriChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onTitleChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onProgressChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onCanGoBackChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onCanGoForwardChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onLoadingChanged(WebKitWebView *view, GParamSpec *spec, void *data);
    static void onLoadChanged(WebKitWebView *view, WebKitLoadEvent loadEvent, void *data);
    static bool onFullscreenHandler(void *data, bool enter);

    QQuickWindow *m_window = nullptr;
    QOpenGLContext *m_glContext = nullptr;
    QOffscreenSurface *m_offscreenSurface = nullptr;
    QTimer *m_pumpTimer = nullptr;
    QElapsedTimer m_clock;

    void (*m_eglImageTarget)(unsigned int target, void *image) = nullptr;

    GMainContext *m_glibContext = nullptr;
    wpe_view_backend_exportable_fdo *m_exportable = nullptr;
    wpe_view_backend *m_viewBackend = nullptr;
    WebKitWebView *m_webView = nullptr;
    WebKitWebViewBackend *m_wkBackend = nullptr;

    wpe_fdo_egl_exported_image *m_exportedImage = nullptr;
    quint32 m_textureId = 0;
    QSize m_textureSize;

    int m_viewWidth = 1;
    int m_viewHeight = 1;
    qreal m_scaleFactor = 1.0;
    bool m_pendingVisible = true;
    bool m_pendingFocused = true;
    bool m_pendingWindowed = true;

    QString m_initialUrl;
    QString m_pendingUrl;
    QString m_url = QStringLiteral("about:blank");
    QString m_title;
    int m_progress = 0;
    bool m_canGoBack = false;
    bool m_canGoForward = false;
    bool m_isLoading = false;

    std::mutex m_frameMutex;
    FrameInfo m_frame;
    bool m_frameChanged = false;

    static const wpe_view_backend_exportable_fdo_egl_client s_exportClient;
};