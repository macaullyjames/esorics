BASE            = ..
#DYNINST_ROOT   ?= $(BASE)/dyninst
CXX	            = g++
CXXFLAGS        = -g -Wall --std=c++14
LIBVERSION      = 1.0
LDFLAGS         = 

# Dyninst etc
#PLATFORM        = x86_64-unknown-linux2.4
DYNLDFLAGS      = -L/usr/lib64/dyninst \
                  -lelf -ldwarf -lcommon -linstructionAPI -lsymtabAPI -lparseAPI

DYNCXXFLAGS     = -I/usr/include/dyninst

CXXFLAGS       += $(DYNCXXFLAGS)
LDFLAGS        += $(DYNLDFLAGS)

ifndef DEBUG
CXXFLAGS       += -O3 
endif
TARG            = libfeat.so
V               = @

.DEFAULT_GOAL := all

all: $(TARG)

HDR = feature.h
LFC =\
	feature.cc\
    idiom.cc\
    operand.cc\
    lookup.cc

LFO = $(LFC:.cc=.o)

%.o:%.cc
	@echo + cc $<
	$(V)$(CXX) -c $(CXXFLAGS) $(INCLUDE) -o $@ $<

libfeat.so: CXXFLAGS += -fPIC -nostdlib
libfeat.so: $(LFO)
	@echo + ld $@
	$(V)$(CXX) -shared -Wl,-soname,libfeat.so.${LIBVERSION} -o $@ $^
	$(V)if [ ! -e $@.${LIBVERSION} ] ; then ln -s $@ $@.${LIBVERSION} ; fi

-include depend
depend: $(LFC) Makefile
	$(V)gcc $(CXXFLAGS) -MM $(LFC) > depend

clean:
	rm -f core core.* *.core *.o $(TARG) *.so *.so.* depend
