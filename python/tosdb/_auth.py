from ._common import _recvall_tcp, _recv_tcp, _send_tcp, TOSDB_VirtualizationError, TOSDB_Error
import socket as _socket
from os import urandom as _urandom
from importlib import find_loader as _find_loader
      
MAX_PASSWORD = 128
MIN_PASSWORD = 12
RAND_SEQ_SZ = 512

_vAUTH_SUCCESS = 'AUTH_SUCCESS'
_vAUTH_FAILURE = 'AUTH_FAILURE'

# hold of on importing pycrypto as late as possible
# don't force dependency outside the std lib unless it's needed
_AES=None
_SHA256=None
def try_import_pycrypto():
    global _AES, _SHA256
    try:
        from Crypto.Hash import SHA256
        from Crypto.Cipher import AES        
        _AES = AES
        _SHA256 = SHA256        
    except ImportError:
        raise TOSDB_VirtualizationError(
            "could not import Crypto (be sure pycrypto package is installed)")


def do_i_have_pycrypto():
    return bool( _find_loader('Crypto') )
        
    
def check_password(p):
    plen = len(p)
    if plen < MIN_PASSWORD:
        raise ValueError("password length < MIN_PASSWORD")
    if plen > MAX_PASSWORD:
        raise ValueError("password length > MAX_PASSWORD")


def hash_password(p):
    Hasher = _SHA256.new()  
    Hasher.update(p.encode())
    return Hasher.digest()


def handle_auth_cli(my_sock,password):
    #hash/overwite password
    password = hash_password(password)
    # recv random sequence from server 
    try:
        rand_seq_iv = _recv_tcp(my_sock) #raw (no need to unpack)
        iv = rand_seq_iv[:_AES.block_size]
        rand_seq = rand_seq_iv[_AES.block_size:]        
    except _socket.timeout as e:
        raise TOSDB_VirtualizationError("socket timed out receiving random sequence")         

    if len(rand_seq) != RAND_SEQ_SZ:
        raise TOSDB_VirtualizationError("invalid size of random sequence")
    
    # encrypt random sequence using hashed password, attached iv    
    crypt_rand_seq = _AES.new(password, _AES.MODE_CFB, iv).encrypt(rand_seq)
    # send that back to server
    _send_tcp(my_sock, crypt_rand_seq) #raw (no need to pack) 
    # see if it worked
    try:
        ret = _recv_tcp(my_sock)
        ret = ret.decode()   
    except _socket.timeout as e:
        raise TOSDB_VirtualizationError("socket timed out receiving status")        
    return (ret == _vAUTH_SUCCESS)


def handle_auth_serv(my_conn,password):
    my_sock, my_addr = my_conn
    #hash/overwite password
    password = hash_password(password)
    # generate random sequence and iv
    rseq = _urandom(RAND_SEQ_SZ)    
    iv = _urandom(_AES.block_size)
    
    # send to client 
    _send_tcp(my_sock, iv+rseq)
    
    # get back the encrypted version
    try:
        crypt_seq_cli = _recv_tcp(my_sock) #raw (no need to unpack)
    except ConnectionAbortedError as e:
        raise TOSDB_VirtualizationError("client aborted connection", e)
    except _socket.timeout:
        raise TOSDB_VirtualizationError("socket timed out receive encrypted random sequence")
    
    if crypt_seq_cli is None:
        raise TOSDB_VirtualizationError("client failed to return encrypted sequence")

    seq_cli = _AES.new(password, _AES.MODE_CFB, iv).decrypt(crypt_seq_cli)      
    seq_match = (rseq == seq_cli)
    try:
        rmsg = _vAUTH_SUCCESS if seq_match else _vAUTH_FAILURE
        _send_tcp(my_sock, rmsg.encode())       
    except BaseException as e:
        raise TOSDB_VirtualizationError("failed to send success/failure to client", e)

    return seq_match

