#include "wpeviewitem.h"

#include <QFocusEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QWheelEvent>

namespace {

constexpr int kPointerMotion = wpe_input_pointer_event_type_motion;
constexpr int kPointerButton = wpe_input_pointer_event_type_button;

quint32 wpeModifiersFromQt(Qt::KeyboardModifiers modifiers)
{
    quint32 result = 0;
    if (modifiers.testFlag(Qt::ControlModifier))
        result |= wpe_input_keyboard_modifier_control;
    if (modifiers.testFlag(Qt::ShiftModifier))
        result |= wpe_input_keyboard_modifier_shift;
    if (modifiers.testFlag(Qt::AltModifier))
        result |= wpe_input_keyboard_modifier_alt;
    if (modifiers.testFlag(Qt::MetaModifier))
        result |= wpe_input_keyboard_modifier_meta;
    return result;
}

quint32 wpeButtonsFromQt(Qt::MouseButtons buttons)
{
    quint32 result = 0;
    if (buttons.testFlag(Qt::LeftButton))
        result |= wpe_input_pointer_modifier_button1;
    if (buttons.testFlag(Qt::RightButton))
        result |= wpe_input_pointer_modifier_button2;
    if (buttons.testFlag(Qt::MiddleButton))
        result |= wpe_input_pointer_modifier_button3;
    return result;
}

unsigned int wpeButtonFromQt(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
        return 1;
    case Qt::RightButton:
        return 2;
    case Qt::MiddleButton:
        return 3;
    default:
        return 0;
    }
}

unsigned int wpeKeysymFromKey(int key)
{
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 0xff0d;
    case Qt::Key_Escape:
        return 0xff1b;
    case Qt::Key_Tab:
        return 0xff09;
    case Qt::Key_Backtab:
        return 0xff89;
    case Qt::Key_Backspace:
        return 0xff08;
    case Qt::Key_Delete:
        return 0xffff;
    case Qt::Key_Insert:
        return 0xff63;
    case Qt::Key_Home:
        return 0xff50;
    case Qt::Key_End:
        return 0xff57;
    case Qt::Key_Left:
        return 0xff51;
    case Qt::Key_Up:
        return 0xff52;
    case Qt::Key_Right:
        return 0xff53;
    case Qt::Key_Down:
        return 0xff54;
    case Qt::Key_PageUp:
        return 0xff55;
    case Qt::Key_PageDown:
        return 0xff56;
    case Qt::Key_Shift:
        return 0xffe1;
    case Qt::Key_Control:
        return 0xffe3;
    case Qt::Key_Alt:
        return 0xffe9;
    case Qt::Key_Meta:
        return 0xffed;
    default:
        break;
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return static_cast<unsigned int>(0xffbe + (key - Qt::Key_F1));

    if (key >= 0x20 && key <= 0x7e)
        return static_cast<unsigned int>(key);

    return 0;
}

} // namespace

WpeViewItem::WpeViewItem()
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setActiveFocusOnTab(true);
}

void WpeViewItem::setEngine(WpeEngine *engine)
{
    if (m_engine == engine)
        return;

    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
    }

    m_engine = engine;

    if (m_engine) {
        connect(m_engine, &WpeEngine::progressChanged,
                this, &WpeViewItem::progressChanged);
        connect(m_engine, &WpeEngine::urlChanged,
                this, &WpeViewItem::urlChanged);
        connect(m_engine, &WpeEngine::titleChanged,
                this, &WpeViewItem::titleChanged);
        connect(m_engine, &WpeEngine::canGoBackChanged,
                this, &WpeViewItem::canGoBackChanged);
        connect(m_engine, &WpeEngine::canGoForwardChanged,
                this, &WpeViewItem::canGoForwardChanged);
        connect(m_engine, &WpeEngine::isLoadingChanged,
                this, &WpeViewItem::isLoadingChanged);
        connect(m_engine, &WpeEngine::fullscreenRequested,
                this, &WpeViewItem::fullscreenRequested);
        connect(m_engine, &WpeEngine::frameReady, this, [this] {
            if (window())
                update();
        });

        m_engine->setViewSize(
            static_cast<int>(width() * (window() ? window()->devicePixelRatio() : 1.0)),
            static_cast<int>(height() * (window() ? window()->devicePixelRatio() : 1.0)));
        if (window())
            m_engine->setScaleFactor(window()->devicePixelRatio());
        update();
    }
}

int WpeViewItem::progress() const
{
    return m_engine ? m_engine->progress() : 0;
}

QString WpeViewItem::url() const
{
    return m_engine ? m_engine->url() : QString();
}

QString WpeViewItem::title() const
{
    return m_engine ? m_engine->title() : QString();
}

bool WpeViewItem::canGoBack() const
{
    return m_engine ? m_engine->canGoBack() : false;
}

bool WpeViewItem::canGoForward() const
{
    return m_engine ? m_engine->canGoForward() : false;
}

bool WpeViewItem::isLoading() const
{
    return m_engine ? m_engine->isLoading() : false;
}

void WpeViewItem::loadUrl(const QString &url)
{
    if (m_engine)
        m_engine->loadUrl(url);
}

void WpeViewItem::goBack()
{
    if (m_engine)
        m_engine->goBack();
}

void WpeViewItem::goForward()
{
    if (m_engine)
        m_engine->goForward();
}

void WpeViewItem::reload()
{
    if (m_engine)
        m_engine->reload();
}

void WpeViewItem::stop()
{
    if (m_engine)
        m_engine->stop();
}

void WpeViewItem::confirmDomFullscreen(bool enter)
{
    if (m_engine)
        m_engine->acceptFullscreen(enter);
}

QQuickFramebufferObject::Renderer *WpeViewItem::createRenderer() const
{
    return new WpeRenderer();
}

void WpeViewItem::geometryChange(const QRectF &newGeometry,
                                 const QRectF &oldGeometry)
{
    QQuickFramebufferObject::geometryChange(newGeometry, oldGeometry);
    if (m_engine && window()) {
        const qreal scale = window()->devicePixelRatio();
        m_engine->setViewSize(static_cast<int>(newGeometry.width() * scale),
                              static_cast<int>(newGeometry.height() * scale));
        m_engine->setScaleFactor(scale);
    }
}

void WpeViewItem::mousePressEvent(QMouseEvent *event)
{
    if (m_engine) {
        const unsigned int button = wpeButtonFromQt(event->button());
        m_pressedButtons |= wpeButtonsFromQt(event->buttons());
        m_engine->dispatchMouse(kPointerButton,
                                static_cast<int>(event->position().x()),
                                static_cast<int>(event->position().y()),
                                button, true, m_pressedButtons,
                                wpeModifiersFromQt(event->modifiers()));
        forceActiveFocus();
    }
    event->accept();
    update();
}

void WpeViewItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_engine) {
        const unsigned int button = wpeButtonFromQt(event->button());
        m_pressedButtons &= ~wpeButtonsFromQt({ event->button() });
        m_engine->dispatchMouse(kPointerButton,
                                static_cast<int>(event->position().x()),
                                static_cast<int>(event->position().y()),
                                button, false, m_pressedButtons,
                                wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
    update();
}

void WpeViewItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_engine) {
        m_engine->dispatchMouse(kPointerMotion,
                                static_cast<int>(event->position().x()),
                                static_cast<int>(event->position().y()),
                                0, false, m_pressedButtons,
                                wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
}

void WpeViewItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_engine && !(m_pressedButtons & wpe_input_pointer_modifier_button1)) {
        m_engine->dispatchMouse(kPointerMotion,
                                static_cast<int>(event->position().x()),
                                static_cast<int>(event->position().y()),
                                0, false, 0,
                                wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
}

void WpeViewItem::wheelEvent(QWheelEvent *event)
{
    if (m_engine) {
        double dx = event->pixelDelta().x();
        double dy = event->pixelDelta().y();
        if (dx == 0.0 && dy == 0.0) {
            dx = event->angleDelta().x() / 8.0;
            dy = event->angleDelta().y() / 8.0;
        }
        m_engine->dispatchAxis(static_cast<int>(event->position().x()),
                               static_cast<int>(event->position().y()),
                               dx, dy,
                               wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
}

void WpeViewItem::keyPressEvent(QKeyEvent *event)
{
    if (m_engine) {
        unsigned int keyCode = wpeKeysymFromKey(event->key());
        const QString text = event->text();
        if (keyCode == 0 && !text.isEmpty() && text.at(0).isPrint()) {
            keyCode = wpe_unicode_to_key_code(text.at(0).unicode());
        }
        m_engine->dispatchKey(keyCode,
                              static_cast<unsigned int>(event->nativeVirtualKey()),
                              true,
                              wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
}

void WpeViewItem::keyReleaseEvent(QKeyEvent *event)
{
    if (m_engine) {
        unsigned int keyCode = wpeKeysymFromKey(event->key());
        const QString text = event->text();
        if (keyCode == 0 && !text.isEmpty() && text.at(0).isPrint()) {
            keyCode = wpe_unicode_to_key_code(text.at(0).unicode());
        }
        m_engine->dispatchKey(keyCode,
                              static_cast<unsigned int>(event->nativeVirtualKey()),
                              false,
                              wpeModifiersFromQt(event->modifiers()));
    }
    event->accept();
}

void WpeViewItem::focusInEvent(QFocusEvent *event)
{
    QQuickFramebufferObject::focusInEvent(event);
    syncActivity();
}

void WpeViewItem::focusOutEvent(QFocusEvent *event)
{
    QQuickFramebufferObject::focusOutEvent(event);
    syncActivity();
}

void WpeViewItem::syncActivity()
{
    if (m_engine)
        m_engine->notifyActivity(isVisible(), hasActiveFocus(),
                                 window() ? !(window()->windowState() == Qt::WindowFullScreen)
                                          : true);
}

WpeRenderer::WpeRenderer()
{
}

void WpeRenderer::synchronize(QQuickFramebufferObject *item)
{
    auto *view = static_cast<WpeViewItem *>(item);
    if (!view || !view->engine())
        return;
    WpeEngine::FrameInfo frame;
    view->engine()->takeFrame(&frame);
    if (frame.textureId != 0) {
        m_frame = frame;
    }
}

void WpeRenderer::render()
{
    QOpenGLFramebufferObject *target = framebufferObject();
    if (!target)
        return;

    QOpenGLFunctions gl(QOpenGLContext::currentContext());
    gl.glViewport(0, 0, target->width(), target->height());
    gl.glClearColor(0.06f, 0.07f, 0.08f, 1.0f);
    gl.glClear(GL_COLOR_BUFFER_BIT);

    if (m_frame.textureId == 0)
        return;

    ensureProgram();
    if (m_program == 0)
        return;

    drawTexturedQuad(m_frame);

    gl.glBindTexture(GL_TEXTURE_2D, 0);
    gl.glUseProgram(0);
}

void WpeRenderer::ensureProgram()
{
    if (m_program != 0)
        return;

    QOpenGLFunctions gl(QOpenGLContext::currentContext());

    const char *vertexSource =
        "attribute vec2 aPos;"
        "attribute vec2 aUv;"
        "varying vec2 vUv;"
        "void main() {"
        "  gl_Position = vec4(aPos, 0.0, 1.0);"
        "  vUv = aUv;"
        "}";

    const char *fragmentSource =
        "precision mediump float;"
        "varying vec2 vUv;"
        "uniform sampler2D uTex;"
        "void main() {"
        "  gl_FragColor = texture2D(uTex, vUv);"
        "}";

    auto compile = [&gl](GLenum type, const char *source) {
        GLuint shader = gl.glCreateShader(type);
        gl.glShaderSource(shader, 1, &source, nullptr);
        gl.glCompileShader(shader);
        GLint status = GL_FALSE;
        gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            char log[512] = {};
            gl.glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            qWarning("WpeRenderer: shader compile failed: %s", log);
        }
        return shader;
    };

    GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);

    m_program = gl.glCreateProgram();
    gl.glAttachShader(m_program, vertex);
    gl.glAttachShader(m_program, fragment);
    gl.glLinkProgram(m_program);

    GLint linked = GL_FALSE;
    gl.glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512] = {};
        gl.glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        qWarning("WpeRenderer: program link failed: %s", log);
        gl.glDeleteProgram(m_program);
        m_program = 0;
    }

    gl.glDeleteShader(vertex);
    gl.glDeleteShader(fragment);
}

void WpeRenderer::drawTexturedQuad(const WpeEngine::FrameInfo &frame)
{
    QOpenGLFunctions gl(QOpenGLContext::currentContext());

    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, frame.textureId);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl.glUseProgram(m_program);
    const GLint frameUniform = gl.glGetUniformLocation(m_program, "uTex");
    const GLint positionAttrib = gl.glGetAttribLocation(m_program, "aPos");
    const GLint uvAttrib = gl.glGetAttribLocation(m_program, "aUv");

    gl.glUniform1i(frameUniform, 0);

    const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };

    gl.glEnableVertexAttribArray(static_cast<GLuint>(positionAttrib));
    gl.glEnableVertexAttribArray(static_cast<GLuint>(uvAttrib));
    gl.glVertexAttribPointer(static_cast<GLuint>(positionAttrib), 2, GL_FLOAT, GL_FALSE,
                             4 * sizeof(float), vertices);
    gl.glVertexAttribPointer(static_cast<GLuint>(uvAttrib), 2, GL_FLOAT, GL_FALSE,
                             4 * sizeof(float), vertices + 2);
    gl.glDrawArrays(GL_TRIANGLES, 0, 6);
    gl.glDisableVertexAttribArray(static_cast<GLuint>(uvAttrib));
    gl.glDisableVertexAttribArray(static_cast<GLuint>(positionAttrib));
}