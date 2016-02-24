include make.local

BASE            = .
#DYNINST_ROOT   ?= $(BASE)/dyninst
#LIBELF         ?= /usr/lib
#LIBDWARF       ?= /usr/lib
CXX	            = g++
CXXFLAGS        = -g -Wall
INCLUDE         = -I$(BASE)/libfeat -I$(DYNINST_ROOT)/include
LIBVERSION      = 1.0
LDFLAGS         =

# Dyninst-dependent programs
#PLATFORM       ?= x86_64-unknown-linux2.4
DYNLDFLAGS      = -L$(LIBELF)\
                  -L$(LIBDWARF)\
                  -L$(DYNINST_ROOT)/$(PLATFORM)/lib\
                  -lelf -ldwarf -lcommon -linstructionAPI -lsymtabAPI\
                  -lparseAPI -liberty
            
ifndef DEBUG
CXXFLAGS       += -O3 
endif
TARG            = ngrams graphlets libcalls supergraphlets calldfa idioms
V               = @

.DEFAULT_GOAL := all

DEPS =\
        ngrams.cc\
        graphlets.cc\
        colors.cc\
        libcalls.cc\
        supergraphlets.cc\
        supergraph.cc\
        calldfa.cc\
        idioms.cc

all: $(TARG)

%.o:%.cc
	@echo + cc $<
	$(V)$(CXX) -c $(CXXFLAGS) $(INCLUDE) -o $@ $<

ngrams: CXXFLAGS += $(DYNCXXFLAGS)
ngrams: LDFLAGS += $(DYNLDFLAGS)
ngrams: ngrams.o
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

graphlets: CXXFLAGS += $(DYNCXXFLAGS)
graphlets: LDFLAGS += $(DYNLDFLAGS)
graphlets: graphlets.o colors.o
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

supergraphlets: CXXFLAGS += $(DYNCXXFLAGS)
supergraphlets: LDFLAGS += $(DYNLDFLAGS)
supergraphlets: supergraphlets.o colors.o supergraph.o
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

calldfa: CXXFLAGS += $(DYNCXXFLAGS)
calldfa: LDFLAGS += $(DYNLDFLAGS)
calldfa: calldfa.o colors.o supergraph.o
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

libcalls: CXXFLAGS += $(DYNCXXFLAGS)
libcalls: LDFLAGS += $(DYNLDFLAGS)
libcalls: libcalls.o
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

idioms: CXXFLAGS += $(DYNCXXFLAGS)
idioms: LDFLAGS += $(DYNLDFLAGS) -L$(BASE)/libfeat -lfeat
idioms: idioms.o libfeat
	@echo + ld $@
	$(V)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

.PHONY: libfeat
libfeat:
	$(V)make -C $@

.PHONY: libfeat_clean
libfeat_clean:
	@make -C libfeat clean

-include depend
depend: $(DEPS) Makefile
	$(V)gcc $(INCLUDE) $(DYNCXXFLAGS) -MM $(DEPS) > depend

.PHONY: clean
clean: libfeat_clean
	rm -f core core.* *.core *.o $(TARG) depend
