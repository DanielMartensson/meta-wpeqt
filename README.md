# meta-wpeqt

A minimal **Qt6 + QML** web browser powered by the **WPE WebKit** engine,
delivered as a **Yocto/OpenEmbedded layer**.

WpeQt renders web content through an EGL-exportable WPE view backend and
presents it inside a Qt Quick scene graph item using a shared OpenGL ES
context. Designed for resource-constrained embedded targets (Watermelon Wine
1A / STM32MP257F), with GPU acceleration strictly required — no software
fallback is accepted.

## Table of Contents

- [Features](#features)
- [Screenshots](#screenshots)
- [Requirements](#requirements)
- [Layer structure](#layer-structure)
- [Build (Yocto)](#build-yocto)
- [Run](#run)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Features

- Back / forward navigation buttons
- Fullscreen toggle (press **F11** to exit fullscreen)
- URL address field (auto-prepends `https://`, or falls back to a Google search)
- Loading progress bar (0–100 %) with a percentage label
- DOM fullscreen forwarding (`onfullscreenchange` → window fullscreen)
- WebGL, HTML5 video (GStreamer), WebRTC enabled
- Hardware-accelerated rendering (EGL/GLES); SHM/software frames are rejected
- Configurable start page (CLI argument or environment variable)
- Lightweight: single local process, minimal Qt Quick chrome, no WebEngine

## Requirements

- Yocto **scarthgap** based distro with the following layers:
  - `meta-qt6` (Qt 6.8.x: `qtbase`, `qtdeclarative`)
  - `meta-watermelon-wine` (stable WPE stack: `wpewebkit` 2.52.6,
    `wpebackend-fdo` 1.16.1, `libwpe` 1.16.3)
  - `meta-openembedded` as required by the layers above
- Distro features: `wayland` and `opengl` (the `wpewebkit` recipe requires
  `opengl`; the browser requires a Wayland compositor such as Weston)
- GPU with EGL/GLES support (STM32MP257F GPU, or any host with Mesa)

### Note on WebGPU

WPE WebKit 2.52 does not expose a runtime WebGPU switch — it is a build-time
flag (`-DENABLE_WEBGPU=ON`). The `meta-watermelon-wine` `wpewebkit` build does
not enable it; everything else (WebGL, canvas, media, WebRTC) is available.

## Layer structure

```
meta-wpeqt/
├── conf/
│   └── layer.conf                     # layer registration
├── recipes-browser/
│   ├── packagegroups/
│   │   └── packagegroup-wpeqt.bb      # runtime packagegroup
│   └── wpeqt/
│       ├── wpeqt_1.0.0.bb             # browser recipe (CMake)
│       └── files/wpeqt/
│           ├── CMakeLists.txt         # Qt6 + WPE + EGL build
│           ├── LICENSE                # MIT license of WpeQt itself
│           ├── wpeqt.desktop          # launcher entry
│           ├── qml/
│           │   └── Main.qml           # browser chrome UI
│           └── src/
│               ├── main.cpp           # entry point, env hardening
│               ├── wpeengine.{h,cpp}  # WPE/WebKit integration
│               └── wpeviewitem.{h,cpp}# QQuickFramebufferObject item
└── recipes-core/
    └── images/
        └── wpeqt-image.bb             # ready-made bootable image
```

## Build (Yocto)

Add the layer to your build configuration:

```bash
# conf/bblayers.conf
BBLAYERS += " \
    /path/to/meta-wpeqt \
"
```

Build either the browser package or the full image:

```bash
bitbake wpeqt            # browser package only
bitbake wpeqt-image      # complete bootable image
```

Alternatively, install the packagegroup into an existing image with:

```bash
# conf/local.conf
CORE_IMAGE_EXTRA_INSTALL:append = " packagegroup-wpeqt"
```

## Run

On the target (or in a built test environment) with a Wayland compositor running:

```bash
wpeqt
```

The scene graph backend is pinned to OpenGL automatically on startup; an
explicit environment guard is also provided:

```bash
export QSG_RHI_BACKEND=opengl
wpeqt --url "https://www.google.com"
```

## Configuration

| Setting               | Means                                                              | Default                       |
| --------------------- | ------------------------------------------------------------------ | ----------------------------- |
| `WPEQT_URL` env var   | Initial URL when no argument is given                              | `https://www.google.com`      |
| `-u`, `--url <URL>`   | Initial URL CLI argument                                           | —                             |
| `QSG_RHI_BACKEND`     | Pinned to `opengl` (GPU) if unset                                  | (set on startup)              |
| `WEBKIT_DISABLE_DMABUF_RENDERER` | 0 = dmabuf compositing enabled (set if unset)      | (set on startup)              |
| `GST_GL_PLATFORM`     | Pinned to `egl` if unset                                           | (set on startup)              |
| `GST_GL_API`          | Pinned to `gles2` if unset                                         | (set on startup)              |

## Architecture

```
main.cpp ── QGuiApplication (Wayland window, QSG_RHI_BACKEND=opengl)
   │
   ├── WpeEngine (QObject, main thread)
   │     ├── Private QOpenGLContext shared with the scene graph context
   │     ├── wpe_view_backend_exportable_fdo_egl_create(...)
   │     ├── WebKitWebView attached to the exportable backend
   │     ├── GMainContext pump (1 ms QTimer)
   │     └── export callback → EGLImage → GL_TEXTURE_2D (GLES2)
   │
   ├── WpeViewItem ("WpeWebView" in QML) = QQuickFramebufferObject
   │     └── WpeRenderer draws the textured quad into the FBO
   │
   └── qml/Main.qml = chrome (back, forward, fullscreen, URL, progress)
```

Design notes:

- All WPE/WebKit calls run on the main thread (WPE is not thread-safe by
  design). Frame data crosses to the render thread under a mutex; textures are
  created in a context that shares with the scene graph.
- **No software rendering**: the backend is EGL-exportable; SHM frames are
  rejected with an explicit warning instead of being displayed.
- Keyboard/mouse/wheel input and visibility/focus state are translated into
  `wpe_view_backend_dispatch_*` events.

## Troubleshooting

| Symptom                     | Likely cause / fix                                                                 |
| --------------------------- | --------------------------------------------------------------------------------- |
| `rejecting SHM buffer`      | No GPU/EGL path available; WPE fell back to software. Ensure `wayland` + `opengl` distro features and the GBM/EGL drivers. |
| Black/frozen viewport       | `wpebackend-fdo` not in the image, or `glEGLImageTargetTexture2DOES` unavailable (check startup logs). |
| High CPU on target          | Some component ran on software (e.g. GStreamer without `egl/gles2`). Confirm env vars above and GPU driver presence. |
| No WebGPU API               | Build-time feature; not enabled in the `wpewebkit` 2.52.6 build (see note above).  |

## License

The layer and WpeQt sources are distributed under the **MIT** license.
See `recipes-browser/wpeqt/files/wpeqt/LICENSE`. The WPE stack itself
(`wpewebkit`, `wpebackend-fdo`, `libwpe`) is governed by its own licenses in
`meta-watermelon-wine`.