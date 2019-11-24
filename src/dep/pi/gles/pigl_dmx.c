/**
** Copyright (C) 2015-2019 Akop Karapetyan
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

#include <bcm_host.h>
#include <interface/vchiq_arm/vchiq_if.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>

#include "pigl.h"

static EGL_DISPMANX_WINDOW_T nativeWindow;
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;

static int bcm_host_initted = 0;

uint32_t pigl_screen_width = 0;
uint32_t pigl_screen_height = 0;

int pigl_init()
{
	if (!bcm_host_initted) {
		bcm_host_init();
		bcm_host_initted = 1;
	}

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY) {
		fprintf(stderr, "eglGetDisplay() failed: EGL_NO_DISPLAY\n");
		pigl_shutdown();
		return 0;
	}

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglInitialize() failed: EGL_FALSE\n");
		pigl_shutdown();
		return 0;
	}

	// get an appropriate EGL frame buffer configuration
	EGLint numConfig;
	EGLConfig config;
	static const EGLint attributeList[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};
	result = eglChooseConfig(display, attributeList, &config, 1, &numConfig);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglChooseConfig() failed: EGL_FALSE\n");
		pigl_shutdown();
		return 0;
	}

	result = eglBindAPI(EGL_OPENGL_ES_API);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglBindAPI() failed: EGL_FALSE\n");
		pigl_shutdown();
		return 0;
	}

	// create an EGL rendering context
	static const EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
	if (context == EGL_NO_CONTEXT) {
		fprintf(stderr, "eglCreateContext() failed: EGL_NO_CONTEXT\n");
		pigl_shutdown();
		return 0;
	}

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0, &pigl_screen_width, &pigl_screen_height);
	if (result < 0) {
		fprintf(stderr, "graphics_get_display_size() failed: < 0\n");
		pigl_shutdown();
		return 0;
	}

	fprintf(stderr, "Hardware resolution: %dx%d\n",
		pigl_screen_width, pigl_screen_height);

	VC_RECT_T dstRect;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width = pigl_screen_width;
	dstRect.height = pigl_screen_height;

	VC_RECT_T srcRect;
	srcRect.x = 0;
	srcRect.y = 0;
	srcRect.width = pigl_screen_width << 16;
	srcRect.height = pigl_screen_height << 16;

	DISPMANX_DISPLAY_HANDLE_T dispManDisplay = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T dispmanUpdate = vc_dispmanx_update_start(0);
	DISPMANX_ELEMENT_HANDLE_T dispmanElement = vc_dispmanx_element_add(dispmanUpdate,
		dispManDisplay, 0, &dstRect, 0, &srcRect,
		DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	nativeWindow.element = dispmanElement;
	nativeWindow.width = pigl_screen_width;
	nativeWindow.height = pigl_screen_height;
	vc_dispmanx_update_submit_sync(dispmanUpdate);

	fprintf(stderr, "Initializing window surface...\n");

	surface = eglCreateWindowSurface(display, config, &nativeWindow, NULL);
	if (surface == EGL_NO_SURFACE) {
		fprintf(stderr, "eglCreateWindowSurface() failed: EGL_NO_SURFACE\n");
		pigl_shutdown();
		return 0;
	}

	fprintf(stderr, "Connecting context to surface...\n");

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	if (result == EGL_FALSE) {
		fprintf(stderr, "eglMakeCurrent() failed: EGL_FALSE\n");
		return 0;
	}

	return 1;
}

void pigl_shutdown()
{
	// Release OpenGL resources
	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (surface != EGL_NO_SURFACE) {
			eglDestroySurface(display, surface);
			surface = EGL_NO_SURFACE;
		}
		if (context != EGL_NO_CONTEXT) {
			eglDestroyContext(display, context);
			context = EGL_NO_CONTEXT;
		}
		eglTerminate(display);
		display = EGL_NO_DISPLAY;
	}

	if (bcm_host_initted) {
		bcm_host_deinit();
		bcm_host_initted = 0;
	}
}

void pigl_swap()
{
	if (display) {
		eglSwapBuffers(display, surface);
	}
}
