IDLS=$(wildcard *.proto)
OBJS=$(IDLS:%.proto=../gen-cpp/%.pb.cc)

all: ${OBJS}
	@echo "remove ../gen-cpp/*.skeleton.cpp files"

../gen-cpp/%.pb.cc: %.proto
	../../../../xtra_rhel6.x/bin/protoc-2.5.0 -I. --cpp_out=../gen-cpp $^
