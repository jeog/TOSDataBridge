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

import sys as _sys

class TOSDB_Error( Exception ):
    """ Base exception for tosdb.py """    
    def __init__(self,  *messages ):        
        Exception( *messages )

class TOSDB_PlatformError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_CLibError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_DateTimeError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_DataError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_ValueError( TOSDB_Error, ValueError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        ValueError( *messages )

class TOSDB_TypeError( TOSDB_Error, TypeError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        TypeError( *messages )

class TOSDB_IndexError( TOSDB_Error, IndexError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        IndexError( *messages )

class TOSDB_VirtualizationError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

_wrap_def = """\
from tosdb.datetime import *

class TOSDB_ImplErrorWrapper( {clss} ):
    def __init__( self, error ):
        {clss}( error )
"""

def wrap_impl_error( clss ):
    if not isinstance( clss, TOSDB_Error):
        raise TypeError( "clss must be instance of TOSDB_Error" )    
    our_def = _wrap_def.format(clss=(type(clss).__name__))   
    exec(our_def)
    our_obj = eval("TOSDB_ImplErrorWrapper")(clss)
    try:
        our_obj.__module__ = _sys._getframe(1).f_globals.get('__name__')
    except:
        pass
    return our_obj


    
        




    
