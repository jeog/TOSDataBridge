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

from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, strftime as _strftime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

CLS_BASE = 'GetOnTimeInterval_'
BLOCK_SIZE = 1000


def spawn(dllroot,outdir,intrvl,val_type,*symbols):   
    if val_type not in ['OHLCV','OHLC','CV','C']:
        raise ValueError("invalid val_type (OHLCV,OHLC,CV or C)")

    exc_cmd = "from tosdb.intervalize import " + CLS_BASE + val_type + " as _Goti"
    exec(exc_cmd, globals())
    
    tosdb.init(root=dllroot)
    
    # create block
    blk = tosdb.TOSDB_DataBlock(BLOCK_SIZE, date_time=True)
    blk.add_items(*(symbols))
    blk.add_topics('last')
    if 'V' in val_type:
        blk.add_topics('volume')
        
    # generate filename          
    dprfx = _strftime("%Y%m%d", _localtime())
    isec = int(intrvl * 60)  
    iobjs = list()

    for s in symbols:
        #
        # create GetOnTimeInterval object for each symbol
        # 
        p = _path(outdir) + '/' + dprfx + '_' \
            + s.replace('/','-S-').replace('$','-D-').replace('.','-P-') \
            + '_' + val_type + '_' + str(intrvl) + 'min.tosdb'       
        iobj = _Goti.send_to_file(blk, s, p, getattr(_TI,_TI.val_dict[isec]), isec/10)
        print( repr(iobj) )
        iobjs.append( iobj )
    
    return iobjs


if __name__ == '__main__':
    parser = _ArgumentParser()  
    parser.add_argument('dllroot', type=str,                       
                        help = 'root path of Windows implementation C Library')
    parser.add_argument('outdir', type=str, help='directory to output data to')
    parser.add_argument('intrvl', type=int, 
                        choices=tuple(map( lambda x: int(x/60),
                                           sorted(_TI.val_dict.keys()) )),
                        help="interval size(minutes)")
    parser.add_argument('--ohlc', action="store_true", 
                        help="use open/high/low/close instead of close")
    parser.add_argument('--vol', action="store_true", help="use volume")
    parser.add_argument('symbols', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    if args.ohlc:
        val_type = 'OHLCV' if args.vol else 'OHLC'
    else:
        val_type = 'CV' if args.vol else 'C'

    spawn(args.dllroot, args.outdir, args.intrvl, val_type, *arg.vars)






