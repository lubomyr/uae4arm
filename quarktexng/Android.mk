# QuarkTex NG's host library, libquarktexng.so, which uaenative.library
# opens for QuarkTex's Amiga libraries: in project/jni/quarktexng/ with
# QuarkTex's Source folder as src/. On Android it draws with OpenGL ES 3,
# so only ogles2.library's commands are executed (host/gles.cpp).
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := quarktexng

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src/host/gles $(LOCAL_PATH)/src/gl

LOCAL_CPPFLAGS := -O2 -std=c++11 -Wall

LOCAL_SRC_FILES := src/host/quarktex.cpp src/host/gldecode.cpp src/host/ogles2.cpp \
	src/host/gl3.cpp src/host/present.cpp src/host/gles.cpp

LOCAL_LDLIBS := -lEGL -lGLESv3

include $(BUILD_SHARED_LIBRARY)
