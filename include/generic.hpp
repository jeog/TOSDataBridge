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
#include <stdint.h>

/* why can't we just forward decl in tos_databridge.h ? */
#if defined(THIS_EXPORTS_INTERFACE)
#define DLL_SPEC_IFACE __declspec (dllexport)
#elif defined(THIS_DOESNT_IMPORT_INTERFACE)
#define DLL_SPEC_IFACE 
#else 
#define DLL_SPEC_IFACE __declspec (dllimport) /* default == import */
#endif /* INTERFACE */

template<typename T> /* forward decl */
inline T
CastGenericFromString(std::string& str);

/* 
    TODO: optimize move semantics 
*/

class DLL_SPEC_IFACE TOSDB_Generic{  

  /* The types we support. Because the biggest object we hold is 8-bytes, we 
     need to store the type (1-byte), and we are 8-byte aligned we have plenty 
     of space to expand the supported types and/or cache helper data. Of course, 
     the internal logic may get a bit cloudier...         

     by using sequential ints the compiler *should* build constant-time lookup
     tables for the switch statements we use to branch depending on type  

     note: we don't hold a 'void' object per-se; when an object holding a string 
           is 'moved' we set the _type_val to TYVAL_VOID so the destructor knows 
           not to free the junk _sub then points at                           */

    static const int TYVAL_VOID      = 0;
    static const int TYVAL_LONG      = 1;
    static const int TYVAL_LONG_LONG = 2;
    static const int TYVAL_FLOAT     = 3;
    static const int TYVAL_DOUBLE    = 4;
    static const int TYVAL_STRING    = 5;  
  
  /* max string length we support is 255 chars (why?) */
    static const int STR_MAX = UCHAR_MAX - 1;

  /* the (max) byte size of our internal representation */
    static const int SUB_SZ  = 8;

  /* raw mem holding our 'object' - 
     an array of byte objects seems the logical way to represent this */
    uint8_t _sub[SUB_SZ];

  /* the type of our 'object' - 
     we can expand this to 64 bits because of alignment*/
    unsigned char _type_val;

  /* _val_switch handles casts internally and supports most of the public 
    interface; should be fast, comprehensive, and safe (in that order, i think) */
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
                return (T)CastGenericFromString<T>(**(std::string**)(this->_sub));                      
            default: throw;
            }
        }catch(...){
            std::ostringstream s;
            s << "error casting generic to < " << typeid(T).name() << " >";
#ifdef __GNUC__
            throw std::bad_cast();
#else /* GNU doesn't like */
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
            _type_val( TYVAL_LONG )
        {      
          
            *((long long*)this->_sub) = val;
        }

    explicit TOSDB_Generic(long long val)
        :
            _type_val( TYVAL_LONG_LONG )
        {      
          
            *((long long*)this->_sub) = val;
        }

    explicit TOSDB_Generic(float val)
        :
            _type_val( TYVAL_FLOAT )
        {      
          
            *((double*)this->_sub) = val;
        }

    explicit TOSDB_Generic(double val)
        :
            _type_val( TYVAL_DOUBLE )
        {      
          
            *((double*)this->_sub) = val;
        }

    explicit TOSDB_Generic(std::string str)
        : /* ! must be able to delete (std::string*)_sub at ANY TIME !*/                         
            _type_val( TYVAL_STRING )  
        { /* let string do all the heavy lifting */            
            *((std::string**)(this->_sub)) = new std::string(str.substr(0,STR_MAX)); 
        } 

    TOSDB_Generic(const TOSDB_Generic& gen);

    TOSDB_Generic(TOSDB_Generic&& gen);

    TOSDB_Generic& 
    operator=(const TOSDB_Generic& gen);

    TOSDB_Generic& 
    operator=(TOSDB_Generic&& gen);

    ~TOSDB_Generic() 
        { 
            if( this->is_string() ) 
                delete *(std::string**)(this->_sub); 
        }

    inline bool /* same type AND value */
    operator==(const TOSDB_Generic& gen) const
    {
        return ( this->_type_val == gen._type_val ) 
               && ( this->as_string() == gen.as_string() );
    }

    inline bool /* different type OR value */
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
        return this->_type_val == TYVAL_FLOAT 
               || this->_type_val == TYVAL_DOUBLE; 
    }

    inline bool 
    is_integer() const 
    { 
        return this->_type_val == TYVAL_LONG 
               || this->_type_val == TYVAL_LONG_LONG; 
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
CastGenericFromString(std::string& str) 
{ 
    static_assert(TOSDB_Generic::Type_Check<T>::value,
                  "don't know how to cast to this type"); 
}  

template<>
inline long long
CastGenericFromString<long long>(std::string& str) { return std::stoll(str); }

template<>
inline long
CastGenericFromString<long>(std::string& str) { return std::stol(str); }

template<>
inline double
CastGenericFromString<double>(std::string& str) { return std::stod(str); }

template<>
inline float
CastGenericFromString<float>(std::string& str) { return std::stof(str); }

template<>
inline std::string
CastGenericFromString<std::string>(std::string& str) { return std::string(str); }

#endif
