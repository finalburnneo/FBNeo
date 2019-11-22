/**
** Copyright (C) 2019 Akop Karapetyan
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**/

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <stdio.h>

#include "pigl.h"

uint32_t pigl_screen_width = 0;
uint32_t pigl_screen_height = 0;

static int device = -1;
static uint32_t connector_id;
static struct gbm_device *gbm_device = NULL;
static struct gbm_surface *gbm_surface = NULL;
static EGLContext context = EGL_NO_CONTEXT;
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface egl_surface = EGL_NO_SURFACE;

static drmModeModeInfo mode_info;
static drmModeCrtc *crtc = NULL;

static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb;

static drmModeConnector *find_connector(drmModeRes *resources)
{
    int i;
    for (i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *connector = drmModeGetConnector(device, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED)
            return connector;
        drmModeFreeConnector(connector);
    }

    return NULL;
}

static drmModeEncoder *find_encoder (drmModeRes *resources, drmModeConnector *connector)
{
    if (connector->encoder_id)
        return drmModeGetEncoder (device, connector->encoder_id);
    return NULL;
}

static int find_display_configuration()
{
    drmModeRes *resources = drmModeGetResources(device);
    if (!resources) {
        fprintf(stderr, "drmModeGetResources returned NULL\n");
        return 0;
    }

    drmModeConnector *connector = find_connector(resources);
    if (!connector) {
        fprintf(stderr, "find_connector returned NULL\n");
        drmModeFreeResources(resources);
        return 0;
    }

    // save the connector_id
    connector_id = connector->connector_id;
    // save the first mode
    mode_info = connector->modes[0];

    pigl_screen_width = mode_info.hdisplay;
    pigl_screen_height = mode_info.vdisplay;

    fprintf(stderr, "Hardware resolution: %dx%d\n",
        pigl_screen_width, pigl_screen_height);

    // find an encoder
    drmModeEncoder *encoder = find_encoder(resources, connector);
    if (!encoder) {
        fprintf(stderr, "find_encoder returned NULL\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        return 0;
    }

    // find a CRTC
    if (encoder->crtc_id)
        crtc = drmModeGetCrtc(device, encoder->crtc_id);

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    return 1;
}

static int match_config_to_visual(EGLDisplay egl_display,
    EGLint visual_id, EGLConfig* configs, int count)
{
    EGLint id;
    for (int i = 0; i < count; ++i)
        if (eglGetConfigAttrib(egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &id)
            && id == visual_id) return i;

    return -1;
}

int pigl_init()
{
    printf("Initializing DRM video\n");

    device = open("/dev/dri/card1", O_RDWR);
    if (device == -1) {
        fprintf(stderr, "Error opening device\n");
        return 0;
    }

    if (!find_display_configuration())
        return 0;

    gbm_device = gbm_create_device(device);
    gbm_surface = gbm_surface_create(gbm_device,
        pigl_screen_width, pigl_screen_height,
        GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);

    display = eglGetDisplay(gbm_device);
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay() failed: 0x%x\n", eglGetError());
        return 0;
    }

    if (eglInitialize(display, NULL, NULL) == EGL_FALSE) {
        fprintf(stderr, "eglInitialize() failed: 0x%x\n", eglGetError());
        return 0;
    }

    // create an OpenGL context
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        fprintf(stderr, "eglBindAPI() failed: 0x%x\n", eglGetError());
        return 0;
    }

    // get an appropriate EGL frame buffer configuration
    EGLint count = 0;
    if (eglGetConfigs(display, NULL, 0, &count) == EGL_FALSE) {
        fprintf(stderr, "eglGetConfigs() failed: 0x%x\n", eglGetError());
        return 0;
    }

    EGLConfig *configs = malloc(count * sizeof *configs);
    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE };
    EGLint num_config;
    if (eglChooseConfig(display, attributes, configs, count, &num_config) == EGL_FALSE) {
        free(configs);
        fprintf(stderr, "eglChooseConfig() failed: 0x%x\n", eglGetError());
        return 0;
    }

    int config_index = match_config_to_visual(display, GBM_FORMAT_XRGB8888,
        configs, num_config);
    if (config_index == -1) {
        free(configs);
        fprintf(stderr, "No suitable config match\n");
        return 0;
    }

    // create an EGL rendering context
    EGLConfig *config = configs[config_index];
    static const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT) {
        free(configs);
        fprintf(stderr, "eglCreateContext() failed: EGL_NO_CONTEXT\n");
        return 0;
    }

    egl_surface = eglCreateWindowSurface(display, config, gbm_surface, NULL);
    free(configs);
    if (egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreateWindowSurface() failed: EGL_NO_SURFACE\n");
        return 0;
    }

    if (eglMakeCurrent(display, egl_surface, egl_surface, context) == EGL_FALSE) {
        fprintf(stderr, "eglMakeCurrent() failed: 0x%x\n", eglGetError());
        return 0;
    }

    fprintf(stderr, "Using renderer: %s\n", glGetString(GL_RENDERER));

    return 1;
}

void pigl_shutdown()
{
    // set the previous crtc
    if (crtc != NULL) {
        drmModeSetCrtc(device, crtc->crtc_id, crtc->buffer_id,
            crtc->x, crtc->y, &connector_id, 1, &crtc->mode);
        drmModeFreeCrtc(crtc);
    }

    if (previous_bo) {
        drmModeRmFB(device, previous_fb);
        gbm_surface_release_buffer(gbm_surface, previous_bo);
    }
    if (egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, egl_surface);
        egl_surface = EGL_NO_SURFACE;
    }
    if (gbm_surface) {
        gbm_surface_destroy(gbm_surface);
        gbm_surface = NULL;
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    if (display != EGL_NO_DISPLAY) {
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }
    if (gbm_device) {
        gbm_device_destroy(gbm_device);
        gbm_device = NULL;
    }
    if (device != -1) {
        close(device);
        device = -1;
    }
}

void pigl_swap()
{
    eglSwapBuffers(display, egl_surface);
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm_surface);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t pitch = gbm_bo_get_stride(bo);
    uint32_t fb;
    drmModeAddFB(device, mode_info.hdisplay, mode_info.vdisplay, 24, 32, pitch, handle, &fb);
    drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);

    if (previous_bo) {
        drmModeRmFB(device, previous_fb);
        gbm_surface_release_buffer(gbm_surface, previous_bo);
    }

    previous_bo = bo;
    previous_fb = fb;
}
