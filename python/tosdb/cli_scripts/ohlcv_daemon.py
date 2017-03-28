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

from tosdb.cli_scripts.daemon import Daemon as _Daemon

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

AINIT_TIMEOUT = 5000
BLOCK_SIZE = 1000
    

class MyDaemon(_Daemon):
    def __init__(self, addr, auth, outdir, pidfile, errorfile, interval, itype, symbols):
        _Daemon.__init__(self, pidfile, stderr = errorfile)
        self._addr = addr    
        self._auth = auth
        self._outdir = _path(outdir)
        self._interval = interval
        self._itype = itype
        self._symbols = symbols        
        # generate filename          
        dprfx = _strftime("%Y%m%d", _localtime())
        # generate paths from filenames
        self._paths = {s.upper() : (_path(self._outdir) + '/' + dprfx + '_' \
                                   + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
                                   + '_' + self._itype + '_' + str(self._interval) + 'sec.tosdb') \
                                     for s in self._symbols}
        # create callback objects
        self._callback_c = self._callback_basic(lambda o: str(o.c), self)
        self._callback_ohlc = self._callback_basic(lambda o: str((o.o, o.h, o.l, o.c)), self)
        self._callback_cv = self._callback_matcher('c',self)
        self._callback_ohlcv = self._callback_matcher('ohlc', self)
        self._iobj = None


    def run(self):
        # create block
        blk = tosdb.VTOSDB_ThreadSafeDataBlock(self._addr, self._auth, BLOCK_SIZE, date_time=True)
        blk.add_items(*(self._symbols))
        blk.add_topics('last')
        if 'V' in self._itype:
            blk.add_topics('volume')       
        # create interval object
        if 'OHLC' in self._itype:
            if 'V' in self._itype:         
                self._iobj = OHLCIntervals(blk, self._interval, interval_cb=self._callback_ohlcv.callback)
            else:
                self._iobj = OHLCIntervals(blk, self._interval, interval_cb=self._callback_ohlc.callback)
        else:
            if 'V' in self._itype:           
                self._iobj = CIntervals(blk, self._interval, interval_cb=self._callback_cv.callback)
            else:
                self._iobj = CIntervals(blk, self._interval, interval_cb=self._callback_c.callback)
        
        while self._iobj.running():
            _sleep(1)
        


    def _write(self, item, s):
        with open(self._paths[item], 'a') as f:
            f.write(s)

    class _callback_basic:
        def __init__(self, to_str_func, parent):
            self._to_str_func = to_str_func
            self._parent = parent
        def callback(self, item, topic, iobj):
            self._parent._write(item, iobj.asctime().ljust(50))       
            s = self._to_str_func(iobj) if not iobj.is_null() else 'N/A'
            self._parent._write(item, s + '\n')           
        
    class _callback_matcher:
        TTOGGLE = {"LAST":"VOLUME", "VOLUME":"LAST"}
        def __init__(self, attrs, parent):
            self._other_interval = dict()
            self._props = attrs       
            self._parent = parent
        def callback(self, item, topic, iobj):            
            if item not in self._other_interval:
                self._other_interval[item] = {"LAST":dict(), "VOLUME":dict()}
            ise = iobj.intervals_since_epoch
            otopic = self.TTOGGLE[topic]
            if ise in self._other_interval[item][otopic]:
                self._parent._write(item, iobj.asctime().ljust(50))
                if not iobj.is_null():
                    m = self._other_interval[item][otopic][ise]                        
                    if topic == 'VOLUME':
                        d = tuple((getattr(m, v) for v in self._props)) + (iobj.c,)
                    else:
                        d = tuple((getattr(iobj, v) for v in self._props)) + (m.c,)                                  
                    self._parent._write(item, str(d) + '\n')
                else:
                    self._parent._write(item, 'N/A \n')                
                self._other_interval[item][otopic].pop(ise)
            else:
                self._other_interval[item][topic][ise] = iobj
      

if __name__ == '__main__':
    parser = _ArgumentParser() 
    parser.add_argument('addr', type=str, 
                        help = 'address of the host system "address port"')
    parser.add_argument('--root', help='root directory to search for the library (on host)')
    parser.add_argument('--path', help='the exact path of the library (on host)')
    parser.add_argument('--auth', help='password to use if authentication required')
    parser.add_argument('outdir', type=str, help = 'directory to output data to')
    parser.add_argument('pidfile', type=str, help = 'path of pid file')
    parser.add_argument('errorfile', type=str, help = 'path of error file')
    parser.add_argument('interval', type=int, help="interval size in seconds")
    parser.add_argument('--ohlc', action="store_true", 
                        help="use open/high/low/close instead of close")
    parser.add_argument('--vol', action="store_true", help="use volume")
    parser.add_argument('symbols', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    addr_parts = args.addr.split(' ')
    addr = (addr_parts[0],int(addr_parts[1]))
    
    if args.ohlc:
        itype = 'OHLCV' if args.vol else 'OHLC'
    else:
        itype = 'CV' if args.vol else 'C'

    if not args.path and not args.root:
        print("need --root or --path argument", file=_stderr)
        exit(1)

    # connect              
    tosdb.admin_init(addr, password=args.auth, timeout=AINIT_TIMEOUT)   
    tosdb.vinit(args.path, args.root)
    
    MyDaemon(addr, args.auth, args.outdir, args.pidfile, args.errorfile, 
             args.interval, itype, args.symbols).start()




