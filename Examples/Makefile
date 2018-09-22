DEBUG ?= 1
CXX = g++
CXXFLAGS = -g -O0 -std=c++0x
SHELL = /bin/bash
VERSION = 1.0
EXTRAFLAGS =  -I../libraft/include
EXTRAFLAGS += -L../../xtra_rhel6.x/libs/protobuf-2.5.0 ../build/libraft/libRaft.a ../build/StoreImpl/libStoreImpl.a -lprotoc -lprotobuf -lrt -lpthread

#vpath %.cpp ./
TARGET_DIR=./bin

src = $(filter-out main.cc, $(notdir $(wildcard *.cc)))
exe = $(src:%.cc=$(TARGET_DIR)/%)

all : $(exe)
.PHONY : all


$(exe) : $(TARGET_DIR)/%: %.cc
	@test -d $(TARGET_DIR) || mkdir $(TARGET_DIR)
	$(CXX) -MMD $(CXXFLAGS) $< -o $@ $(EXTRAFLAGS) 

#check header for obj reconstruction
-include $(OBJDIR)/*.d

.PHONY : clean 
clean :	
	-rm -fr $(OBJDIR)
	-rm -fr $(TARGET_DIR)
