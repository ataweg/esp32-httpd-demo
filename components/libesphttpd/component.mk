#
# Component Makefile (for esp-idf)
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default,
# this will take the sources in this directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

ifdef CONFIG_ESPHTTPD_ENABLED

MKESPFSIMAGE=$(COMPONENT_BUILD_DIR)/mkespfsimage/mkespfsimage.exe

COMPONENT_SRCDIRS := core espfs util
COMPONENT_ADD_INCLUDEDIRS := core espfs util include
COMPONENT_ADD_LDFLAGS := -lwebpages-espfs -llibesphttpd

COMPONENT_EXTRA_CLEAN := mkespfsimage/* webpages.espfs* libwebpages-espfs.a

HTMLDIR := $(subst ",,$(CONFIG_ESPHTTPD_HTMLDIR))

JS_MINIFY_TOOL ?= uglifyjs

CFLAGS += -DFREERTOS

ifeq ("$(CONFIG_ESPHTTPD_USE_HEATSHRINK)","y")
   USE_HEATSHRINK := yes
   COMPONENT_ADD_INCLUDEDIRS += lib/heatshrink
   CFLAGS += -DESPFS_HEATSHRINK
else
   USE_HEATSHRINK := no
endif

ifeq ("$(CONFIG_ESPHTTPD_USE_GZIP_COMPRESSION)","y")
   USE_GZIP_COMPRESSION := yes
   CFLAGS += -DUSE_GZIP_COMPRESSION
else
   USE_GZIP_COMPRESSION := no
endif

liblibesphttpd.a: libwebpages-espfs.a

# ignore vim swap files
FIND_OPTIONS = -not -iname '*.swp' -not -iname '*.bak'

# mkespfsimage will compress html, css, svg and js files with gzip by default if enabled
# override with -g cmdline parameter

webpages.espfs: $(PROJECT_PATH)/$(HTMLDIR) $(PROJECT_PATH)/$(HTMLDIR)/* $(MKESPFSIMAGE)
ifeq ("$(CONFIG_ESPHTTPD_USE_COMPRESS_W_YUI)","y")
	echo "Build espfs file with web pages ..."
	rm -rf html_compressed;
	cp -r ../html html_compressed;
	echo "Compression assets with yui-compressor. This may take a while..."
	for file in `find html_compressed -type f -name "*.js"`; do $(YUI-COMPRESSOR) --type js $$file -o $$file; done
	for file in `find html_compressed -type f -name "*.css"`; do $(YUI-COMPRESSOR) --type css $$file -o $$file; done
	awk "BEGIN {printf \"YUI compression ratio was: %.2f%%\\n\", (`du -b -s html_compressed/ | sed 's/\([0-9]*\).*/\1/'`/`du -b -s ../html/ | sed 's/\([0-9]*\).*/\1/'`)*100}"
# mkespfsimage will compress html, css, svg and js files with gzip by default if enabled
# override with -g cmdline parameter
	cd html_compressed; find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(BUILD_DIR_BASE)webpages.espfs; cd ..;
else
  ifeq ("$(CONFIG_ESPHTTPD_USE_UGLIFYJS)","y")
	echo "Build espfs file with web pages using uglifyjs ..."
	rm -rf html_compressed;
	cp -r $(PROJECT_PATH)/$(HTMLDIR) html_compressed;
	echo "Compressing javascript assets with uglifyjs"
	for file in `find html_compressed -type f -name "*.js"`; do $(JS_MINIFY_TOOL) $$file -c -m -o $$file; done
	awk "BEGIN {printf \" compression ratio was: %.2f%%\\n\", (`du -b -s html_compressed/ | sed 's/\([0-9]*\).*/\1/'`/`du -b -s $(PROJECT_PATH)/$(HTMLDIR) | sed 's/\([0-9]*\).*/\1/'`)*100}"
	cd html_compressed; find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(COMPONENT_BUILD_DIR)/webpages.espfs; cd ..;
  else
    ifeq ("$(CONFIG_ESPHTTPD_USE_HEATSHRINK)","y")
	echo "Build espfs file with web pages using heatshrink ..."
    else
	echo "Build espfs file with web pages ..."
    endif
	cd  $(PROJECT_PATH)/$(HTMLDIR) && find . $(FIND_OPTIONS) | $(MKESPFSIMAGE) > $(COMPONENT_BUILD_DIR)/webpages.espfs
  endif
endif

libwebpages-espfs.a: webpages.espfs
	$(OBJCOPY) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.rodata \
		webpages.espfs webpages.espfs.o.tmp
	$(CC) -nostdlib -Wl,-r webpages.espfs.o.tmp -o webpages.espfs.o -Wl,-T $(COMPONENT_PATH)/webpages.espfs.esp32.ld
	$(AR) cru $@ webpages.espfs.o

$(MKESPFSIMAGE): $(COMPONENT_PATH)/espfs/mkespfsimage
	mkdir -p $(COMPONENT_BUILD_DIR)/mkespfsimage
	$(MAKE) -C $(COMPONENT_BUILD_DIR)/mkespfsimage -f $(COMPONENT_PATH)/espfs/mkespfsimage/Makefile \
		USE_HEATSHRINK="$(USE_HEATSHRINK)" \
		USE_GZIP_COMPRESSION="$(USE_GZIP_COMPRESSION)" \
		CC=$(HOSTCC)

#clean:
#	$(MAKE) -C $(COMPONENT_BUILD_DIR)/mkespfsimage -f $(COMPONENT_PATH)/espfs/mkespfsimage/Makefile clean

endif
