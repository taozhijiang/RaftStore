#!/usr/bin/env python

import sys

import json
import requests


# These two lines enable debugging at httplib level (requests->urllib3->http.client)
# You will see the REQUEST, including HEADERS and DATA, and RESPONSE with HEADERS but without DATA.
# The only thing missing will be the response.body which is not logged.
#try:
#    import http.client as http_client
#except ImportError:
#    # Python 2
#    import httplib as http_client
#http_client.HTTPConnection.debuglevel = 1
#
#
#logging.basicConfig()
#logging.getLogger().setLevel(logging.DEBUG)
#requests_log = logging.getLogger("requests.packages.urllib3")
#requests_log.setLevel(logging.DEBUG)
#requests_log.propagate = True


RAFT_AUTH_USR='usr'
RAFT_AUTH_PASSWD='passwd'
RAFT_BASE='http://www.example.com/raftstore/api/dbproject/v1/'

def usage():
    print 'raftstore usage:'
    print '    raft get    key'
    print '    raft set    key value'
    print '    raft rm     key'
    print '    raft range  start [limit] [end]'
    print '    raft search key [limit]'
    print ''
    sys.exit()

def get(key):
    if not key:
        usage()
    payload = {'KEY': key}
    res = requests.get(RAFT_BASE+"get", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['CODE'] != 0:
        print msg
        return
    print msg['VALUE']
    
    
    
def set(key, value):
    if not key or not value:
        usage()
    payload = {'KEY': key, 'VALUE': value}
    res = requests.get(RAFT_BASE+"set", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    print msg
    
def remove(key):
    if not key:
        usage()
    payload = {'KEY': key}
    res = requests.get(RAFT_BASE+"remove", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    print msg
    
def rng(start, limit, end):
    payload = {}
    if start:
        payload['START'] = start
    if limit:
        payload['LIMIT'] = int(limit)
    if end:
        payload['END'] = end
        
    res = requests.get(RAFT_BASE+"range", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['CODE'] != 0 or 'VALUE' not in msg:
        print msg
        return
    ls = json.loads(msg['VALUE'])
    for item in ls:
        print item
    
def search(key, limit):
    if not key:
        usage()
        
    payload = {'SEARCH': key}
    if limit:
        payload['LIMIT'] = int(limit)
        
    res = requests.get(RAFT_BASE+"search", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['CODE'] != 0 or 'VALUE' not in msg:
        print msg
        return
    ls = json.loads(msg['VALUE'])
    for item in ls:
        print item

if __name__ ==  "__main__":

    if len(sys.argv) < 2:
        usage()
        
    cmd = sys.argv[1]
    arg1 = None; arg2 = None; arg3 = None;
    
    if len(sys.argv) >= 3:
        arg1 = sys.argv[2]
    if len(sys.argv) >= 4:
        arg2 = sys.argv[3]
    if len(sys.argv) >= 5:
        arg3 = sys.argv[4]
                
    if cmd == 'get':    
        get(arg1)        
        
    elif cmd == 'set':
        set(arg1, arg2)
        
    elif cmd == 'rm':
        remove(arg1)        
    
    elif cmd == 'rng':
        rng(arg1, arg2, arg3)        
        
    elif cmd == 'se':
        search(arg1, arg2)        
    
    else:
        print 'Unknown cmd:' + cmd
        usage()
