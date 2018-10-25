#!/usr/bin/env python

import sys
import os.path 

import base64
import json
import requests

import hashlib
import zlib

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
    print '    raft get  key [type]'
    print '    raft set  key value'
    print '    raft setp file'
    print '    raft rm   key'
    print '    raft rng  start [limit] [end]'
    print '    raft se   key [limit]'
    print ''
    sys.exit()

def get(key):
    if not key:
        usage()
    payload = {'key': key}
    res = requests.get(RAFT_BASE+"get", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['code'] != 0:
        print msg
        return
    print msg['value']
    
    
    
def set(key, value):
    if not key or not value:
        usage()
    payload = {'key': key, 'value': value}
    res = requests.get(RAFT_BASE+"set", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    print msg


def setp(filename):
    if not filename:
        usage()

    try:
        data = open(filename, "r").read()
        
        md = hashlib.md5()   
        md.update(data)
        md5sum = md.hexdigest()
        
        ext = os.path.splitext(filename)[1]
        if not ext:
            ext = '.bin'
        
        deflate_compress = zlib.compressobj(9, zlib.DEFLATED, -zlib.MAX_WBITS)
        deflate_data = deflate_compress.compress(data) + deflate_compress.flush()
        encoded = base64.b64encode(deflate_data)
    except Exception, e:
        print 'Read and encode file %s failed: %s' %(filename, repr(e))
        return

    key = md5sum + ext
    payload = {'key': key, 'value': encoded, 'type': 'compact', 'md5sum': md5sum }
    res = requests.post(RAFT_BASE+"set", 
                       data = json.dumps(payload),
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    print msg
    
    # more hints
    if 'code' in msg and msg['code'] == 0:
        md5r = msg['md5sum']
        print RAFT_BASE+'get?key='+key+'&type=raw'  # compact|raw|deflate
    
def remove(key):
    if not key:
        usage()
    payload = {'key': key}
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
        payload['start'] = start
    if limit:
        payload['limit'] = int(limit)
    if end:
        payload['end'] = end
        
    res = requests.get(RAFT_BASE+"range", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['code'] != 0 or 'value' not in msg:
        print msg
        return
    ls = json.loads(msg['value'])
    for item in ls:
        print item
    
def search(key, limit):
    if not key:
        usage()
        
    payload = {'search': key}
    if limit:
        payload['limit'] = int(limit)
        
    res = requests.get(RAFT_BASE+"search", 
                       params = payload,
                       auth = (RAFT_AUTH_USR, RAFT_AUTH_PASSWD), 
                       )
    if res.status_code != 200:
        print 'HTTP ERROR:' + repr(res.status_code)
        return
    
    msg = res.json()
    if msg['code'] != 0 or 'value' not in msg:
        print msg
        return
    ls = json.loads(msg['value'])
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

    elif cmd == 'setp':
        setp(arg1)

    elif cmd == 'rm':
        remove(arg1)        
    
    elif cmd == 'rng':
        rng(arg1, arg2, arg3)        
        
    elif cmd == 'se':
        search(arg1, arg2)        
    
    else:
        print 'Unknown cmd:' + cmd
        usage()
