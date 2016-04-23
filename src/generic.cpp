/* 
Copyright (C) 2014 Jonathon Ogden   < jeog.dev@gmail.com >

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

//#include "tos_databridge.h"
#include "generic.hpp"

TOSDB_Generic& 
TOSDB_Generic::operator=(const TOSDB_Generic& gen)
{
    if ((*this) != gen){  
      this->_type_val = gen._type_val;
      this->_type_bsz = gen._type_bsz;
      if(this->_sub) 
          delete this->_sub;
      this->_sub_deep_copy(gen);
    }
    return *this;    
}

TOSDB_Generic& 
TOSDB_Generic::operator=(TOSDB_Generic&& gen)
{
    delete this->_sub;

    this->_sub = gen._sub;
    this->_type_val = gen._type_val;
    this->_type_bsz = gen._type_bsz;

    gen._sub = nullptr;
    gen._type_val = VOID_;
    gen._type_bsz = 0;

    return *this;
}

void 
TOSDB_Generic::_sub_deep_copy(const TOSDB_Generic& src)
{      
    if(src._type_val == STRING_){
        this->_sub = new std::string(*((std::string*)src._sub));
    }else{
        this->_sub = malloc(src._type_bsz);        
        /* NO CHECK: if we can't allocate 4 or 8 bytes 
                     we'll blow up a million other places */
        memcpy(this->_sub, src._sub,  src._type_bsz);
    }
}

std::string 
TOSDB_Generic::as_string() const
{
    if( this->is_integer() )             
        return std::to_string(this->as_long_long());

    else if( this->is_floating_point() ) 
        return std::to_string(this->as_double());  

    else if( this->is_string() )         
        return *((std::string*)(this->_sub));

    else                               
        return std::string();
}


