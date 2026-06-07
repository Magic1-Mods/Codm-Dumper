LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := codmdumper

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/Metadata.cpp \
    src/Il2CppBase.cpp \
    src/SectionHelper.cpp \
    src/ElfReader.cpp \
    src/Il2CppExecutor.cpp \
    src/Dumper.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/Includes

LOCAL_CPPFLAGS := \
    -std=c++17 \
    -Os \
    -ffunction-sections \
    -fdata-sections \
    -fexceptions \
    -frtti

LOCAL_LDFLAGS := -Wl,--gc-sections

include $(BUILD_EXECUTABLE)
