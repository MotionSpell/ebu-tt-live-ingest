MYDIR=$(call get-my-dir)
TARGET:=$(BIN)/subtitle-live-ingester.exe
TARGETS+=$(TARGET)

EXE_SUBTITLE_LIVE_INGESTER_SRCS:=\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
	$(MYDIR)/../signals/src/lib_appcommon/safemain.cpp\
	$(MYDIR)/../signals/src/lib_appcommon/options.cpp\
	$(MYDIR)/main.cpp\
	$(MYDIR)/pipeliner.cpp\

$(TARGET): $(EXE_SUBTITLE_LIVE_INGESTER_SRCS:%=$(BIN)/%.o)
