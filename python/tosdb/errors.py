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

class TOSDB_Error( Exception ):
    """ Base exception for tosdb.py """    
    def __init__(self,  *messages ):        
        Exception( *messages )

class TOSDB_PlatformError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_CLibError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_ValueError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_DateTimeError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_DataError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_IndexError( TOSDB_Error, IndexError ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )
        IndexError( *messages )

class TOSDB_VirtError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

class TOSDB_VirtCommError( TOSDB_VirtError ):
    def __init__( self, *messages ):
        TOSDB_Error( messages )

    
