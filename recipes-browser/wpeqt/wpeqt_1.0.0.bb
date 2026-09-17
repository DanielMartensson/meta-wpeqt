SUMMARY = "Minimal Qt6 QML browser built on the WPE WebKit engine"
DESCRIPTION = "WpeQt renders web content through WPE WebKit with a GLES/EGL \
exportable backend and presents it inside a Qt Quick scene graph item. The \
chrome is a small Qt Quick toolbar (back, forward, fullscreen, URL field, \
progress bar). GPU acceleration is mandatory; any software (SHM) fallback is \
rejected at runtime."
HOMEPAGE = "https://github.com/DanielMartensson/meta-wpeqt"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=6c7bd58213aecdf2fd702be55f876f47"

inherit qt6-cmake pkgconfig features_check

REQUIRED_DISTRO_FEATURES = "wayland opengl"

DEPENDS += " \
    qtbase \
    qtdeclarative \
    wpewebkit \
    wpebackend-fdo \
    libwpe \
    virtual/egl \
"

EXTRA_OECMAKE:append = " -DCMAKE_BUILD_TYPE=Release"

RDEPENDS:${PN} += " \
    wpewebkit \
    wpebackend-fdo \
    libwpe \
    qtdeclarative-qmlplugins \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
"

FILES:${PN} += "${datadir}/applications/wpeqt.desktop"

SRC_URI = "file://wpeqt"
S = "${WORKDIR}/wpeqt"