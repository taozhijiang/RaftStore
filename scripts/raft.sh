#!/bin/bash

RAFT_AUTH='usr:passwd'
RAFT_BASE='http://www.example.com/raftstore/api/dbproject/v1/'

function raftget() {
    url="${RAFT_BASE}/get"
    val=`curl --user ${RAFT_AUTH} -G --silent --data-urlencode key=${1} ${url}`
    tmp=`echo ${val} | sed 's/.*"VALUE":\([^,}]*\).*/\1/'`
    ret=`echo ${tmp} | sed 's/\"//g'`
    echo ${ret}
}

function raftset() {
    url="${RAFT_BASE}/set"
    val=`curl --user ${RAFT_AUTH} -G --silent --data-urlencode key=${1} --data-urlencode value=${2} ${url}`
    echo ${val}
}

function raftrm() {
    url="${RAFT_BASE}/remove"
    val=`curl --user ${RAFT_AUTH} -G --silent --data-urlencode key=${1} ${url}`
    echo ${val}
}

function raftrng() {
    url="${RAFT_BASE}/range"
    val=`curl --user ${RAFT_AUTH} -G --silent ${url}`
    echo ${val}
}

function raftse() {
    url="${RAFT_BASE}/search"
    val=`curl --user ${RAFT_AUTH} -G --silent --data-urlencode search=${1} ${url}`
    echo ${val}
}

alias raft_get='raft.py get'
alias raft_set='raft.py set'
alias raft_rm='raft.py rm'
alias raft_rng='raft.py rng'
alias raft_se='raft.py se'
alias raft_setp='raft.py setp'
alias raft_up='raft.py upload'
