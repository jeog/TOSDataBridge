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
#include <typeinfo>
#include <math.h>


class TOSDB_Generic{
    static const int VOID_      = 0;
    static const int LONG_      = 1;
    static const int LONG_LONG_ = 2;
    static const int FLOAT_     = 3;
    static const int DOUBLE_    = 4;
    static const int STRING_    = 5;

    static const int STR_MAX    = UCHAR_MAX - 1;

    void* _sub;
    unsigned char _type_val;
    /* have some space from alignment so we'll cache the size */
    unsigned char _type_bsz; 

    template<typename T> T     
    _val_switch() const;

    void _sub_deep_copy(const TOSDB_Generic& src);

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
            _sub( new long(val) ),
            _type_val( LONG_ ),
            _type_bsz( sizeof(long) )
        {      
        }

    explicit TOSDB_Generic(long long val)
        :
            _sub( new long long(val) ),
            _type_val( LONG_LONG_ ),
            _type_bsz( sizeof(long long) )
        {
        }

    explicit TOSDB_Generic(float val)
        :
            _sub( new float(val) ),
            _type_val( FLOAT_ ),
            _type_bsz( sizeof(float) )
        {
        }

    explicit TOSDB_Generic(double val)
        :
            _sub( new double(val) ),
            _type_val( DOUBLE_ ),
            _type_bsz( sizeof(double) )
        {
        }

    explicit TOSDB_Generic(std::string str)
        :   
            /* let string class handle details for us */
            _sub( new std::string(str.substr(0,STR_MAX)) ),            
            _type_val( STRING_ ) /*,
            _type_bsz( 0 ) <- superfluous */
        {    
        }

    TOSDB_Generic(TOSDB_Generic&& gen)
        :
            _sub( gen._sub ),
            _type_val( gen._type_val ),
            _type_bsz( gen._type_bsz )
        {    
            gen._sub  = nullptr;
            gen._type_val = VOID_;
            gen._type_bsz = 0;
        }

    TOSDB_Generic(const TOSDB_Generic& gen)
        : 
            _type_val( gen._type_val ),
            _type_bsz( gen._type_bsz )
        {  
            this->_sub_deep_copy(gen);        
        }  

  TOSDB_Generic& 
  operator=(const TOSDB_Generic& gen);

  TOSDB_Generic& 
  operator=(TOSDB_Generic&& gen);

  ~TOSDB_Generic() 
      { 
          if(this->_sub) 
              delete this->_sub; 
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

  inline size_t 
  size() const { return this->_type_bsz; }

  inline bool 
  is_float() const { return (this->_type_val == FLOAT_); }  

  inline bool 
  is_double() const { return (this->_type_val == DOUBLE_); }  

  inline bool 
  is_long() const { return (this->_type_val == LONG_); }

  inline bool 
  is_long_long() const { return (this->_type_val == LONG_LONG_); }  

  inline bool 
  is_string() const { return (this->_type_val == STRING_); }

  inline bool 
  is_floating_point() const 
  { 
      return (this->_type_val == FLOAT_ || this->_type_val == DOUBLE_); 
  }

  inline bool 
  is_integer() const 
  { 
      return (this->_type_val == LONG_ || this->_type_val == LONG_LONG_); 
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
  as_string() const; /*generic.cpp*/

  inline operator  
  long() const { return this->as_long(); }

  inline operator  
  long long() const { return this->as_long_long(); }

  inline operator  
  float() const { return this->as_float(); }

  inline operator  
  double() const { return this->as_double(); }      

  inline operator  
  std::string() const { return this->as_string(); }
};  


template<typename T>
T 
TOSDB_Generic::_val_switch() const
{
    try{
        switch(this->_type_val){
        case LONG_:      
            return (T)(*(long*)(this->_sub)); 
        case LONG_LONG_: 
            return (T)(*(long long*)(this->_sub)); 
        case FLOAT_:     
            return (T)(*(float*)(this->_sub)); 
        case DOUBLE_:    
            return (T)(*(double*)(this->_sub)); 
        case STRING_:    
            return (T)(std::is_floating_point<T>::value 
                        ? std::stod(*(std::string*)(this->_sub)) 
                        : std::stoll(*(std::string*)(this->_sub)));
        default: throw;
        }
    }catch(...){
        std::ostringstream s;
        s << "error casting generic to < " << typeid(T).name() << " >";
        throw std::bad_cast(s.str().c_str());
    }
}


#endif
