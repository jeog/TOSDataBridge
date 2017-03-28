# Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >
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


TTOGGLE = {"LAST":"VOLUME", "VOLUME":"LAST"}


class _Basic:
    def __init__(self, to_str_func, write_func):
        self._to_str_func = to_str_func
        self._write_func = write_func

    def callback(self, item, topic, iobj):
        self._write_func(item, iobj.asctime().ljust(50))       
        s = self._to_str_func(iobj) if not iobj.is_null() else 'N/A'
        self._write_func(item, s + '\n')           
      
        
class _Matcher:    
    def __init__(self, attrs, write_func):
        self._other_interval = dict()
        self._props = attrs       
        self._write_func = write_func

    def callback(self, item, topic, iobj):            
        if item not in self._other_interval:
            self._other_interval[item] = {"LAST":dict(), "VOLUME":dict()}
        ise = iobj.intervals_since_epoch
        otopic = TTOGGLE[topic]
        if ise in self._other_interval[item][otopic]:
            self._write_func(item, iobj.asctime().ljust(50))
            if not iobj.is_null():
                m = self._other_interval[item][otopic][ise]                        
                if topic == 'VOLUME':
                    d = tuple((getattr(m, v) for v in self._props)) + (iobj.c,)
                else:
                    d = tuple((getattr(iobj, v) for v in self._props)) + (m.c,)                                  
                self._write_func(item, str(d) + '\n')
            else:
                self._write_func(item, 'N/A \n')                
            self._other_interval[item][otopic].pop(ise)
        else:
            self._other_interval[item][topic][ise] = iobj


