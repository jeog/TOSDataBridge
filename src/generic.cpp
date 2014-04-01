/* 
Copyright (C) 2014 Jonathon Ogden	 < jeog.dev@gmail.com >

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

#include "tos_databridge.h"
#include "generic.hpp"

namespace JO{

Generic& Generic::operator=(const Generic& gen)
{
	if ((*this) != gen)
	{			
		_typeVal = gen._typeVal;
		_typeBSz = gen._typeBSz;
		if( _sub ) 
			delete _sub;
		_sub_deep_copy(*this, gen);
	}
	return *this;		
}

Generic& Generic::operator=( Generic&& gen )
{
	delete _sub;
	_sub = gen._sub;
	_typeVal = gen._typeVal;
	_typeBSz = gen._typeBSz;
	gen._sub = nullptr;
	gen._typeVal = VOID_;
	gen._typeBSz = 0;
	return *this;
}

void Generic::_sub_deep_copy( Generic& dest, const Generic& src)
{			
	if( src._typeVal == STRING_ )
		dest._sub = new std::string(*((std::string*)src._sub));
	else
	{
		dest._sub = malloc( src._typeBSz );
		memcpy_s(dest._sub, src._typeBSz, src._sub, src._typeBSz );
	}
}

std::string Generic::as_string() const
{
	if( this->is_integer() )
		return std::to_string(as_long_long());
	else if( this->is_floating_point() )
		return std::to_string(as_double()); 	
	else if( this->is_string() )
		return *((std::string*)this->_sub);
	else
		return std::string();
}

};