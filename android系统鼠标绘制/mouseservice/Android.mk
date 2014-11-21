LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	mouse.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils

LOCAL_CFLAGS := \
	-fpermissive

LOCAL_MODULE:= mouse

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	framebuffer.cpp

LOCAL_MODULE:= framebuffer

LOCAL_MODULE_TAGS:= optional

include $(BUILD_EXECUTABLE)
