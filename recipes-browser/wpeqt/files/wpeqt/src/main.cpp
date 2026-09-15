#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

#include <wpe/wpe.h>

#include "wpeengine.h"
#include "wpeviewitem.h"

#include <cstdio>

namespace {

QString initialUrlFromArguments(const QStringList &arguments)
{
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        if (argument == QStringLiteral("--url") || argument == QStringLiteral("-u")) {
            if (i + 1 < arguments.size())
                return arguments.at(i + 1);
        } else if (argument.startsWith(QStringLiteral("--url="))) {
            return argument.mid(6);
        }
    }

    const QByteArray environmentUrl = qgetenv("WPEQT_URL");
    if (!environmentUrl.isEmpty())
        return QString::fromLocal8Bit(environmentUrl);

    return QStringLiteral("https://www.google.com");
}

void hardenEnvironment()
{
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");
    if (qEnvironmentVariableIsEmpty("WEBKIT_DISABLE_DMABUF_RENDERER"))
        qputenv("WEBKIT_DISABLE_DMABUF_RENDERER", "0");
    if (qEnvironmentVariableIsEmpty("GST_GL_PLATFORM"))
        qputenv("GST_GL_PLATFORM", "egl");
    if (qEnvironmentVariableIsEmpty("GST_GL_API"))
        qputenv("GST_GL_API", "gles2");
}

} // namespace

int main(int argc, char *argv[])
{
    hardenEnvironment();

    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("WpeQt"));
    application.setApplicationVersion(QStringLiteral("1.0.0"));
    application.setOrganizationName(QStringLiteral("WpeQt"));

    qmlRegisterType<WpeViewItem>("WpeQt", 1, 0, "WpeWebView");

    if (!wpe_loader_init("libWPEBackend-fdo-1.0.so"))
        qWarning("WpeQt: wpe_loader_init could not open libWPEBackend-fdo-1.0.so");

    WpeEngine engine;
    engine.setInitialUrl(initialUrlFromArguments(application.arguments()));

    QQmlApplicationEngine qmlEngine;
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("wpe"), &engine);
    qmlEngine.load(QUrl(QStringLiteral("qrc:/qt/qml/WpeQt/Main.qml")));

    const auto roots = qmlEngine.rootObjects();
    if (roots.isEmpty()) {
        qCritical("WpeQt: failed to load the Main QML module");
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(roots.first());
    if (window)
        engine.attachToWindow(window);

    return application.exec();
}