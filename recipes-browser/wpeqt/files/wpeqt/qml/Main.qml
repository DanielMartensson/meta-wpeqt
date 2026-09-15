import QtQuick
import QtQuick.Controls
import WpeQt

component ChromeButton: Rectangle {
    id: button
    property string label: ""
    property bool active: true
    signal clicked()
    width: 36
    height: 32
    radius: 6
    color: area.pressed ? "#3c4043" : (area.hovered && active ? "#2f3033" : "transparent")
    Text {
        text: button.label
        anchors.centerIn: parent
        color: button.active ? "#e8eaed" : "#5f6368"
        font.pixelSize: 18
    }
    MouseArea {
        id: area
        anchors.fill: parent
        enabled: button.active
        hoverEnabled: true
        onClicked: button.clicked()
    }
}

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    minimumWidth: 640
    minimumHeight: 480
    color: "#101014"
    title: webView.title.length > 0 ? webView.title + " — WpeQt" : "WpeQt"

    readonly property int chromeHeight: 48
    readonly property int progressHeight: 3

    function navigat(address) {
        var target = address.trim()
        if (target.length === 0)
            return
        if (!/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(target)) {
            if (target.indexOf(".") >= 0 || target.indexOf("localhost") === 0) {
                target = "https://" + target
            } else {
                target = "https://www.google.com/search?q=" + encodeURIComponent(target)
            }
        }
        if (urlField.activeFocus)
            urlField.text = target
        webView.loadUrl(target)
    }

    function toggleFullscreen() {
        root.visibility = (root.visibility === Window.FullScreen)
                ? Window.Windowed
                : Window.FullScreen
    }

    function enterFullscreen(enter) {
        root.visibility = enter ? Window.FullScreen : Window.Windowed
        webView.confirmDomFullscreen(enter)
    }

    WpeWebView {
        id: webView
        engine: wpe
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressStrip.bottom
        anchors.bottom: parent.bottom
        focus: true
        onFullscreenRequested: function (enter) {
            root.enterFullscreen(enter)
        }
    }

    Rectangle {
        id: chrome
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.chromeHeight
        color: "#1e1f24"

        Row {
            id: controls
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            ChromeButton {
                id: backButton
                label: "\u2190"
                active: webView.canGoBack
                onClicked: webView.goBack()
            }

            ChromeButton {
                id: forwardButton
                label: "\u2192"
                active: webView.canGoForward
                onClicked: webView.goForward()
            }

            ChromeButton {
                id: fullscreenButton
                label: "\u26F6"
                onClicked: root.toggleFullscreen()
            }

            TextField {
                id: urlField
                height: 32
                width: Math.max(240, controls.width - backButton.width - forwardButton.width
                                - fullscreenButton.width - statusLabel.width - 8 * 6)
                anchors.verticalCenter: parent.verticalCenter
                placeholderText: "https://"
                placeholderTextColor: "#9aa0a6"
                color: "#e8eaed"
                selectByMouse: true
                font.pixelSize: 14
                background: Rectangle {
                    radius: 6
                    border.color: urlField.activeFocus ? "#8ab4f8" : "#3c4043"
                    border.width: 1
                    color: "#282a2e"
                }
                onAccepted: root.navigat(text)
            }

            Binding {
                target: urlField
                property: "text"
                value: webView.url
                when: !urlField.activeFocus
            }

            Text {
                id: statusLabel
                anchors.verticalCenter: parent.verticalCenter
                visible: webView.isLoading
                text: webView.progress + " %"
                color: "#9aa0a6"
                font.pixelSize: 12
            }
        }
    }

    Rectangle {
        id: progressStrip
        anchors.top: chrome.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.progressHeight
        color: "#1e1f24"
        visible: webView.isLoading

        Rectangle {
            id: progressFill
            height: parent.height
            width: parent.width * Math.min(1.0, webView.progress / 100.0)
            color: "#8ab4f8"
        }
    }

    Shortcut {
        sequence: "F11"
        onActivated: root.toggleFullscreen()
    }
}