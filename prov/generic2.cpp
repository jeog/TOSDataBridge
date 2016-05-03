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

#include "generic2.hpp"


/* COPY CONSTRUCT */
TOSDB_Generic::TOSDB_Generic(const TOSDB_Generic& gen)
    : 
        _type_val( gen._type_val )
    {  
        if( gen.is_string() )        
            *(std::string**)this->_sub = 
                new std::string(**(std::string**)(gen._sub));
        else            
            memcpy(this->_sub, gen._sub, SUB_SZ);                      
    }  


/* MOVE CONSTRUCT */
TOSDB_Generic::TOSDB_Generic(TOSDB_Generic&& gen)
    :      
        _type_val( gen._type_val )
    {                
      /* we are stealing the scalar value OR the string pointer */           
        memcpy(this->_sub, gen._sub, SUB_SZ);      

      /* leave the pointer but set _type_val to VOID to avoid bad free; ptr is an 
         implementation detail of the moved object that we can safely ignore */
        gen._type_val = TYVAL_VOID;            
    }


/* ASSIGNMENT */
TOSDB_Generic& 
TOSDB_Generic::operator=(const TOSDB_Generic& gen)
{
    if( *this == gen )
        return *this;

    if( gen.is_string() )
    {          
        if( this->is_string() ) 
            /* if we're also a string simply assign */
            **(std::string**)(this->_sub) = **(std::string**)(gen._sub);
        else
            /* otherwise allocate new */ 
            *(std::string**)this->_sub = 
                new std::string(**(std::string**)(gen._sub));
    }
    else
    {
        if( this->is_string() )
            delete *(std::string**)this->_sub;  
           
        memcpy(this->_sub, gen._sub, SUB_SZ);    
    }

    this->_type_val = gen._type_val;        
    return *this;    
}


/* MOVE ASSIGNMENT */
TOSDB_Generic& 
TOSDB_Generic::operator=(TOSDB_Generic&& gen)
{
  /* not sure if this is optimal or its better to branch(as above) and move
     the string object in, allowing gen's destructor to delete 

     although, this way is a lot cleaner than 2 nested conditionals*/

    if(this->_type_val == TYVAL_STRING)
        delete *(std::string**)this->_sub; 
     
    this->_type_val = gen._type_val;

  /* we are stealing the scalar value OR the string pointer */           
    memcpy(this->_sub, gen._sub, SUB_SZ);    
       
  /* leave the pointer but set _type_val to VOID to avoid bad free; ptr is an 
     implementation detail of the moved object that we can safely ignore */
    gen._type_val = TYVAL_VOID;

    return *this;
}


std::string 
TOSDB_Generic::as_string() const
{
   switch(this->_type_val){
   case(TYVAL_LONG):
   case(TYVAL_LONG_LONG):
       return std::to_string(*((long long*)this->_sub));
   case(TYVAL_FLOAT):
   case(TYVAL_DOUBLE):
       return std::to_string(*((double*)this->_sub));
   case(TYVAL_STRING):
       return **(std::string**)(this->_sub);
   default:
       return std::string();
   };
}

size_t 
TOSDB_Generic::size() const
{
   switch(this->_type_val){
   case(TYVAL_LONG):      return sizeof(long);
   case(TYVAL_LONG_LONG): return sizeof(long long);
   case(TYVAL_FLOAT):     return sizeof(float);
   case(TYVAL_DOUBLE):    return sizeof(double);
   case(TYVAL_STRING):    return (*(std::string**)(this->_sub))->size();
   default:               return 0;
   };
}


