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

"""intervalize/constant_value.py:  structure streaming tosdb data in constant-value intervals

*** IN DEVELOPMENT ***

Constant value intervals break data up based on some constant value being reached during
the interval. For instance, constant volume intervals create a new interval each time
the underlying volume of the item reaches a certain fixed amount for that interval:

tradional fixed-time interval:

last:    22    23    21    22    23    24    27   28   30
bid:     21    23    21    21    22    23    26   28   29
volume:  700   1300  900   200   400   500   0    100  2000
index:   1     2     3     4     5     6     7    8    9

2000 share constant-volume interval:

last:           23                     24              30
bid:            23                     23              29
volume:         2000                   2000            2000
index:          1                      2               3

*** IN DEVELOPMENT ***
"""
