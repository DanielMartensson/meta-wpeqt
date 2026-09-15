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

The layer is **self-contained for the entire WPE stack** — `libwpe`,
`wpebackend-fdo` and `wpewebkit` recipes are all shipped by this layer — and
depends only on well-known public layers.

### Meta-layer dependencies

| Meta-layer | Yocto collection | Provides |
|---|---|---|
| openembedded-core | `core` | `virtual/egl`, `libdrm`, `libxkbcommon`, `wayland`, `vulkan-headers`/`vulkan-loader`/`vulkan-volk`, `gstreamer1.0`, all `-plugins-*` |
| meta-openembedded | `openembedded-layer` | `libjxl`, `libbacktrace`, `lcms`, `libepoxy` |
| meta-openembedded | `multimedia-layer` | `libavif` |
| meta-openembedded | `meta-python` | pulled in by `qt6-layer` |
| meta-qt6 | `qt6-layer` | Qt 6.8 (`qtbase`, `qtdeclarative`) |

### Required distro features

- `wayland` — Wayland compositor (e.g. Weston)
- `opengl` — EGL/GLES stack from the BSP

### Implicit runtime dependencies

These are not declared as explicit DEPENDS but are required for correct
operation on target:

| Package | Purpose |
|---|---|
| `libxkbcommon` | XKB keymap processing (keyboard scancode → keysym translation) for WPE + Wayland + Qt |
| `virtual/egl` | EGL display / context for GPU compositing |
| `vulkan-loader` | WPE WebKit builds with `USE_VULKAN=ON`; loader is recommended at runtime |
| `ca-certificates` | HTTPS support (WPE WebKit RRECOMMENDS) |
| Wayland compositor | WpeQt renders into a Wayland surface; a compositor (Weston etc.) must be running |

Yocto release: **scarthgap**.

Distro features required: `wayland` and `opengl` (the `wpewebkit` recipe
enforces `opengl`; the browser needs a Wayland compositor such as Weston).
Target hardware: any GPU with EGL/GLES support (e.g. the STM32MP257F used by
Watermelon Wine 1A). No proprietary or private layer is required.

### Note on WebGPU

WPE WebKit 2.52 does not expose a runtime WebGPU switch — it is a build-time
flag (`-DENABLE_WEBGPU=ON`). The `wpewebkit` recipe shipped in this layer does
not enable it; everything else (WebGL, canvas, media, WebRTC) is available.

## Layer structure

```
meta-wpeqt/
├── conf/
│   └── layer.conf                     # layer registration
├── recipes-browser/
│   ├── libwpe/
│   │   └── libwpe_1.16.3.bb           # WPE core library (BSD-2-Clause)
│   ├── wpebackend-fdo/
│   │   ├── wpebackend-fdo_1.16.1.bb   # freedesktop.org WPE backend
│   │   └── wpebackend-fdo_%.bbappend  # runtime dlopen() symlink for WpeQt
│   ├── wpewebkit/
│   │   ├── wpewebkit_2.52.6.bb        # WPE WebKit engine
│   │   └── files/                     # bwrap + libsoup MIME patches
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

The layer resolves its own `libwpe`, `wpebackend-fdo` and `wpewebkit`
recipes, so no extra WPE layer is needed.

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
See `recipes-browser/wpeqt/files/wpeqt/LICENSE`.

| Component            | License             | Where in the layer                          |
| -------------------- | ------------------- | -------------------------------------------- |
| WpeQt (app, recipes) | MIT                 | `recipes-browser/wpeqt/`                     |
| wpewebkit            | BSD-2-Clause / LGPL | `recipes-browser/wpewebkit/`                 |
| wpebackend-fdo       | BSD-2-Clause        | `recipes-browser/wpebackend-fdo/`            |
| libwpe               | BSD-2-Clause        | `recipes-browser/libwpe/`                    |