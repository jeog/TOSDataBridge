# Copyright (C) 2014 Jonathon Ogden   < jeog.dev@gmail.com >
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#   See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License,
#   'LICENSE.txt', along with this program.  If not, see 
#   <http://www.gnu.org/licenses/>.

import tosdb
from sys import stderr as _stderr, maxsize as _maxsize
from math import log as _log
from random import choice as _rchoice
from time import sleep as _sleep
from argparse import ArgumentParser as _ArgumentParser
from platform import system as _system

T_ITEMS = ['SPY','GOOG','QQQ']
T_TOPICS = ['LAST','LASTX','VOLUME','LAST_SIZE','PUT_CALL_RATIO']

bsize = 100
args=None
vaddr=None

_ladj = lambda s : s.ljust(20)

BASE_ADMIN_CALLS = ['connected','get_block_limit','set_block_limit','type_bits','type_string']


def init():
    global vaddr

    print()
    if args.path:
        print('+ lib path:', args.path)
    elif args.root:
        print('+ lib root:', args.root)
    else:
        print('error: tosdb_test.py requires a --root or --path arg for library')
        return False

    if args.virtual:
        raw = args.virtual.split(' ')
        try:
            v = ((raw[0],int(raw[1])),)
        except IndexError:
            print("usage: tosdb_test --virtual 'address port [timeout]'",file=_stderr)
            return False
        vaddr = v[0]
        v += (int(raw[2]),) if len(raw) > 2 else ()
        if args.auth:
            v = v[:2] + (args.auth,) + v[2:]
        try:
            tosdb.admin_init(*v)        
            print('+ admin_init successful')
        except Exception as e:
            print('- admin_init failed:', str(e))
            return False        
        try:
            res = tosdb.vinit(dllpath=args.path, root=args.root)
            print('+ vinit successful')
        except Exception as e:
            print('- vinit failed:',str(e))
            return False
    else:
        if _system() not in ["Windows","windows","WINDOWS"]:
            print("error: can only run '--virtual' on non Windows systems")
            return False
        try:
            res = tosdb.init(dllpath=args.path, root=args.root)
            print('+ init successful')
        except Exception as e:
            print('- init failed:',str(e))
            return False       

    print()
    return True


def test():   
    global bsize  
    if args.virtual:
        admin_calls = {k:getattr(tosdb,'v'+k) for k in BASE_ADMIN_CALLS}
    else:
        admin_calls = {k:getattr(tosdb,k) for k in BASE_ADMIN_CALLS}

    print(_ladj("CONNECTED:"), admin_calls['connected']() )

    blim = admin_calls['get_block_limit']()  
    if blim < 2: # in case someone else lowered this 
        admin_calls['set_block_limit'](2) # (need to be able to half it w/o hitting 0)
    blim = admin_calls['get_block_limit']()
    print(_ladj("GET BLOCK LIMIT:"), blim )

    print(_ladj("HALVE BLOCK LIMIT:"))
    admin_calls['set_block_limit'](int(blim/2))
    print(_ladj("GET BLOCK LIMIT:"), admin_calls['get_block_limit']() )
  
    print()
    print(_ladj("-- TOPIC BITS/STRINGS --"))
    for t in T_TOPICS:
        print('  ', _ladj(t), str(admin_calls['type_bits'](t)).ljust(4),
              _ladj(admin_calls['type_string'](t)))

    if args.virtual:
        print()
        print("-- CREATE (VIRTUAL) BLOCK --")        
        b1 = tosdb.VTOSDB_DataBlock(vaddr,args.auth,bsize,True,2000)
    else:
        print()
        print("-- CREATE BLOCK --")
        b1 = tosdb.TOSDB_DataBlock(bsize,True,2000)

    for i,v in b1.info().items():
        print('  ',_ladj(i), v)
  
    print()
    bsize = b1.get_block_size()
    print(_ladj("GET BLOCK SIZE"), bsize)

    print(_ladj("DOUBLE BLOCK SIZE"))
    bsize *= 2
    b1.set_block_size(bsize)
    print(_ladj("GET BLOCK SIZE"), b1.get_block_size())
  
    print()
    print(_ladj("-- ADD ITEMS --"))
    b1.add_items(*T_ITEMS)
    print('  ',_ladj("ITEMS:"), b1.items())
    print('  ',_ladj("ITEMS_PRECACHED:"), b1.items_precached())
    print('  ',_ladj("TOPICS:"), b1.topics())
    print('  ',_ladj("TOPICS_PRECACHED:"), b1.topics_precached())

    print()
    print(_ladj("-- ADD TOPICS --"))
    b1.add_topics(*T_TOPICS)
    print('  ',_ladj("ITEMS:"), b1.items())
    print('  ',_ladj("ITEMS_PRECACHED:"), b1.items_precached())
    print('  ',_ladj("TOPICS:"), b1.topics())
    print('  ',_ladj("TOPICS_PRECACHED:"), b1.topics_precached())

    if sorted(b1.items()) != sorted(T_ITEMS):
        print("!! ITEMS DON'T MATCH !!", file=_stderr)
    if sorted(b1.topics()) != sorted(T_TOPICS):
        print("!! TOPICS DON'T MATCH !!", file=_stderr)
  
    print()
    rt = T_TOPICS.pop()  
    print("-- REMOVE TOPIC: " + rt + " --")
    b1.remove_topics(rt)

    if sorted(b1.topics()) != sorted(T_TOPICS):
        print("!! TOPICS DON'T MATCH !!", file=_stderr)
    else:
        print('  ',_ladj("ITEMS:"), b1.items())
        print('  ',_ladj("TOPICS:"), b1.topics())

    print()
    ri = T_ITEMS.pop()
    print("-- REMOVE ITEM: " + ri + " --")
    b1.remove_items(ri)

    if sorted(b1.items()) != sorted(T_ITEMS):
        print("!! ITEMS DON'T MATCH !!", file=_stderr)
    else:
        print('  ',_ladj("ITEMS:"), b1.items())
        print('  ',_ladj("TOPICS:"), b1.items())
    
    print()
    print("Sleep 5 seconds for historical data...",flush=1,end='')
    for _ in range(10):
        _sleep(.5)
        print('...',flush=1,end='')
    print()
  
    print()
    print("-- GET (RANDOM) --")  
    for i in range(0,10,2):
        ii = _rchoice(T_ITEMS)
        tt = _rchoice(T_TOPICS)
        dd = _rchoice([1,0])      
        print('  ',ii.ljust(6),_ladj(tt),str(bool(dd)).ljust(6),end=' ')
        try:
            v = b1.get(ii,tt,dd,i)
            print(str(v))
        except tosdb._common.TOSDB_DataError:
            print("no data yet")           
    
    ii = T_ITEMS[0] # SPY
    tt = "LAST"  

    print()
    print("-- STREAM SNAPSHOT --", ii,tt,'date_time=True','end=3')
    vdt = b1.stream_snapshot(ii,tt,True,3)
    for k in vdt:
        print('  ',str(k[1]),str(k[0]))

    print()
    print("-- STREAM SNAPSHOT --", ii,tt,'date_time=False','end=10')
    vdt = b1.stream_snapshot(ii,tt,False,10)
    print('  ',str(vdt))

    print()
    print("-- STREAM SNAPSHOT FROM MARKER --",ii,tt, 'date_time=True','beg = 0')
    for i in range(3):
        print('  ',"(sleep 2 second(s))")
        _sleep(2)
        ssfm = b1.stream_snapshot_from_marker(ii,tt,True, 0)
        if(ssfm):
            ssfm.reverse()
            for k in ssfm:
                print('  ',str(k[1]), str(k[0]))  
        else:
            print('   ...') 

    print()
    print("-- ITEM FRAME --",tt,'date_time=False')
    print('  ',b1.item_frame(tt))
  
    print()
    print("-- TOPIC FRAME --",ii,'date_time=False')
    print('  ',b1.topic_frame(ii))

    if not args.virtual:
        print()
        print("-- TOTAL FRAME --",ii,'date_time=False')
        print('  ',b1.total_frame())
  
    print()



if __name__ == '__main__':
    parser = _ArgumentParser()
    parser.add_argument('--virtual', type=str,
                        help='test virtual interface via "address port [timeout]"')
    parser.add_argument('--root', help='root directory to search for the library')
    parser.add_argument('--path', help='the exact path of the library')
    parser.add_argument('--auth', help='password to use for authentication')
    args = parser.parse_args()
    if init():
        test()
    else:
        exit(1)
  

