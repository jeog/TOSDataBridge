from Crypto.Hash import SHA256 as _SHA256
from Crypto.Cipher import AES as _AES
from Crypto import Random as _Random

from ._common import _recvall_tcp, _recv_tcp, _send_tcp, TOSDB_VirtualizationError
import socket as _socket
      
DEFAULT_PASSWORD = "default_pass"
MAX_PASSWORD = 30
MIN_PASSWORD = 10
RAND_SEQ_SZ = 512
IV_SZ = _AES.block_size

_AUTH_SUCCESS = 'SUCCESS'
_AUTH_FAILURE = 'FAILURE'


def check_password(p):
    plen = len(p)
    if plen < MIN_PASSWORD:
        raise ValueError("password length < MIN_PASSWORD")
    if plen > MAX_PASSWORD:
        raise ValueError("password length > MAX_PASSWORD")

def hash_password(p):
    Hasher = _SHA256.new()  
    Hasher.update(p)
    return Hasher.digest()

def encrypt_randseq(hp, rseq, iv):      
    _Cipher = _AES.new(hp, _AES.MODE_CFB, iv)
    ct = _Cipher.encrypt(rseq)
    return ct

def generate_randseq():
    r = _Random.new()
    return r.read(RAND_SEQ_SZ)

def generate_iv():
    return _Random.new().read(IV_SZ)

def handle_auth_cli(my_sock,password):
    # hash password
    hashed_pass = hash_password(password)
    # recv random sequence from server 
    try:
        rand_seq_iv = _recv_tcp(my_sock) #raw (no need to unpack)
        iv = rand_seq_iv[:IV_SZ]
        rand_seq = rand_seq_iv[IV_SZ:]        
    except _socket.timeout as e:
        raise TOSDB_VirtualizationError("socket timed out receiving random sequence")         
    # check size of rand_seq
    if len(rand_seq) != RAND_SEQ_SZ:
      raise TOSDB_VirtualizationError("rand_seq != RAND_SEQ_SZ")
    # encrypt random sequence using hashed password 
    crypt_rand_seq = encrypt_randseq(hashed_pass, rand_seq, iv)   
    # send that back to server
    _send_tcp(my_sock, crypt_rand_seq) #raw (no need to pack) 
    # see if it worked
    try:
      ret = _recv_tcp(my_sock)
      ret = ret.decode()   
    except _socket.timeout as e:
      raise TOSDB_VirtualizationError("socket timed out receiving status")        
    return (ret == _AUTH_SUCCESS)

def handle_auth_serv(my_conn,password):
    my_sock, my_addr = my_conn
    # generate random sequence and iv
    rseq = generate_randseq()    
    iv = generate_iv()
    # send to client 
    _send_tcp(my_sock, iv+rseq)
    # get back the encrypted version
    try:
        crypt_seq_cli = _recv_tcp(my_sock) #raw (no need to unpack)
    except _socket.timeout as e:
        raise TOSDB_VirtualizationError("socket timed out receive encrypted random sequence") 
    hashed_pass = hash_password(password)
    our_crypt_seq = encrypt_randseq(hashed_pass, rseq, iv)
    # compare ours w/ the clients
    if our_crypt_seq == crypt_seq_cli:  
        print("authentication attempt suceeded!", my_addr)
        _send_tcp(my_sock, _AUTH_SUCCESS.encode())
        return True  
    print("authentication attempt failed!", my_addr)
    _send_tcp(my_sock, _AUTH_FAILURE.encode())
    return False

