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

#ifndef JO_JO_GENERIC
#define JO_JO_GENERIC

#include <sstream>
#include <string>
#include <iostream>
#include <typeinfo>
#include <math.h>
#include <limits.h>
#include <string.h>


template<typename T> /* forward decl */
inline T
CastGenericFromString(std::string str);


class TOSDB_Generic{  
    static const int TYVAL_VOID      = 0;
    static const int TYVAL_LONG      = 1;
    static const int TYVAL_LONG_LONG = 2;
    static const int TYVAL_FLOAT     = 3;
    static const int TYVAL_DOUBLE    = 4;
    static const int TYVAL_STRING    = 5;

    static const int STR_MAX = UCHAR_MAX - 1;

    /* raw mem holding our 'object' */
    uint8_t _sub[8];

    /* the type of our 'object' */
    unsigned char _type_val;

    /* have some space from alignment so we'll cache the size (of the 
       implementations internal representation, NOT the actual size of the type) */
    unsigned char _type_bsz; 

    template< typename T >  
    T     
    _val_switch() const
    {
        try{
            switch(this->_type_val){
            case TYVAL_LONG:            
            case TYVAL_LONG_LONG: 
                return (T)(*(long long*)(this->_sub)); 
            case TYVAL_FLOAT:                
            case TYVAL_DOUBLE:    
                return (T)(*(double*)(this->_sub)); 
            case TYVAL_STRING:    
                return (T)CastGenericFromString<T>(*(std::string*)(this->_sub));                      
            default: throw;
            }
        }catch(...){
            std::ostringstream s;
            s << "error casting generic to < " << typeid(T).name() << " >";
#ifdef __GNUC__
            throw std::bad_cast();
#else
            throw std::bad_cast(s.str().c_str());
#endif
       }
   }

public:  
    template<typename T>
    struct Type_Check {
        static const bool 
        value = std::is_same<T, float>::value 
                || std::is_same<T, long>::value 
                || std::is_same<T, double>::value 
                || std::is_same<T, long long>::value 
                || std::is_same<T, std::string>::value;
    };

    explicit TOSDB_Generic(long val)
        :           
            _type_val( TYVAL_LONG ),
            _type_bsz( sizeof(long long) )
        {      
          
            *((long long*)this->_sub) = val;
        }

    explicit TOSDB_Generic(long long val)
        :
            _type_val( TYVAL_LONG_LONG ),
            _type_bsz( sizeof(long long) )
        {      
          
            *((long long*)this->_sub) = val;
        }

    explicit TOSDB_Generic(float val)
        :
            _type_val( TYVAL_FLOAT ),
            _type_bsz( sizeof(double) )
        {      
          
            *((double*)this->_sub) = val;
        }

    explicit TOSDB_Generic(double val)
        :
            _type_val( TYVAL_DOUBLE ),
            _type_bsz( sizeof(double) )
        {      
          
            *((double*)this->_sub) = val;
        }

    explicit TOSDB_Generic(std::string str)
        : /* ! must be able to delete (std::string*)_sub at ANY TIME !*/                         
            _type_val( TYVAL_STRING ), 
            _type_bsz( sizeof(std::string*) )  
        { /* let string do all the heavy lifting */            
            *((std::string**)(this->_sub)) = new std::string(str.substr(0,STR_MAX)); 
        }

    TOSDB_Generic(TOSDB_Generic&& gen)
        :      
            _type_val( gen._type_val ),
            _type_bsz( gen._type_bsz )
        {                
            /* we are stealing the scalar value OR the string pointer */           
            memcpy(this->_sub, gen._sub, gen._type_bsz);      
            /* keep gen's carcas from screwing us if it had a string ptr */          
            memset(gen._sub, 0, 8);
            /* and change its type to keep it's destructor from doing the same */
            gen._type_val = TYVAL_VOID;            
        }

    TOSDB_Generic(const TOSDB_Generic& gen)
        : 
            _type_val( gen._type_val ),
            _type_bsz( gen._type_bsz )
        {  
            if(gen._type_val == TYVAL_STRING)        
                *(std::string**)this->_sub = 
                    new std::string(*((std::string*)gen._sub));
            else            
                memcpy(this->_sub, gen._sub, gen._type_bsz);                      
        }  

  TOSDB_Generic& 
  operator=(const TOSDB_Generic& gen);

  TOSDB_Generic& 
  operator=(TOSDB_Generic&& gen);

  ~TOSDB_Generic() 
      { 
          if(this->_type_val == TYVAL_STRING) 
              delete *(std::string**)(this->_sub); 
      }

  inline bool 
  operator==(const TOSDB_Generic& gen) const
  {
      return ( (this->_type_val == gen._type_val) 
                && (this->as_string() == gen.as_string()) );
  }

  inline bool 
  operator!=(const TOSDB_Generic& gen) const 
  {
      return !(this->operator==(gen));
  }

  size_t 
  size() const;

  inline bool 
  is_float() const { return (this->_type_val == TYVAL_FLOAT); }  

  inline bool 
  is_double() const { return (this->_type_val == TYVAL_DOUBLE); }  

  inline bool 
  is_long() const { return (this->_type_val == TYVAL_LONG); }

  inline bool 
  is_long_long() const { return (this->_type_val == TYVAL_LONG_LONG); }  

  inline bool 
  is_string() const { return (this->_type_val == TYVAL_STRING); }

  inline bool 
  is_floating_point() const 
  { 
      return (this->_type_val == TYVAL_FLOAT || this->_type_val == TYVAL_DOUBLE); 
  }

  inline bool 
  is_integer() const 
  { 
      return (this->_type_val == TYVAL_LONG || this->_type_val == TYVAL_LONG_LONG); 
  }

  inline long      
  as_long() const { return this->_val_switch<long>(); }

  inline long long 
  as_long_long() const { return this->_val_switch<long long>(); }

  inline float     
  as_float() const { return this->_val_switch<float>(); }

  inline double    
  as_double() const { return this->_val_switch<double>(); }

  std::string      
  as_string() const; 

  inline operator  
  long() const { return this->_val_switch<long>(); }

  inline operator  
  long long() const { return this->_val_switch<long long>(); }

  inline operator  
  float() const { return this->_val_switch<float>(); }

  inline operator  
  double() const { return this->_val_switch<double>(); }      

  inline operator  
  std::string() const { return this->as_string(); }
};  


template<typename T>
inline T
CastGenericFromString(std::string str) 
{ 
    static_assert(TOSDB_Generic::Type_Check<T>::value,
                  "don't know how to cast to this type"); 
}  

template<>
inline long long
CastGenericFromString<long long>(std::string str) { return std::stoll(str); }

template<>
inline long
CastGenericFromString<long>(std::string str) { return std::stol(str); }

template<>
inline double
CastGenericFromString<double>(std::string str) { return std::stod(str); }

template<>
inline float
CastGenericFromString<float>(std::string str) { return std::stof(str); }

template<>
inline std::string
CastGenericFromString<std::string>(std::string str) { return str; }

#endif
