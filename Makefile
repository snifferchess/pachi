
#### CONFIGURATION

# Uncomment one of the options below to change the way Pachi is built.
# Alternatively, you can pass the option to make itself, like:
# 	make MAC=1 DOUBLE_FLOATING=1


# Do you compile on Windows instead of Linux ?
# Please note that performance may not be optimal.
# (XXX: For now, only the mingw target is supported on Windows.
# Patches for others are welcome!)
# To compile in msys2 with mingw-w64, uncomment the following line

# MSYS2=1
# MSYS2_STATIC=1      # Try static build ?
# WIN_HAVE_NO_REGEX_SUPPORT=1

# Do you compile on MacOS/X instead of Linux?
# Please note that performance may not be optimal.

# MAC=1

# Compile Pachi with dcnn support ?
# You'll need to install Boost and Caffe libraries.
# If Caffe is in a custom directory you can set it here.

DCNN=1
CAFFE_PREFIX=/usr/local/caffe

# By default, Pachi uses low-precision numbers within the game tree to
# conserve memory. This can become an issue with playout counts >1M,
# e.g. with extremely long thinking times or massive parallelization;
# 24 bits of floating_t mantissa become insufficient then.

# DOUBLE_FLOATING=1

# Enable performance profiling using gprof. Note that this also disables
# inlining, which allows more fine-grained profile, but may also distort
# it somewhat.

# PROFILING=gprof

# Enable performance profiling using google-perftools. This is much
# more accurate, fine-grained and nicer than gprof and does not change
# the way the actual binary is compiled and runs.

# PROFILING=perftools


# Target directories when running 'make install' / 'make install-data'.
# Pachi will look for extra data files (such as dcnn, pattern, joseki or
# fuseki database) in system directory below in addition to current directory
# (or DATA_DIR environment variable if present).
PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin
DATADIR?=$(PREFIX)/share/pachi

# Generic compiler options. You probably do not really want to twiddle
# any of this.
# (N.B. -ffast-math breaks us; -fomit-frame-pointer is added below
# unless PROFILING=gprof.)
CUSTOM_CFLAGS?=-Wall -ggdb3 -O3 -std=gnu99 -frename-registers -pthread -Wsign-compare -D_GNU_SOURCE -DDATA_DIR=\"$(DATADIR)\"
CUSTOM_CXXFLAGS?=-Wall -ggdb3 -O3


###################################################################################################################
### CONFIGURATION END

MAKEFLAGS += --no-print-directory


##############################################################
ifdef MSYS2
	SYS_CFLAGS?=
	SYS_LDFLAGS?=-pthread -L$(CAFFE_PREFIX)/bin
	SYS_LIBS?=-lws2_32
	CUSTOM_CXXFLAGS+=-I/mingw32/include/OpenBLAS

ifdef WIN_HAVE_NO_REGEX_SUPPORT
	SYS_CFLAGS += -DHAVE_NO_REGEX_SUPPORT
else
	SYS_LIBS += -lregex
endif

	DCNN_LIBS:=-lcaffe -lboost_system-mt -lglog -lstdc++ $(SYS_LIBS)
ifdef MSYS2_STATIC		# Static build, good luck
	DCNN_LIBS:=-Wl,--whole-archive -l:libcaffe.a -Wl,--no-whole-archive  -Wl,-Bstatic  \
                   -lboost_system-mt -lboost_thread-mt -lopenblas -lhdf5_hl -lhdf5 -lszip -lgflags_static \
                   -lglog -lprotobuf -lz -lstdc++ -lwinpthread $(SYS_LIBS)   -Wl,-Bdynamic -lshlwapi
endif
else
##############################################################
ifdef MAC
	SYS_CFLAGS?=-DNO_THREAD_LOCAL
	SYS_LDFLAGS?=-pthread -rdynamic
	SYS_LIBS?=-lm -ldl
	DCNN_LIBS:=-lcaffe -lboost_system -lglog -lstdc++ $(SYS_LIBS)
else
##############################################################
	SYS_CFLAGS?=-march=native
	SYS_LDFLAGS?=-pthread -rdynamic
	SYS_LIBS?=-lm -lrt -ldl
	DCNN_LIBS:=-lcaffe -lboost_system -lglog -lstdc++ $(SYS_LIBS)
endif
endif

ifdef CAFFE_PREFIX
	SYS_LDFLAGS+=-L$(CAFFE_PREFIX)/lib -Wl,-rpath=$(CAFFE_PREFIX)/lib
	CXXFLAGS+=-I$(CAFFE_PREFIX)/include
endif

ifdef DCNN
	CUSTOM_CFLAGS+=-DDCNN
	CUSTOM_CXXFLAGS+=-DDCNN
	SYS_LIBS:=$(DCNN_LIBS)
endif

ifdef DOUBLE_FLOATING
	CUSTOM_CFLAGS+=-DDOUBLE_FLOATING
endif

ifeq ($(PROFILING), gprof)
	CUSTOM_LDFLAGS+=-pg
	CUSTOM_CFLAGS+=-pg -fno-inline
else
	# Whee, an extra register!
	CUSTOM_CFLAGS+=-fomit-frame-pointer
ifeq ($(PROFILING), perftools)
	SYS_LIBS+=-lprofiler
endif
endif

ifndef LD
LD=ld
endif

ifndef AR
AR=ar
endif

ifndef INSTALL
INSTALL=/usr/bin/install
endif

export
unexport INCLUDES
INCLUDES=-I.


OBJS=$(DCNN_OBJS) board.o gtp.o move.o ownermap.o pattern3.o pattern.o patternsp.o patternprob.o playout.o probdist.o random.o stone.o timeinfo.o network.o fbook.o chat.o util.o gogui.o pachi.o
ifdef DCNN
	DCNN_OBJS=caffe.o dcnn.o
endif
# Low-level dependencies last
SUBDIRS=uct uct/policy playout tactics t-unit t-predict distributed engines
DATAFILES=patterns.prob patterns.spat book.dat golast19.prototxt golast.trained joseki19.pdict

all: gitversion.h all-recursive pachi

LOCALLIBS=$(SUBDIRS:%=%/lib.a)
$(LOCALLIBS): all-recursive
	@
pachi: $(OBJS) $(LOCALLIBS)
	$(call cmd,link)

# Use runtime gcc profiling for extra optimization. This used to be a large
# bonus but nowadays, it's rarely worth the trouble.
.PHONY: pachi-profiled
pachi-profiled:
	@make clean all XLDFLAGS=-fprofile-generate XCFLAGS="-fprofile-generate -fomit-frame-pointer -frename-registers"
	./pachi -t =5000 no_tbook < gtp/genmove_both.gtp
	@make clean all clean-profiled XLDFLAGS=-fprofile-use XCFLAGS="-fprofile-use -fomit-frame-pointer -frename-registers"

gitversion.h: .git/HEAD .git/index
	@echo "[make] gitversion.h"
	@branch=`git status | grep '^On branch' | sed -e 's/On branch //'`; \
	 hash=`git rev-parse --short HEAD`;           \
	 date=`git log  --pretty="%at"  HEAD^..HEAD`; \
         date=`date +"%b %e %Y" --date=@$$date`;      \
	 ( echo "#define GIT_BRANCH \"$$branch\"";    \
	   echo "#define GIT_HASH   \"$$hash\"";      \
	   echo "#define GIT_DATE   \"$$date\"" ) > $@

# install-recursive?
install:
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) pachi $(BINDIR)/

install-data:
	$(INSTALL) -d $(DATADIR)
	@for file in $(DATAFILES); do                               \
		if [ -f $$file ]; then                              \
                        echo $(INSTALL) $$file $(DATADIR)/;         \
			$(INSTALL) $$file $(DATADIR)/;              \
		else                                                \
			echo "Warning: $$file datafile is missing"; \
                fi                                                  \
	done;

# Generic clean rule is in Makefile.lib
clean:: clean-recursive
	-@rm pachi gitversion.h >/dev/null 2>&1

clean-profiled:: clean-profiled-recursive

TAGS: FORCE
	@echo "Generating TAGS ..."
	@etags `find . -name "*.[ch]" -o -name "*.cpp"`

FORCE:


-include Makefile.lib
