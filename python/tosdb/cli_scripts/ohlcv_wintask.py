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
#from tosdb.intervalize import TimeInterval as _TI
from tosdb import ohlc

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr


BLOCK_SIZE = 1000
    

def spawn(dllroot,outdir,interval,val_type,*symbols):   
    if val_type not in ['OHLCV','OHLC','CV','C']:
        raise ValueError("invalid val_type (OHLCV,OHLC,CV or C)") 
        
    # generate filename          
    dprfx = _strftime("%Y%m%d", _localtime())
     
    paths = {s.upper() : (_path(outdir) + '/' + dprfx + '_' \
                         + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
                         + '_' + val_type + '_' + str(interval) + 'min.tosdb') for s in symbols}

    # open file to write
    files = {s : open(s,'w',1) for s in paths}
    
    def callback_c(item, topic, iobj):
        f = files[item]        
        f.write(iobj.asctime().ljust(50))
        if not iobj.is_null():
            f.write(str(iobj.c) + '\n')

    def callback_ohlc(item, topic, iobj):
        f = files[item]        
        f.write(iobj.asctime().ljust(50))
        if not iobj.is_null():
            d = (iobj.open, iobj.high, iobj.low, iobj.close)
            f.write(str(str(d)) + '\n')

    class _callback_matcher
        def __init__(self, attrs)
            self.other_interval = dict()
            self.attrs = attrs
        def get(self, i):
            return (getattr(i, v) for v in self.attrs)
        def callback(self, item, topic, iobj):
            if other_interval[item] is None:
                other_interval[item] = (topic, iobj)
            else:
                if other_interval[item][0] == topic:
                    raise TOSDB_Error("last/volume mismatch")
                if other_interval[item][1].intervals_since_epoch \
                   != iobj.intervals_since_epoch:
                    raise TOSDB_Error("last/volume interval mismatch")
                f = files[item]        
                f.write(iobj.asctime().ljust(50))
                if not iobj.is_null():                    
                    d = (self.get(iobj), self.get(other_interval[item][1]))
                    if topic == 'VOLUME':                    
                        d = (d[1],d[0])
                    d = tuple(d[0]) + tuple(d[1])
                    f.write(str(d))
                other_interval[item].clear()

    callback_cv = _callback_matcher('c')
    callback_ohlcv = _callback_matcher('ohlc')
  
    # connect 
    tosdb.init(root=dllroot)
    
    # create block
    blk = tosdb.TOSDB_DataBlock(BLOCK_SIZE, date_time=True)
    blk.add_items(*(symbols))
    blk.add_topics('last')        
        
    # create interval object
    if 'OHLC' in val_type:
        if 'V' in val_type:
            blk.add_topics('volume')
            iobj = TOSDB_OpenHighLowCloseIntervals(blk, interval, interval_cb=callback_ohlcv)
        else:
            iobj = TOSDB_OpenHighLowCloseIntervals(blk, interval, interval_cb=callback_ohlc)
    else:
        if 'V' in val_type:
            blk.add_topics('volume')
            iobj = TOSDB_CloseIntervals(blk, interval, interval_cb=callback_cv)
        else:
            iobj = TOSDB_CloseIntervals(blk, interval, interval_cb=callback_c)

    ### TODO: HANDLE WAIT/EXIT
            

if __name__ == '__main__':
    parser = _ArgumentParser()  
    parser.add_argument('dllroot', type=str,                       
                        help = 'root path of Windows implementation C Library')
    parser.add_argument('outdir', type=str, help='directory to output data to')
    parser.add_argument('interval', type=int, help="interval size in seconds")
    parser.add_argument('--ohlc', action="store_true", 
                        help="use open/high/low/close instead of close")
    parser.add_argument('--vol', action="store_true", help="use volume")
    parser.add_argument('symbols', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    if args.ohlc:
        val_type = 'OHLCV' if args.vol else 'OHLC'
    else:
        val_type = 'CV' if args.vol else 'C'

    spawn(args.dllroot, args.outdir, args.interval, val_type, *arg.vars)






