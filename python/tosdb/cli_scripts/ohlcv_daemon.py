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
from tosdb import ohlc
from .daemon import Daemon as _Daemon

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

_val_type = ''

AINIT_TIMEOUT = 5000
BLOCK_SIZE = 1000
    
class MyDaemon(_Daemon):
    def __init__(self, addr, dll_root, out_dir, pid_file, error_file, 
                 interval, symbols):
        _Daemon.__init__(self, pid_file, stderr = error_file)
        self._addr = addr
        self._dll_root = dll_root
        self._out_dir = _path(out_dir)
        self._interval = interval
        self._symbols = symbols        


    def run(self):
        # generate filename          
        dprfx = _strftime("%Y%m%d", _localtime())
         
        paths = {s.upper() : (_path(outdir) + '/' + dprfx + '_' \
                             + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
                             + '_' + _val_type + '_' + str(self._interval) + 'min.tosdb') \
                 for s in self._symbols}

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
    
        # initialize windows side   
        tosdb.admin_init(self._addr, AINIT_TIMEOUT)   
        tosdb.vinit(root=self._dll_root)

        # create block
        blk = tosdb.VTOSDB_DataBlock(self._addr, BLOCK_SIZE, date_time=True)
        blk.add_items(*(self._symbols))
        blk.add_topics('last')
    
        # create interval object
        if 'OHLC' in _val_type:
            if 'V' in _val_type:
                blk.add_topics('volume')
                iobj = TOSDB_OpenHighLowCloseIntervals(blk, interval, interval_cb=callback_ohlcv)
            else:
                iobj = TOSDB_OpenHighLowCloseIntervals(blk, interval, interval_cb=callback_ohlc)
        else:
            if 'V' in _val_type:
                blk.add_topics('volume')
                iobj = TOSDB_CloseIntervals(blk, interval, interval_cb=callback_cv)
            else:
                iobj = TOSDB_CloseIntervals(blk, interval, interval_cb=callback_c)
                
        ### TODO: HANDLE WAIT/EXIT
      

if __name__ == '__main__':
    parser = _ArgumentParser() 
    parser.add_argument('addr', type=str, 
                        help = 'address of Windows implementation "address port"')
    parser.add_argument('dllroot', type=str, 
                        help = 'root path of Windows implementation C Library')
    parser.add_argument('outdir', type=str, help = 'directory to output data to')
    parser.add_argument('pidfile', type=str, help = 'path of pid file')
    parser.add_argument('errorfile', type=str, help = 'path of error file')
    parser.add_argument('interval', type=int, 
                        choices=tuple(map( lambda x: int(x/60),
                                           sorted(_TI.val_dict.keys()) )),
                        help="interval size(minutes)" )
    parser.add_argument('--ohlc', action="store_true", 
                        help="use open/high/low/close instead of close")
    parser.add_argument('--vol', action="store_true", help="use volume")
    parser.add_argument('vars', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    addr_parts = args.addr.split(' ')
    addr = (addr_parts[0],int(addr_parts[1]))
    
    if args.ohlc:
        _val_type = 'OHLCV' if args.vol else 'OHLC'
    else:
        _val_type = 'CV' if args.vol else 'C'


    MyDaemon(addr, args.dllroot, args.outdir, args.pidfile, args.errorfile,
             args.interval, args.vars).start()




