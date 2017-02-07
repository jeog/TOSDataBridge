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

from random import choice as _choice, randint as _randint
from time import sleep as _sleep, gmtime as _gmtime
from argparse import ArgumentParser as _ArgumentParser
from platform import system as _system

NEXT_YEAR = _gmtime().tm_year + 1

#expires 01/19/2018
SPY_CALL = '.SPY180119C250'

T_ITEMS = ['SPY','GOOG','QQQ','XLU','XLI','XLE','XLF','XLB','XLP','XLK',
            'MSFT','IWM',SPY_CALL]

T_TOPICS = [t.val for t in tosdb.TOPICS]
T_TOPICS.remove('NULL_TOPIC')

BSIZE = 100
BTIMEOUT = 3000
MAX_ADD = 7

args=None

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

    if _system() not in ["Windows","windows","WINDOWS"]:
        print("error: can only run '--virtual' on non Windows systems")
        return False
    try:
        tosdb.init(dllpath=args.path, root=args.root)
        print('+ init successful')
    except Exception as e:
        print('- init failed:',str(e))
        return False
    
    print()
    return True

def choice_without_replace(i, n, o=[]):
    i = list(i)
    o = list(o)
    if len(i) == 0 or len(o) >= n:
        return o
    else:
        v = _choice(i)
        i.remove(v)
        o.append(v)
        return choice_without_replace(i,n,o)

    
def test(n,pause):   
    is_conn = tosdb.connected()
    if not is_conn:
        print("*** COULD NOT CONNECT... exiting ***\n")
        exit(1)
        
    print("-- CREATE BLOCK --\n")
    b1 = tosdb.TOSDB_DataBlock(BSIZE,True,BTIMEOUT)

    for _ in range(n):
        nt = _randint(1,MAX_ADD)
        ni = _randint(1,MAX_ADD)
        topics = choice_without_replace(T_TOPICS, nt)
        items = choice_without_replace(T_ITEMS, ni)
        print('ADD ITEMS: ', str(items))        
        b1.add_items(*items)
        print('ADD TOPICS: ', str(topics))
        b1.add_topics(*topics)
        
        _sleep(pause)
        print()
        print(b1)
        _sleep(pause)                
        
        items = b1.items()
        ni = len(items)
        if ni > 1:
            ni = _randint(1,ni-1)
            items = choice_without_replace(items, ni)
            print('REMOVE ITEMS: ', str(items))
            b1.remove_items(*items)
            
        topics = b1.topics()
        nt = len(topics)
        if nt > 1:
            nt = _randint(1, nt-1)
            topics = choice_without_replace(topics, nt)
            print('REMOVE TOPICS: ', str(topics))
            b1.remove_topics(*topics)
        
        _sleep(pause)
        print()
        print(b1)
        _sleep(pause)



if __name__ == '__main__':
    parser = _ArgumentParser()
    parser.add_argument('--virtual', type=str,
                        help='test virtual interface via "address port [timeout]"')
    parser.add_argument('num', help='number of runs',type=int, default=100)
    parser.add_argument('pause', help='seconds to pause',choices=['.5','1','3','5'])
    parser.add_argument('--root', help='root directory to search for the library')
    parser.add_argument('--path', help='the exact path of the library')
    args = parser.parse_args()
    if init():
        test(args.num, float(args.pause))
    else:
        exit(1)
  

