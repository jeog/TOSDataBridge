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
from argparse import ArgumentParser as _ArgumentParser
from sys import stderr as _stderr
from random import choice as _rchoice
from time import sleep as _sleep

args=None
T_ITEMS = ['SPY','GOOG','QQQ']
T_TOPICS = ['LAST','LASTX','VOLUME','LAST_SIZE','PUT_CALL_RATIO']
bsize = 100

def init():
  global args
  parser = _ArgumentParser()
  parser.add_argument('build', type=str, help='build: x86 or x64')
  args = parser.parse_args()

  if args.build not in ('x86','x64'):
      print('invalid build argument', file=_stderr)
      print('usage: tosdb_test.py [x86|x64]', file=_stderr)
      return

  bdir = "../bin/Release/" + ("Win32" if args.build == 'x86' else "x64")
  if not tosdb.init(root=bdir):
      print("--- INIT FAILED ---", file=_stderr)
      return False
  print()
  return True

def test():   
  global bsize
  _ladj = lambda s : s.ljust(20)
  
  print(_ladj("CONNECTED:"), tosdb.connected() )
  blim = tosdb.get_block_limit()
  
  print(_ladj("GET BLOCK LIMIT:"), blim )
  print(_ladj("HALVE BLOCK LIMIT:"))
  tosdb.set_block_limit(int(blim/2))
  print(_ladj("GET BLOCK LIMIT:"), tosdb.get_block_limit() )
  
  print("\n",_ladj("TOPIC BITS/STRINGS:"))
  for t in T_TOPICS:
      print(_ladj(t), str(tosdb.type_bits(t)).ljust(4),
            _ladj(tosdb.type_string(t)))

  print("\nCREATE BLOCK:")
  b1 = tosdb.TOSDB_DataBlock(bsize,True,2000)
  for i,v in b1.info().items():
    print(_ladj(i), v)
  
  bsize = b1.get_block_size()
  print("\n",_ladj("GET BLOCK SIZE"), bsize)
  bsize *= 2
  print(_ladj("DOUBLE BLOCK SIZE"))
  b1.set_block_size(bsize)
  print(_ladj("GET BLOCK SIZE"), b1.get_block_size())
  
  print('\n',_ladj("ADD ITEMS & TOPICS:"))
  b1.add_items(*T_ITEMS)
  b1.add_topics(*T_TOPICS)
  print(_ladj("ITEMS:"), b1.items())
  print(_ladj("TOPICS:"), b1.topics())
  if sorted(b1.items()) != sorted(T_ITEMS):
    print("--- ITEMS DON'T MATCH ---", file=_stderr)
  if sorted(b1.topics()) != sorted(T_TOPICS):
    print("--- TOPICS DON'T MATCH ---", file=_stderr)
  rt = T_TOPICS.pop()
  
  print(_ladj(("REMOVE TOPIC " + rt)))
  b1.remove_topics(rt)
  if sorted(b1.topics()) != sorted(T_TOPICS):
    print("--- TOPICS DON'T MATCH ---", file=_stderr)
  else:
    print(_ladj("TOPICS:"), b1.topics())
  ri = T_ITEMS.pop()
  print(_ladj(("REMOVE ITEM " + ri)))
  b1.remove_items(ri)
  if sorted(b1.items()) != sorted(T_ITEMS):
    print("--- ITEMS DON'T MATCH ---", file=_stderr)
  else:
    print(_ladj("ITEMS:"), b1.items())
    
  print("\nSLEEP FOR 5 SECONDS TO GET SOME DATA")
  _sleep(5)
  
  print("\nGET (RANDOM):")  
  for i in range(0,10,2):
      ii = _rchoice(T_ITEMS)
      tt = _rchoice(T_TOPICS)
      dd = _rchoice([1,0])      
      print(_ladj(ii),_ladj(tt),_ladj(str(dd)),end=' ')
      try:
        v = b1.get(ii,tt,dd,i)
        print(str(v))
      except tosdb._common.TOSDB_DataError:
        print("no data yet")           
    
  
  ii = _rchoice(T_ITEMS)
  tt = "LAST"  
  print("\nSTREAM SNAPSHOT:", ii,tt,'date_time=True','end=3')
  vdt = b1.stream_snapshot(ii,tt,True,3)
  for k in vdt:
      print(str(k[1]),str(k[0]))
  print("\nSTREAM SNAPSHOT:", ii,tt,'date_time=False','end=10')
  vdt = b1.stream_snapshot(ii,tt,False,10)
  print(str(vdt))

  print("\nSTREAM SNAPSHOT FROM MARKER:",ii,tt,
        'date_time=True','beg = 0')
  for i in range(3):
    print(str(i+1), "(sleep 2 second(s))")
    _sleep(2)
    ssfm = b1.stream_snapshot_from_marker(ii,tt,True, 0)
    if(ssfm):
      for k in ssfm:
        print(str(k[1]), str(k[0]))   

  print("\nITEM FRAME:",tt,'date_time=False')
  print(b1.item_frame(tt))
  
  print("\nTOPIC FRAME:",ii,'date_time=False')
  print(b1.topic_frame(ii))

  print("\nTOTAL FRAME:",ii,'date_time=False')
  print(b1.total_frame())
  
  print()

  
if __name__ == '__main__':
    if init():
        test()
