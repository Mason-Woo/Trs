# =========================================================
INCLUDE_DIR    += -I. -I./include/ -I../rpc -I../common -I../common/ssl/ -I../depend/include -I../

LIB_DIR        += ../depend/lib/libssl.a
LIB_DIR        += ../depend/lib/libcrypto.a
LIB_DIR        += ../depend/lib/libsrtp2.a
LIB_DIR        += ../depend/lib/libavformat.a
LIB_DIR        += ../depend/lib/libavfilter.a
LIB_DIR        += ../depend/lib/libavcodec.a
LIB_DIR        += ../depend/lib/libswresample.a
LIB_DIR        += ../depend/lib/libavresample.a
LIB_DIR        += ../depend/lib/libswscale.a
LIB_DIR        += ../depend/lib/libavutil.a
LIB_DIR        += ../depend/lib/libopus.a
LIB_DIR        += ../depend/lib/libfdk-aac.a
LIB_DIR        += ../depend/lib/libvpx.a
LIB_DIR        += ../depend/lib/libx264.a
LIB_DIR        += ../depend/lib/libmfx.a
#LIB_DIR        += -lpthread -lbz2 -lz -lva -ldl -lva-drm
LIB_DIR        += -lpthread -lbz2 -lz -ldl

# ====================================================
CC             = gcc
CXX 		   = g++
#CXX 		   = clang
CFLAGS         = -g -W -Wall -Werror -O2 -pg
CXXFLAGS       = -g -std=c++0x -DWEBRTC_POSIX -W -Wall -Werror -O2 -pg -Wno-strict-aliasing -Wno-missing-field-initializers -Wno-unused-function -Wno-deprecated-declarations \
				 -Wno-unused-variable -Wno-unused-parameter -Wno-unused-value

# 这4个标志位是互斥的
#CXXFLAGS       += -DUSE_PUBLISH
#CXXFLAGS       += -DUSE_VP8_WEBM
CXXFLAGS       += -DUSE_VP9_WEBM
#CXXFLAGS       += -DUSE_TRANSCODER

CXXFLAGS       += -D__STDC_CONSTANT_MACROS
# ==========================================================
SOURCES += $(wildcard ./*.cpp)
SOURCES += $(wildcard ../common/*.cpp)
SOURCES += $(wildcard ../common/ssl/*.cpp)
SOURCES += $(wildcard ../webrtc/p2p/base/*.cpp)
SOURCES += $(wildcard ../webrtc/base/*.cpp)
SOURCES += $(wildcard ../webrtc/modules/rtp_rtcp/source/*.cpp)
OBJECTS += $(patsubst %.cpp,%.o, $(patsubst %.c,%.o, $(SOURCES)))
# ==========================================================
ALL_OBJECTS = $(OBJECTS)
# ==========================================================
DEP_FILE += $(foreach obj, $(ALL_OBJECTS), $(dir $(obj)).$(basename $(notdir $(obj))).d)
# ==========================================================
TARGET = trs
# ==========================================================

all: $(TARGET)

-include $(DEP_FILE)

.%.d: %.cpp
	@echo "update $@ ..."; \
    echo -n $< | sed s/\.cpp/\.o:/ > $@; \
    $(CXX) $(INCLUDE_DIR) $(CXXFLAGS)  -MM $< | sed '1s/.*.://' >> $@;

%.o: %.cpp
	$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) -o $@ -c $<

.%.d: %.c
	@echo "update $@ ..."; \
    echo -n $< | sed s/\.c/\.o:/ > $@; \
    $(CC) $(INCLUDE_DIR) $(CFLAGS)  -MM $< | sed '1s/.*.://' >> $@;

%.o: %.c
	$(CC) $(INCLUDE_DIR) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJECTS)
	$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) $(OBJECTS) $(LIB_DIR) -o $@

clean:
	rm -f $(DEP_FILE) $(OBJECTS) $(TARGET) *.o
