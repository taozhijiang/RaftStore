DEBUG   ?= 1
CXX      = g++
CXXFLAGS = -g -O0 -std=c++0x
SHELL    = /bin/bash
VERSION  = 1.0
PACKAGE  = RaftStoreHttpSrv
EXTRAFLAGS  = -I../libRaft/include -I.. -I../../xtra_rhel6.x/include
EXTRAFLAGS += -L../../xtra_rhel6.x/libs/protobuf-2.5.0 -L../../xtra_rhel6.x/libs/boost -L../../xtra_rhel6.x/libs
XLDFLAGS    = ../build/libRaft/libRaft.a ../build/StoreImpl/libStoreImpl.a ../build/tzhttpd/libtzhttpd.a -lprotoc -lprotobuf -lpthread -lboost_system -lboost_thread -lboost_date_time -lboost_regex -lpthread -ljsoncpp -lrt -ldl -lconfig++ -lssl -lcryptopp


OBJ_DIR    = ./obj

vpath %.cc .

srcs = $(filter-out main.cc, $(notdir $(wildcard *.cc)))
objs = $(srcs:%.cc=$(OBJ_DIR)/%.o)

(PACKAGE) : $(objs) 
	@test -d $(OBJ_DIR) || mkdir $(OBJ_DIR)
	$(CXX) -c $(CXXFLAGS) main.cc -o $(OBJ_DIR)/main.o  $(EXTRAFLAGS)
	$(CXX) $(OBJ_DIR)/main.o $^ $(CXXFLAGS) -o $(PACKAGE)  $(EXTRAFLAGS)  $(XLDFLAGS)

$(objs) : $(OBJ_DIR)/%.o: %.cc
	@test -d $(OBJ_DIR) || mkdir $(OBJ_DIR)
	$(CXX) -MMD -c $(CXXFLAGS) $< -o $@  $(EXTRAFLAGS) 

#check header for obj reconstruction
-include $(OBJ_DIR)/*.d

.PHONY : clean 
clean :	
	-rm -fr $(OBJ_DIR)
