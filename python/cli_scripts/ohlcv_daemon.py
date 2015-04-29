import tosdb
from tosdb.intervalize import TimeInterval as _TI
from argparse import ArgumentParser as _ArgumentParser
from time import localtime as _localtime, sleep as _sleep
from os.path import realpath as _path
from daemon import Daemon as _Daemon

_val_type = ''
_cls_base = 'GetOnTimeInterval_'
    
class MyDaemon(_Daemon):
    def __init__(self, addr, dll_root, out_dir, intrvl , symbols):
        _Daemon.__init__( self,"/tmp/ohlcv_daemon.pid", 
                          stderr = '/home/jon/dev/TOSDataBridge/python/' +
                                   'cli_scripts/err_file.txt')
        self._addr = addr
        self._dll_root = dll_root
        self._out_dir = _path(out_dir)
        self._intrvl = intrvl
        self._symbols = symbols
        self._iobjs = list()

    def run(self):  
        # initialize windows side     
        tosdb.admin_init( self._addr )   
        tosdb.vinit( root=self._dll_root )
        # create block
        blk = tosdb.VTOSDB_DataBlock( self._addr, 10000, date_time=True)
        blk.add_items( *(self._symbols) )
        blk.add_topics( 'last' )
        if args.vol:
            blk.add_topics( 'volume' ) 
        # generate filename             
        lt = _localtime()
        dprfx = str(lt.tm_year) + str(lt.tm_mon) + str(lt.tm_mday) 
        isec = int(self._intrvl * 60)    
        
        for s in self._symbols:
            #
            # create GetOnTimeInterval object for each symbol
            # 
            p = self._out_dir + '/' + dprfx + '_' + s + '_' + _val_type \
                + '_' + str(self._intrvl) + 'min.tosdb'           
            iobj = _Goti.send_to_file( blk, s, p, _TI.vals[ isec ], isec/10)
            self._iobjs.append( iobj )

        for i in self._iobjs:
            i.join()


if __name__ == '__main__':
    parser = _ArgumentParser() 
    parser.add_argument( 'addr', type=str, help = 'address of core(Windows) ' +
                         'implementation " address port "' )
    parser.add_argument( 'dllroot', type=str, help = 'root of the path of core'
                         '(Windows) implementation DLL' )
    parser.add_argument( 'outdir', type=str, help = 'directory to output ' +
                         ' data/files to' )
    parser.add_argument( 'intrvl', type=int, 
                         choices=tuple (map( lambda x: int(x/60), 
                                             sorted(_TI.vals) ) ),
                         help="interval size(minutes)" )
    parser.add_argument( '--ohlc', action="store_true", 
                         help="use open,high,low,close instead of close" )
    parser.add_argument( '--vol', action="store_true", help="use volume")
    parser.add_argument('vars', nargs='*', help="symbols to pull")
    args = parser.parse_args()

    addr_parts = args.addr.split(' ')
    addr = (addr_parts[0],int(addr_parts[1]))
    
    if args.ohlc:
        _val_type = 'OHLCV' if args.vol else 'OHLC'
    else:
        _val_type = 'V' if args.vol else 'C'

    exec("from tosdb.intervalize import " + _cls_base + _val_type + " as _Goti")    

    MyDaemon( addr, args.dllroot, args.outdir, args.intrvl, args.vars).start()




