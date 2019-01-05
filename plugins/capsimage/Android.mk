LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := capsimage

APP_SUBDIRS := $(patsubst $(LOCAL_PATH)/%, %, $(shell find $(LOCAL_PATH)/CAPSImg \
														   $(LOCAL_PATH)/Codec \
														   $(LOCAL_PATH)/Core \
														   $(LOCAL_PATH)/Device \
														   $(LOCAL_PATH)/LibIPF -type d))

LOCAL_C_INCLUDES := $(foreach D, $(APP_SUBDIRS), $(LOCAL_PATH)/$(D))

LOCAL_CFLAGS := -Wall -Wno-sign-compare -Wno-missing-braces -Wno-parentheses -g  -O2 -fomit-frame-pointer  -fno-exceptions -fno-rtti -std=c++0x

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := $(foreach F, $(APP_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.cpp))))
LOCAL_SRC_FILES += $(foreach F, $(APP_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.c))))

LOCAL_SHARED_LIBRARIES := 

include $(BUILD_SHARED_LIBRARY)
