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
from random import choice as _choice, shuffle as _shuffle, randint as _randint
from time import sleep as _sleep
from argparse import ArgumentParser as _ArgumentParser
from platform import system as _system

N=1000
MAX_SEC_PER_TEST=30
TOPICS=['LAST','VOLUME','LAST_SIZE','LASTX']
ITEMS=['SPY','/ES:XCME']


def test(func_name, block, args, passes, wait):
    mdata = []
    for i in range(passes):
        print('+     ', func_name, args[0], args[1], str(i))
        _sleep(wait)        
        d = getattr(block,func_name)(*args)
        if d:
            d.reverse()
            mdata.extend(d)
        else:
            print('-     ', 'NONE')
    sdata = block.stream_snapshot(args[0], args[1], date_time=args[2])
    sdata.reverse()
    fout_name = 'out/' + func_name + '-' + args[0].replace('/','+') + '-' \
                + args[1] + '-' + str(args[2]) + '-' + str(passes) + '-' \
                + str(wait) + '.test'
    with open(fout_name, 'w') as fout:
        for i in range(max(len(mdata),len(sdata))):
            _write_line(fout, mdata, sdata, i, args[2])    


def test_marker_func(fname, *args):
    block = tosdb.TOSDB_DataBlock(N,True)
    block.add_items(*ITEMS)
    block.add_topics(*TOPICS)   
    topics = list(TOPICS)    
    _shuffle(topics)
    for topic in topics:
        item = _choice(ITEMS)
        date_time = _choice([True,False])        
        passes = _randint(3,10)
        wait = int(MAX_SEC_PER_TEST/passes)
        print("+ TEST", fname, str((item,topic,date_time,passes,wait)))
        test(fname, block, (item, topic, date_time) + args, passes, wait)
    block.close()    


def _write_line(fout, mdata, sdata, i, date_time):
    def _try_write(lst, i, w, *ii):
        try:
            v = lst[i][ii[0]] if ii else lst[i]
            fout.write( str(v).ljust(w) )                       
        except IndexError:
            fout.write( "".ljust(w) )        
    if date_time:
        _try_write(mdata,i,10,0)
        fout.write(' ')
        _try_write(sdata,i,10,0)
        fout.write('\t')
        _try_write(mdata,i,10,1)
        fout.write(' ')
        _try_write(sdata,i,10,1)                
    else:
        _try_write(mdata,i,50)
        fout.write('\t')
        _try_write(sdata,i,50)
    fout.write('\n')

    
if __name__ == '__main__':
    parser = _ArgumentParser()
    parser.add_argument('--root', help='root directory to search for the library')
    parser.add_argument('--path', help='the exact path of the library')
    args = parser.parse_args()
    if not args.path and not args.root:  
        print('error: --root or --path arg requried')
        exit(1)  
    try:
        res = tosdb.init(dllpath=args.path, root=args.root)        
    except Exception as e:
        print('error: exception during init:', str(e))
        exit(1)  
    print("+ BEGIN")
    test_marker_func('stream_snapshot_from_marker')
    test_marker_func('n_from_marker', N)
    tosdb.clean_up()
    print("+ END")
 


