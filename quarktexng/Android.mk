# QuarkTex NG's host library, libquarktexng.so, which uaenative.library
# opens for QuarkTex's Amiga libraries: in project/jni/quarktexng/ with
# QuarkTex's Source folder as src/ and gl4es as gl4es/. It draws with OpenGL
# ES 3 and executes ogles2.library's and Warp3D's commands itself;
# minigl.library's and agl.library's go to libquarktexng_gl4es.so below.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := quarktexng

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/host/gles $(LOCAL_PATH)/src/gl

LOCAL_CPPFLAGS := -O2 -std=c++11 -Wall

LOCAL_SRC_FILES := src/host/quarktex.cpp src/host/gldecode.cpp src/host/ogles2.cpp src/host/w3d.cpp src/host/ffp.cpp \
	src/host/gl3.cpp src/host/present.cpp src/host/gles.cpp

LOCAL_LDLIBS := -lEGL -lGLESv3

include $(BUILD_SHARED_LIBRARY)

# libquarktexng_gl4es.so: the same sources for minigl.library and
# agl.library, whose OpenGL 1.1 gl4es (github.com/ptitseb/gl4es, in gl4es/
# here) draws with OpenGL ES 2. The OpenGL ES library opens it for their
# contexts. gl4es's OpenGL functions stay inside it, so that the OpenGL ES
# library's are the real ones; it exports only qt_gl4es_functions.

# gl4es as its Android.mk builds it, but started by the library when it
# needs it (NO_INIT_CONSTRUCTOR), and named apart from androidsdl's old one
include $(CLEAR_VARS)

LOCAL_MODULE := quarktexng_gl4es_gl

GL4ES := $(LOCAL_PATH)/gl4es

LOCAL_C_INCLUDES := $(GL4ES)/include

LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(filter-out %/glx_stubs.c %/rpi.c %/utils.c, \
	$(wildcard $(GL4ES)/src/gl/*.c $(GL4ES)/src/gl/wrap/*.c $(GL4ES)/src/gl/math/*.c $(GL4ES)/src/glx/*.c)))

LOCAL_CFLAGS := -std=gnu99 -funwind-tables -O3 -fvisibility=hidden -include $(GL4ES)/include/android_debug.h \
	-DNOX11 -DNO_GBM -DDEFAULT_ES=2 -DSTATICLIB -DNO_INIT_CONSTRUCTOR -Wno-typedef-redefinition -Wno-dangling-else

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := quarktexng_gl4es

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/gl

# gl4es's GL/gl.h, but the system's EGL and GLES headers
LOCAL_CPPFLAGS := -O2 -std=c++11 -Wall -DQT_GL4ES -idirafter $(LOCAL_PATH)/gl4es/include

LOCAL_SRC_FILES := src/host/quarktex.cpp src/host/gldecode.cpp src/host/mgl.cpp \
	src/host/gl3.cpp src/host/present.cpp src/host/gles.cpp

LOCAL_STATIC_LIBRARIES := quarktexng_gl4es_gl

LOCAL_LDFLAGS := -Wl,--exclude-libs,ALL

LOCAL_LDLIBS := -lEGL -llog -ldl

include $(BUILD_SHARED_LIBRARY)
