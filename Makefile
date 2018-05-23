#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32-httpd-demo
BUILD_DIR_BASE := ../build/
#BUILD_DIR_BASE := F:/working/Build/Projects/Evaluation/ESP-WROOM-32/Firmware/esp32-httpd-demo/build/

include $(IDF_PATH)/make/project.mk

CFLAGS += -DESP32 \
          -DFREERTOS \
          -Wno-unused-function \
          -Wno-unused-variable \
          -Wno-unused-but-set-variable

htmlclean:
	echo delete webpages
	rm -f $(BUILD_DIR_BASE)libesphttpd/webpages.*
	rm -f $(BUILD_DIR_BASE)libesphttpd/libwebpages-espfs.a
