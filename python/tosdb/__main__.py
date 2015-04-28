from tosdb import *
from tosdb import _SYS_IS_WIN, _Thread
from argparse import ArgumentParser as _ArgumentParser

parser = _ArgumentParser()
parser.add_argument( '--virtual-client', type=str,
                     help='initialize virtual client "address port [timeout]"')
parser.add_argument( '--virtual-server', type=str,
                     help='start virtual server "address port [timeout]"' )
parser.add_argument( '--root', 
                     help = 'root directory to search for the library' )
parser.add_argument( '--path', help='the exact path of the library' )
args = parser.parse_args()

if args.virtual_server and _SYS_IS_WIN:
    raw_args = args.virtual_server.split(' ')
    vs_args = ((raw_args[0],int(raw_args[1])),)
    vs_args += (int(raw_args[2]),) if len(raw_args) > 2 else ()  
    _Thread( target=enable_virtualization, args=vs_args).start()
    
if args.virtual_client:
    raw_args = args.virtual_client.split(' ')
    vc_args = ((raw_args[0],int(raw_args[1])),)
    vc_args += (int(raw_args[2]),) if len(raw_args) > 2 else ()
    admin_init( *vc_args )
              
if args.path:
    vinit(dllpath=args.path) if args.virtual_client else init(dllpath=args.path)
elif args.root:
    vinit(root=args.root) if args.virtual_client else init(root=args.root)
   
