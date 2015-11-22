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

namespace JO {
/*  
  this is just a crass hack to allow tosdb_data_stream to overload by 
  return type, allowing for a simpler generic C++ interface
*/
class Generic {
  static const int VOID_    = 0;
  static const int LONG_    = 1;
  static const int LONG_LONG_ = 2;
  static const int FLOAT_   = 3;
  static const int DOUBLE_  = 4;
  static const int STRING_  = 5;

  void* _sub;
  unsigned char _type_val;
  unsigned char _type_bsz;
  
  template<typename T> void  _throw_bad_cast() const;
  template<typename T> T     _cast_to_val() const;
  template<typename T> T     _val_switch() const;

  void _sub_deep_copy(Generic& dest, const Generic& src);

public:  
  template<typename T>
  struct Type_Check {
    static const bool value = (std::is_same<T, float>::value 
                              || std::is_same<T, long>::value 
                              || std::is_same<T, double>::value 
                              || std::is_same<T, long long>::value 
                              || std::is_same<T, std::string>::value);
  };

  explicit Generic(long val)
    :
      _sub(new long(val)),
      _type_val(LONG_),
      _type_bsz(sizeof(long))
    {      
    }

  explicit Generic(long long val)
    :
      _sub(new long long(val)),
      _type_val(LONG_LONG_),
      _type_bsz(sizeof(long long))
    {
    }

  explicit Generic(float val)
    :
      _sub(new float(val)),
      _type_val(FLOAT_),
      _type_bsz(sizeof(float))
    {
    }

  explicit Generic(double val)
    :
      _sub(new double(val)),
      _type_val(DOUBLE_),
      _type_bsz(sizeof(double))
    {
    }

  explicit Generic(std::string str)
    : /* limit our string size + /0 to max UCHAR val */
      _sub(new std::string(str.substr(0,254))),
      _type_val(STRING_),
      _type_bsz((unsigned char)str.size() + 1)
    {    
    }

  Generic(Generic&& gen)
    :
      _sub(gen._sub),
      _type_val(gen._type_val),
      _type_bsz(gen._type_bsz)
    {    
      gen._sub  = nullptr;
      gen._type_val = VOID_;
      gen._type_bsz = 0;
    }

  Generic(const Generic& gen)
    : 
      _type_val(gen._type_val),
      _type_bsz(gen._type_bsz)
    {  
      _sub_deep_copy(*this, gen);        
    }  

  Generic& operator=(const Generic& gen);
  Generic& operator=(Generic&& gen);

  ~Generic() { if(_sub) delete _sub; }

  inline bool operator==(const Generic& gen) const
  {
    return ((_type_val == gen._type_val) && (as_string() == gen.as_string()));
  }

  inline bool operator!=(const Generic& gen) const 
  {
    return !this->operator==(gen);
  }

  inline size_t size() const { return _type_bsz; }

  inline bool is_float() const { return (_type_val == FLOAT_); }  
  inline bool is_double() const { return (_type_val == DOUBLE_); }  
  inline bool is_long() const { return (_type_val == LONG_); }
  inline bool is_long_long() const { return (_type_val == LONG_LONG_); }  
  inline bool is_string() const { return (_type_val == STRING_); }
  inline bool is_floating_point() const 
  { 
    return (_type_val == FLOAT_ || _type_val == DOUBLE_); 
  }
  inline bool is_integer() const    
  { 
    return (_type_val == LONG_ || _type_val == LONG_LONG_); 
  }
  inline long      as_long() const { return _val_switch<long>(); }
  inline long long as_long_long() const { return _val_switch<long long>(); }
  inline float     as_float() const { return _val_switch<float>(); }
  inline double    as_double() const { return _val_switch<double>(); }
  std::string      as_string() const;
  inline operator  long() const { return as_long(); }
  inline operator  long long() const { return as_long_long(); }
  inline operator  float() const { return as_float(); }
  inline operator  double() const { return as_double(); }      
  inline operator  std::string() const { return as_string();}
};  


template<typename T>
T Generic::_val_switch() const
{
  try{
    switch(_type_val){
    case LONG_:      return (T)(*(long*)_sub); 
    case LONG_LONG_: return (T)(*(long long*)(_sub)); 
    case FLOAT_:     return (T)(*(float*)(_sub)); 
    case DOUBLE_:    return (T)(*(double*)(_sub)); 
    case STRING_:    return (T)(std::is_floating_point<T>::value 
                                  ? std::stod(*(std::string*)(_sub)) 
                                  : std::stoll(*(std::string*)(_sub)));
    default: throw;
    }
  }catch(...){
    std::ostringstream msgStrm;
    msgStrm << "error casting generic to < " << typeid(T).name() << " >";
#ifndef XPLATFORM_PYTHON_CONSTS_ONLY
    throw std::bad_cast(msgStrm.str().c_str());
#else
    throw std::bad_cast();
#endif
  }
}

};
#endif
