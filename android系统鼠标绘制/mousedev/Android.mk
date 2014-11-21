LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	mouse.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libui \
	libgui \
	libskia \
	libbinder

LOCAL_C_INCLUDES := \
	external/skia/include/core \
	external/skia/include/effects \
	external/skia/src/ports \
	external/skia/include/utils \
	external/skia/include/lazy \
	external/skia/include/images

LOCAL_CFLAGS := \
	-fpermissive

LOCAL_MODULE:= mousedev

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

define _add-mousedev-image
include $$(CLEAR_VARS)
LOCAL_MODULE := framework_base_mousedev_$(notdir $(1))
LOCAL_MODULE_STEM := $(notdir $(1))
_img_modules += $$(LOCAL_MODULE)
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $$(TARGET_ROOT_OUT)/res/images
include $$(BUILD_PREBUILT)
endef

_img_modules :=
_images :=
$(foreach _img, $(call find-subdir-subdir-files, "images", "*.png"), \
  $(eval $(call _add-mousedev-image,$(_img))))

include $(CLEAR_VARS)
LOCAL_MODULE := mousedev_res_images
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(_img_modules)
include $(BUILD_PHONY_PACKAGE)

_add-mousedev-image :=
_img_modules :=
