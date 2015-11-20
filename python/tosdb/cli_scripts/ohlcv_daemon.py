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
from tosdb.intervalize import TimeInterval as _TI
from .daemon import Daemon as _Daemon

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

_val_type = ''

CLS_BASE = 'GetOnTimeInterval_'
AINIT_TIMEOUT = 5000
BLOCK_SIZE = 1000
    
class MyDaemon(_Daemon):
  def __init__(self, addr, dll_root, out_dir, pid_file, error_file, 
               intrvl, symbols):
    _Daemon.__init__(self, pid_file, stderr = error_file)
    self._addr = addr
    self._dll_root = dll_root
    self._out_dir = _path(out_dir)
    self._intrvl = intrvl
    self._symbols = symbols
    self._iobjs = list()

  def run(self):  
    # initialize windows side   
    tosdb.admin_init( self._addr, AINIT_TIMEOUT )   
    tosdb.vinit( root=self._dll_root )

    # create block
    blk = tosdb.VTOSDB_DataBlock( self._addr, BLOCK_SIZE, date_time=True)
    blk.add_items( *(self._symbols) )
    blk.add_topics( 'last' )
    if args.vol:
        blk.add_topics( 'volume' )
 
    # generate filename             
    dprfx = _strftime("%Y%m%d", _localtime())
    isec = int(self._intrvl * 60)  
    for s in self._symbols:
      #
      # create GetOnTimeInterval object for each symbol
      # 
      p = self._out_dir + '/' + dprfx + '_' \
          + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
          +'_' + _val_type + '_' + str(self._intrvl) + 'min.tosdb'           
      iobj = _Goti.send_to_file(blk, s, p, _TI.vals[isec], isec/10)
      print(repr(iobj), file=_stderr)
      self._iobjs.append(iobj)

    for i in self._iobjs:
      if i:
        i.join()

    while(True):
      _sleep(10)
      

if __name__ == '__main__':
  parser = _ArgumentParser() 
  parser.add_argument('addr', type=str, 
                      help = 'address of Windows implementation "address port"')
  parser.add_argument('dllroot', type=str, 
                      help = 'root path of Windows implementation C Library')
  parser.add_argument('outdir', type=str, help = 'directory to output data to')
  parser.add_argument('pidfile', type=str, help = 'path of pid file')
  parser.add_argument('errorfile', type=str, help = 'path of error file')
  parser.add_argument('intrvl', type=int, 
    choices=tuple(map(lambda x: int(x/60), sorted(_TI.val_dict.keys()))),
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

  exc_cmd = "from tosdb.intervalize import " + CLS_BASE + _val_type + " as _Goti"
  exec(exc_cmd)  

  MyDaemon(addr, args.dllroot, args.outdir, args.pidfile, args.errorfile,
           args.intrvl, args.vars).start()




