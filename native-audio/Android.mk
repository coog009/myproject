LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main.c

LOCAL_SHARED_LIBRARIES := \
	libOpenSLES \
	liblog \
	libandroid

LOCAL_C_INCLUDES := \
	frameworks/wilhelm/include

LOCAL_MODULE:= native-audio

include $(BUILD_EXECUTABLE)
