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
from tosdb.intervalize.ohlc import TOSDB_OpenHighLowCloseIntervals as OHLCIntervals, \
                                   TOSDB_CloseIntervals as CIntervals

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

BLOCK_SIZE = 1000
_paths = dict()

def spawn(outdir, interval, is_ohlc, has_vol, *symbols):
    global _paths
    itype = ("OHLC" if is_ohlc else "C") + ("V" if has_vol else "")    
    # generate filename          
    dprfx = _strftime("%Y%m%d", _localtime())
    # generate paths from filenames
    _paths = {s.upper() : (_path(outdir) + '/' + dprfx + '_' \
                          + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
                          + '_' + itype + '_' + str(interval) + 'sec.tosdb')
                          for s in symbols}  
    # create callback objects
    if has_vol:
        callback = _callback_matcher('ohlc' if is_ohlc else 'c')
    else:
        l = lambda o: str((o.o, o.h, o.l, o.c)) if is_ohlc else lambda o: str(o.c)  
        callback = _callback_basic(l)     
    # create block
    blk = tosdb.TOSDB_ThreadSafeDataBlock(BLOCK_SIZE, date_time=True)
    blk.add_items(*(symbols))
    blk.add_topics('last')        
    if has_vol:
        blk.add_topics('volume')       
    # create interval object
    IObj = OHLCIntervals if is_ohlc else CIntervals
    iobj = IObj(blk, interval, interval_cb=callback.callback)
    try:
        while iobj.running():
            _sleep(1)
    except:
        iobj.stop()
    finally:
        blk.close()
        tosdb.clean_up()
            

def _write(item, s):
    with open(_paths[item], 'a') as f:
        f.write(s)

class _callback_basic:
    def __init__(self, to_str_func):
        self._to_str_func = to_str_func
    def callback(self, item, topic, iobj):
        _write(item, iobj.asctime().ljust(50))       
        s = self._to_str_func(iobj) if not iobj.is_null() else 'N/A'
        _write(item, s + '\n')           
    
class _callback_matcher:
    TTOGGLE = {"LAST":"VOLUME", "VOLUME":"LAST"}
    def __init__(self, attrs):
        self._other_interval = dict()
        self._props = attrs       
    def callback(self, item, topic, iobj):            
        if item not in self._other_interval:
            self._other_interval[item] = {"LAST":dict(), "VOLUME":dict()}
        ise = iobj.intervals_since_epoch
        otopic = self.TTOGGLE[topic]
        if ise in self._other_interval[item][otopic]:
            _write(item, iobj.asctime().ljust(50))
            if not iobj.is_null():
                m = self._other_interval[item][otopic][ise]                        
                if topic == 'VOLUME':
                    d = tuple((getattr(m, v) for v in self._props)) + (iobj.c,)
                else:
                    d = tuple((getattr(iobj, v) for v in self._props)) + (m.c,)                                  
                _write(item, str(d) + '\n')
            else:
                _write(item, 'N/A \n')                
            self._other_interval[item][otopic].pop(ise)
        else:
            self._other_interval[item][topic][ise] = iobj
            

if __name__ == '__main__':
    parser = _ArgumentParser()  
    parser.add_argument('--root', help='root directory to search for the library')
    parser.add_argument('--path', help='the exact path of the library')
    parser.add_argument('outdir', type=str, help='directory to output data to')
    parser.add_argument('interval', type=int, help="interval size in seconds")
    parser.add_argument('--ohlc', action="store_true", 
                        help="use open/high/low/close instead of close")
    parser.add_argument('--vol', action="store_true", help="use volume")
    parser.add_argument('symbols', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    if not args.path and not args.root:
        print("need --root or --path argument", file=_stderr)
        exit(1)

    # connect        
    tosdb.init(args.path,args.root)    
    spawn(args.outdir, args.interval, bool(args.ohlc), bool(args.vol), *args.symbols)






