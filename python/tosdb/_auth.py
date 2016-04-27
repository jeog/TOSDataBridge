from Crypto.Hash import SHA256 as _SHA256
from Crypto.Cipher import AES as _AES
from Crypto import Random as _Random
      
MAX_PASSWORD = 30
MIN_PASSWORD = 10
RAND_SEQ_SZ = 512

_Hasher = _SHA256.new()
_iv = 'a' * _AES.block_size

def hash_password(p):
    _Hasher.update(p)
    return _Hasher.digest()

def encrypt_randseq(hp, rseq):    
    _Cipher = _AES.new(hp, _AES.MODE_CFB, _iv)
    ct = _Cipher.encrypt(rseq)
    return ct

def generate_randseq():
    r = _Random.new()
    return r.read(RAND_SEQ_SZ)
