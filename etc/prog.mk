ifeq ($(TARGET),)
# if the target is so far undefined
TARGET := $(shell basename `pwd`)
endif

include $(OPENMRNPATH)/etc/config.mk

include ../../subdirs
include $(OPENMRNPATH)/etc/$(TARGET).mk

include $(OPENMRNPATH)/etc/path.mk


VPATH = ../../

FULLPATHASMSRCS := $(wildcard $(VPATH)/*.S)
FULLPATHCSRCS := $(wildcard $(VPATH)/*.c)
FULLPATHCXXSRCS := $(wildcard $(VPATH)/*.cxx)
FULLPATHCPPSRCS := $(wildcard $(VPATH)/*.cpp)
FULLPATHTESTSRCS := $(wildcard $(VPATH)/tests/*_test.cc)
ASMSRCS := $(notdir $(FULLPATHASMSRCS)) $(wildcard *.S)
CSRCS := $(notdir $(FULLPATHCSRCS)) $(wildcard *.c)
CXXSRCS := $(notdir $(FULLPATHCXXSRCS)) $(wildcard *.cxx)
CPPSRCS := $(notdir $(FULLPATHCPPSRCS)) $(wildcard *.cpp)
TESTSRCS := $(notdir $(FULLPATHTESTSRCS)) $(wildcard *_test.cc)
OBJS := $(CXXSRCS:.cxx=.o) $(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o) $(ASMSRCS:.S=.o)
TESTOBJS := $(TESTSRCS:.cc=.o)

LIBDIR = $(OPENMRNPATH)/targets/$(TARGET)/lib
FULLPATHLIBS = $(wildcard $(LIBDIR)/*.a) $(wildcard lib/*.a)
LIBDIRS := $(SUBDIRS)
LIBS = $(STARTGROUP) \
       $(foreach lib,$(LIBDIRS),-l$(lib)) \
       $(ENDGROUP) \
       $(LINKCORELIBS)

SUBDIRS += lib
INCLUDES += -I$(OPENMRNPATH)/src/ -I$(OPENMRNPATH)/include
ifdef APP_PATH
INCLUDES += -I$(APP_PATH)
endif
CFLAGS += $(INCLUDES)
CXXFLAGS += $(INCLUDES)
LDFLAGS += -Llib -L$(LIBDIR)

EXECUTABLE = $(shell basename `cd ../../; pwd`)

DEPS += TOOLPATH
MISSING_DEPS:=$(call find_missing_deps,$(DEPS))

ifneq ($(MISSING_DEPS),)
all docs clean veryclean tests mksubdirs:
	@echo "******************************************************************"
	@echo "*"
	@echo "*   Unable to build for $(TARGET), missing dependencies: $(MISSING_DEPS)"
	@echo "*"
	@echo "******************************************************************"
else

# This defines how to create nonexistant directories.
MKSUBDIR_OPENMRNINCLUDE=applib.mk

include $(OPENMRNPATH)/etc/recurse.mk

all: $(EXECUTABLE)$(EXTENTION)

# Makes sure the subdirectory builds are done before linking the binary.
# The targets and variable BUILDDIRS are defined in recurse.mk.
$(FULLPATHLIBS): $(BUILDDIRS)

$(EXECUTABLE)$(EXTENTION): $(OBJS) $(FULLPATHLIBS)  depmake
	$(LD) -o $@ $(OBJS) $(OBJEXTRA) $(LDFLAGS) $(LIBS) $(SYSLIBRARIES)

.PHONY: depmake

depmake:
	make -C $(OPENMRNPATH)/targets/$(TARGET) all

-include $(OBJS:.o=.d)
-include $(TESTOBJS:.o=.d)

.SUFFIXES:
.SUFFIXES: .o .c .cxx .cpp .S

.S.o:
	$(AS) $(ASFLAGS) $< -o $@
	$(AS) -MM $(ASFLAGS) $< > $*.d

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM $(CXXFLAGS) $< > $*.d

.cxx.o:
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM $(CXXFLAGS) $< > $*.d

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $(CFLAGS) $< > $*.d

clean: clean-local

clean-local:
	rm -rf *.o *.d *.a *.so *.output *.cout $(TESTOBJS:.o=) $(EXECUTABLE)$(EXTENTION) $(EXECUTABLE).bin $(EXECUTABLE).lst

veryclean: clean-local

TEST_MISSING_DEPS:=$(call find_missing_deps,HOST_TARGET GTESTPATH GTESTSRCPATH GMOCKPATH GMOCKSRCPATH)

ifneq ($(TEST_MISSING_DEPS),)
tests:
	@echo "***Not building tests at target $(TARGET), because missing: $(TEST_MISSING_DEPS) ***"

else
VPATH:=$(VPATH):$(GTESTPATH)/src:$(GTESTSRCPATH):$(GMOCKPATH)/src:$(GMOCKSRCPATH):../../tests
INCLUDES += -I$(GTESTPATH)/include -I$(GTESTPATH) -I$(GMOCKPATH)/include -I$(GMOCKPATH)

TEST_OUTPUTS=$(TESTOBJS:.o=.output)

TEST_EXTRA_OBJS += gtest-all.o gmock-all.o

.cc.o:
	$(CXX) $(CXXFLAGS) $< -o $@
	$(CXX) -MM $(CXXFLAGS) $< > $*.d

gtest-all.o : %.o : $(GTESTSRCPATH)/src/%.cc
	$(CXX) $(CXXFLAGS) -I$(GTESTPATH) -I$(GTESTSRCPATH)  $< -o $@
	$(CXX) -MM $(CXXFLAGS) -I$(GTESTPATH) -I$(GTESTSRCPATH) $< > $*.d

gmock-all.o : %.o : $(GMOCKSRCPATH)/src/%.cc
	$(CXX) $(CXXFLAGS) -I$(GMOCKPATH) -I$(GMOCKSRCPATH)  $< -o $@
	$(CXX) -MM $(CXXFLAGS) -I$(GMOCKPATH) -I$(GMOCKSRCPATH) $< > $*.d

.PHONY: $(TEST_OUTPUTS)

$(TEST_OUTPUTS) : %_test.output : %_test
	./$*_test --gtest_death_test_style=threadsafe

$(TESTOBJS:.o=) : %_test : %_test.o $(TEST_EXTRA_OBJS) $(FULLPATHLIBS)
	$(LD) -o $*_test$(EXTENTION) $*_test.o $(TEST_EXTRA_OBJS) $(OBJEXTRA) $(LDFLAGS)  $(LIBS) $(SYSLIBRARIES) -lstdc++

$(info test deps: $(FULLPATHLIBS) )

%_test.o : %_test.cc
	$(CXX) $(CXXFLAGS:-Werror=) -fpermissive  $< -o $*_test.o
	$(CXX) -MM $(CXXFLAGS) $< > $*_test.d

#$(TEST_OUTPUTS) : %_test.output : %_test.cc gtest-all.o gtest_main.o
#	$(CXX) $(CXXFLAGS) $< -o $*_test.o
#	$(CXX) -MM $(CXXFLAGS) $< > $*_test.d
#	$(LD) -o $*_test$(EXTENTION) $+ $(OBJEXTRA) $(LDFLAGS) $(LIBS) $(SYSLIBRARIES) -lstdc++
#	./$*_test

tests : all $(TEST_OUTPUTS)

endif  # if we are able to run tests

endif
