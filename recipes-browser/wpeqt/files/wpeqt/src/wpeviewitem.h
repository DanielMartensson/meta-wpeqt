#pragma once

#include <QQuickFramebufferObject>

#include "wpeengine.h"

class WpeRenderer;

class WpeViewItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(WpeEngine *engine READ engine WRITE setEngine)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

public:
    explicit WpeViewItem();

    WpeEngine *engine() const { return m_engine; }
    void setEngine(WpeEngine *engine);

    int progress() const;
    QString url() const;
    QString title() const;
    bool canGoBack() const;
    bool canGoForward() const;
    bool isLoading() const;

    Q_INVOKABLE void loadUrl(const QString &url);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void confirmDomFullscreen(bool enter);

    Renderer *createRenderer() const override;

Q_SIGNALS:
    void progressChanged(int progress);
    void urlChanged(const QString &url);
    void titleChanged(const QString &title);
    void canGoBackChanged(bool canGoBack);
    void canGoForwardChanged(bool canGoForward);
    void isLoadingChanged(bool loading);
    void fullscreenRequested(bool enter);

protected:
    void geometryChange(const QRectF &newGeometry,
                        const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void syncActivity();
    WpeEngine *m_engine = nullptr;
    quint32 m_pressedButtons = 0;

    friend class WpeRenderer;
};

class WpeRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit WpeRenderer();
    void synchronize(QQuickFramebufferObject *item) override;
    void render() override;

private:
    void ensureProgram();
    void drawTexturedQuad(const WpeEngine::FrameInfo &frame);

    WpeEngine::FrameInfo m_frame;
    quint32 m_program = 0;
};