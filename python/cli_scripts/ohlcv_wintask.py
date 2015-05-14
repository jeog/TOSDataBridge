import tosdb
from tosdb.intervalize import TimeInterval as _TI
from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, sleep as _sleep
from os.path import realpath as _path
from sys import stderr as _stderr

_cls_base = 'GetOnTimeInterval_'
_TIMEOUT = 5000
TI_VALS = ( _TI.min.val, _TI.three_min.val, _TI.five_min.val, _TI.ten_min.val,
            _TI.fifteen_min.val, _TI.thirty_min.val, _TI.hour.val )

def spawn(dllroot,outdir,intrvl,val_type,*symbols):
   
    if val_type not in ['OHLCV','OHLC','CV','C']:
         raise ValueError("invalid val_type (OHLCV,OHLC,CV or C)")

    exec( "from tosdb.intervalize import " + _cls_base + val_type + " as _Goti",
          globals() )
    
    tosdb.init( root=dllroot )
    
    # create block
    blk = tosdb.TOSDB_DataBlock( 10000, date_time=True)
    blk.add_items( *(symbols) )
    blk.add_topics( 'last' )
    if 'V' in val_type:
        blk.add_topics( 'volume' )
        
    # generate filename             
    lt = _localtime()
    dprfx = str(lt.tm_year) + str(lt.tm_mon) + str(lt.tm_mday) 
    isec = int(intrvl * 60)
    
    iobjs = list()
    for s in symbols:
        #
        # create GetOnTimeInterval object for each symbol
        # 
        p = _path(outdir) + '/' + dprfx + '_' + \
                s.replace('/','-').replace('$','-') + '_' + \
                val_type + '_' + str(intrvl) + 'min.tosdb'           
        iobj = _Goti.send_to_file( blk, s, p, getattr(_TI,_TI.val_dict[ isec ]),
                                   isec/10)
        print( repr(iobj) )
        iobjs.append( iobj )
    
    return iobjs

if __name__ == '__main__':
    parser = _ArgumentParser()    
    parser.add_argument( 'dllroot', type=str, help = 'root of the path of core'
                         '(Windows) implementation DLL' )
    parser.add_argument( 'outdir', type=str, help = 'directory to output ' +
                         ' data/files to' )
    parser.add_argument( 'intrvl', type=int, 
                         choices=tuple( map( lambda x: int(x/60),
                                             sorted(_TI.val_dict.keys()) ) ),
                         help="interval size(minutes)" )
    parser.add_argument( '--ohlc', action="store_true", 
                         help="use open,high,low,close instead of close" )
    parser.add_argument( '--vol', action="store_true", help="use volume")
    parser.add_argument('symbols', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    if args.ohlc:
        val_type = 'OHLCV' if args.vol else 'OHLC'
    else:
        val_type = 'CV' if args.vol else 'C'

    spawn(args.dllroot, args.outdir, args.intrvl, val_type, *arg.vars)






