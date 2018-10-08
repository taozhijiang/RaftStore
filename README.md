# RaftStore 

This is a project forked from [logcabin](https://github.com/logcabin/logcabin), which is a hierarchical-tree memory-based distributed storage service,and it is originally developed by Diego Ongaro, the author of Raft Consensus Algorithm.   

To make this project interesting, I have done the fellowing modifications:   
1. Remove test and mock stuffs, cleaner code may help better understand.   
2. Using CMake to build the project.   
3. Remove hierarchical-tree storage in memory, usig persistant flat KV store by [leveldb](https://github.com/google/leveldb). In addition to set, get, remove, also supports range, search.   
4. Add [tzhttpd](https://github.com/taozhijiang/tzhttpd) to support HTTP interface, including HTTP Basic Auth for isolation.

The purpose of the project is helping me better understand Raft Consensus Algorithm.   

Steps to startup:
1. Build main service and client libraries, you will get RaftCabinSrv and libRaft.a, libStoreImpl.a.   
```bash
cd ${PROJECT}; mkdir build; cd build; cmake ../; make
```
2. Build Raft tools, you will get rsCtrl, rsOps, rsReconf.   
```bash
cd ${PROJECT}/Examples; make
``` 
3. Build RaftStoreHttpSrv to support HTTP interface.   
```bash
cd ${PROJECT}/HttpSrv; make
```
4. Configure and start RaftCabinSrv. You should understand the example raftstore-{1,2,3}.conf, and do you proper modifications.   
```bash
./RaftCabinSrv -c raftstore-1.conf --bootstrap

./RaftCabinSrv -c raftstore-1.conf -l c101.debug -d
./RaftCabinSrv -c raftstore-2.conf -l c102.debug -d
./RaftCabinSrv -c raftstore-3.conf -l c103.debug -d
```
And do the membership update by:   
```bash
ALLSERVERS=127.0.0.1:5254,127.0.0.1:5255,127.0.0.1:5256
./rsReconf --cluster=$ALLSERVERS set 127.0.0.1:5254 127.0.0.1:5255 127.0.0.1:5256
```
5. Start RaftStoreHttpSrv to support RESTful API. You can modify the RaftStoreHttpSrv.conf example configuration.   
```bash
./RaftStoreHttpSrv -d
```

All should be done, enjoy!    
```bash
GET /raftstore/api/Leshua/v1/set?KEY=test&VALUE=wwwxzf
GET /raftstore/api/Leshua/v1/get?KEY=test
GET /raftstore/api/Leshua/v1/search?SEARCH=te
GET /raftstore/api/Leshua/v1/range
```
