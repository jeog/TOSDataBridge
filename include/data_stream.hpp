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

#ifndef JO_TOSDB_DATA_STREAM
#define JO_TOSDB_DATA_STREAM

#ifndef STR_DATA_SZ
#include "tos_databridge.h"
#define STR_DATA_SZ TOSDB_STR_DATA_SZ // ((unsigned long)0xFF)
#endif

#include <deque>
#include <string>
#include <vector>
#include <mutex>  

/* implemented in src/data_streams.tpp */

/* interface */
#define DATASTREAM_INTERFACE_TEMPLATE template< typename SecTy, typename GenTy >
#define DATASTREAM_INTERFACE_CLASS DataStreamInterface<SecTy, GenTy>

/* base object */
#define DATASTREAM_PRIMARY_TEMPLATE \
    template<typename Ty, \
             typename SecTy, \
             typename GenTy, \
             bool UseSecondary,\
             typename Allocator>

#define DATASTREAM_PRIMARY_CLASS DataStream<Ty, SecTy, GenTy, UseSecondary, Allocator>

/* specialization of base that inherits from base */
#define DATASTREAM_SECONDARY_TEMPLATE \
    template<typename Ty, \
             typename SecTy, \
             typename GenTy, \
             typename Allocator > 

#define DATASTREAM_SECONDARY_CLASS DataStream<Ty, SecTy, GenTy, true, Allocator>


/*forward decl*/
class DataStreamError;
class DataStreamTypeError;
class DataStreamSizeViolation;
class DataStreamOutOfRange;
class DataStreamInvalidArgument;


namespace {
template<typename InTy, bool F>
void BuildThrowTypeError(const char* method)
{     
  std::ostringstream s;  
  s << "Invalid argument < " << (F ? "UNKNOWN" : typeid(InTy).name())
    << " > passed to DataStream." << method <<" for this instantiation.";
  throw DataStreamTypeError(s.str().c_str());    
}
};


template< typename SecTy, typename GenTy >      
class DataStreamInterface {
public:
  typedef GenTy generic_ty;
  typedef SecTy secondary_ty;

  typedef std::pair<GenTy, SecTy> both_ty;
  typedef std::vector<GenTy>      generic_vector_ty;
  typedef std::vector<SecTy>      secondary_vector_ty;

  /* hard-coded to avoid some of the corner cases. */
  static const size_t MAX_BOUND_SIZE = INT_MAX;

private:
  template<typename InTy, typename OutTy>
  size_t 
  _copy(OutTy* dest, size_t sz, int end, int beg, secondary_ty* sec) const; 
 
  template<typename InTy, typename OutTy>
  long long 
  _copy_using_atomic_marker(OutTy* dest, size_t sz, int beg, secondary_ty* sec) const;

protected:
  unsigned int _count;
  DataStreamInterface()
    : 
      _count(0) 
    {
    }

public:
  virtual ~DataStreamInterface() 
    {
    }

  virtual size_t      
  bound_size() const = 0;

  virtual size_t      
  bound_size(size_t) = 0;

  virtual size_t      
  size() const = 0;

  virtual bool        
  empty() const = 0;  

  virtual bool        
  uses_secondary() const = 0;

  virtual long long   
  marker_position() const = 0;

  virtual bool        
  is_marker_dirty() const = 0;

  virtual generic_ty  
  operator[](int) const = 0;

  virtual both_ty     
  both(int) const = 0;  

  virtual generic_vector_ty 
  vector(int end = -1, int beg = 0) const = 0;

  virtual secondary_vector_ty 
  secondary_vector(int end = -1, int beg = 0) const = 0;  
  
  virtual void 
  push(const generic_ty& obj, secondary_ty sec = secondary_ty()) = 0;

  virtual void /* SHOULD WE THROW? */ 
  secondary(secondary_ty* dest, int indx) const { dest = nullptr; }
  

 /* MACROS that help avoid constructing GenTy if possible */

#define VIRTUAL_VOID_PUSH_2ARG_DROP(InTy, OutTy) \
virtual void push(const InTy val, secondary_ty sec = secondary_ty()) { \
  this->push((OutTy)val, std::move(sec)); \
} 

#define VIRTUAL_VOID_PUSH_2ARG_BREAK(InTy) \
virtual void push(const InTy val, secondary_ty sec = secondary_ty()) { \
  this->push(std::to_string(val) , std::move(sec)); \
} 

#define VIRTUAL_VOID_PUSH_2ARG_LOOP(InTy, LoopOnC1) \
virtual void push(const InTy str, secondary_ty sec = secondary_ty()) { \
  if(this->_count++){ \
    this->_count = 0; \
    BuildThrowTypeError<const InTy,true>("push()"); \
  } \
  this->push(LoopOnC1, std::move(sec)); \
} 

#define VIRTUAL_VOID_COPY_2ARG_DROP(InTy, OutTy) \
virtual size_t copy(InTy* dest, size_t sz, int end = -1, int beg = 0, \
                    secondary_ty* sec = nullptr) const { \
  return this->_copy<OutTy>(dest, sz, end, beg, sec); \
} 

#define VIRTUAL_VOID_COPY_2ARG_BREAK(InTy, DropBool) \
virtual size_t copy(InTy* dest, size_t sz, int end = -1, int beg = 0, \
                    secondary_ty* sec = nullptr) const { \
  BuildThrowTypeError<InTy*,DropBool>("copy()"); \
  return 0; \
}

#define VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(InTy, OutTy) \
virtual long long copy_from_marker(InTy* dest, size_t sz, int beg = 0, \
                                   secondary_ty* sec = nullptr) const { \
  return this->_copy_using_atomic_marker< OutTy >(dest, sz, beg, sec); \
} 

#define VIRTUAL_VOID_MARKER_COPY_2ARG_BREAK(InTy, DropBool) \
virtual long long copy_from_marker(InTy* dest, size_t sz, int beg = 0, \
                                   secondary_ty* sec = nullptr) const { \
  BuildThrowTypeError<InTy*,DropBool>("copy_from_marker()"); \
  return 0; \
}

  VIRTUAL_VOID_PUSH_2ARG_DROP(float, double)
  VIRTUAL_VOID_PUSH_2ARG_BREAK(double)
  VIRTUAL_VOID_PUSH_2ARG_DROP(unsigned char, unsigned short)
  VIRTUAL_VOID_PUSH_2ARG_DROP(unsigned short, unsigned int)
  VIRTUAL_VOID_PUSH_2ARG_DROP(unsigned int, unsigned long)
  VIRTUAL_VOID_PUSH_2ARG_DROP(unsigned long, unsigned long long)
  VIRTUAL_VOID_PUSH_2ARG_BREAK(unsigned long long)
  VIRTUAL_VOID_PUSH_2ARG_DROP(char, short)
  VIRTUAL_VOID_PUSH_2ARG_DROP(short, int)
  VIRTUAL_VOID_PUSH_2ARG_DROP(int, long)
  VIRTUAL_VOID_PUSH_2ARG_DROP(long, long long)
  VIRTUAL_VOID_PUSH_2ARG_BREAK(long long)
  VIRTUAL_VOID_PUSH_2ARG_LOOP(std::string, str.c_str())
  VIRTUAL_VOID_PUSH_2ARG_LOOP(char*, std::string(str))

  VIRTUAL_VOID_COPY_2ARG_DROP(long long, long)
  VIRTUAL_VOID_COPY_2ARG_DROP(long, int)
  VIRTUAL_VOID_COPY_2ARG_DROP(int, short)
  VIRTUAL_VOID_COPY_2ARG_DROP(short, char)
  VIRTUAL_VOID_COPY_2ARG_BREAK(char, true)
  VIRTUAL_VOID_COPY_2ARG_DROP(unsigned long long, unsigned long)
  VIRTUAL_VOID_COPY_2ARG_DROP(unsigned long, unsigned int)
  VIRTUAL_VOID_COPY_2ARG_DROP(unsigned int, unsigned short)
  VIRTUAL_VOID_COPY_2ARG_DROP(unsigned short, unsigned char)
  VIRTUAL_VOID_COPY_2ARG_BREAK(unsigned char, true)
  VIRTUAL_VOID_COPY_2ARG_DROP(double, float)
  VIRTUAL_VOID_COPY_2ARG_BREAK(float, false) 

  virtual size_t 
  copy(char** dest, 
       size_t dest_sz, 
       size_t str_sz, 
       int end = -1, 
       int beg = 0 , 
       secondary_ty* sec = nullptr) const;

  virtual size_t 
  copy(std::string* dest, 
       size_t sz, 
       int end = -1, 
       int beg = 0, 
       secondary_ty* sec = nullptr) const;

  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(long long, long)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(long, int)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(int, short)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(short, char)
  VIRTUAL_VOID_MARKER_COPY_2ARG_BREAK(char, true)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(unsigned long long, unsigned long)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(unsigned long, unsigned int)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(unsigned int, unsigned short)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(unsigned short, unsigned char)
  VIRTUAL_VOID_MARKER_COPY_2ARG_BREAK(unsigned char, true)
  VIRTUAL_VOID_MARKER_COPY_2ARG_DROP(double, float)
  VIRTUAL_VOID_MARKER_COPY_2ARG_BREAK(float, false) 

  virtual long long 
  copy_from_marker(char** dest, 
                   size_t dest_sz, 
                   size_t str_sz,             
                   int beg = 0, 
                   secondary_ty* sec = nullptr) const;

  virtual long long 
  copy_from_marker(std::string* dest, 
                   size_t sz,                     
                   int beg = 0, 
                   secondary_ty* sec = nullptr) const;
};


template< typename SecTy, typename GenTy >
std::ostream& 
operator<<(std::ostream&, const DataStreamInterface<SecTy,GenTy>&);


template<typename Ty,
         typename SecTy,
         typename GenTy,      
         bool UseSecondary = false,
         typename Allocator = std::allocator<Ty> >
class DataStream 
  /* CONTAINS A PRIMARY DEQUE */
    : public DataStreamInterface<SecTy, GenTy>{             

  class{
    static const bool valid = GenTy::Type_Check<Ty>::value;  
    static_assert(valid, "DataStream: Ty failed GenTy type-check"); 
  }MyTypeCheck;  
  
  typedef DataStream<Ty,SecTy,GenTy,UseSecondary,Allocator>  _my_ty;
  typedef DataStreamInterface<SecTy,GenTy>                   _my_base_ty;
  typedef std::deque<Ty,Allocator>                           _my_impl_ty;      

  _my_ty& 
  operator=(const _my_ty &);
  
  void 
  _push(const Ty _item); 

protected:
  typedef std::lock_guard<std::recursive_mutex >  _my_lock_guard_type;
   
  _my_impl_ty      _my_impl_obj;
  size_t           _q_bound, _q_count;
  long long* const _mark_count;
  bool* const      _mark_is_dirty;     
  volatile bool    _push_has_priority;

  std::recursive_mutex* const  _mtx;

  inline void _yld_to_push() const
  {  
    if( !(this->_push_has_priority) )      
      std::this_thread::yield();
  } 

  template<typename T>
  bool
  _check_adj(int& end, 
             int& beg, 
             const std::deque<T,Allocator>& impl) const;

  void
  _incr_internal_counts();

  template<typename ImplTy, typename DestTy> 
  size_t 
  _copy_to_ptr(ImplTy& impl, 
               DestTy* dest, 
               size_t sz, 
               unsigned int end, 
               unsigned int beg) const;

public:
  typedef _my_base_ty  interface_type;
  typedef Ty           value_type;

  DataStream(size_t sz);     
  DataStream(const _my_ty & stream);
  DataStream(_my_ty && stream);
  virtual ~DataStream();

  inline bool      
  empty() const { return this->_my_impl_obj.empty(); }

  inline size_t    
  size() const { return this->_q_count; }

  inline bool      
  uses_secondary() const { return false; }

  inline bool      
  is_marker_dirty() const { return *(this->_mark_is_dirty); }

  inline long long 
  marker_position() const { return *(this->_mark_count); }   

  inline size_t    
  bound_size() const { return this->_q_bound; }
   
  size_t 
  bound_size(size_t sz);
    
  inline void 
  push(const Ty val, secondary_ty sec = secondary_ty()){
      this->_count = 0;    
      this->_push(val);    
  }

  inline void    
  push(const generic_ty& gen, secondary_ty sec = secondary_ty()){
      this->_count = 0;
      this->_push((Ty)gen);    
  }

  long long 
  copy_from_marker(Ty* dest, 
                   size_t sz,              
                   int beg = 0, 
                   secondary_ty* sec = nullptr) const;
  
  long long 
  copy_from_marker(char** dest, 
                   size_t dest_sz, 
                   size_t str_sz,                
                   int beg = 0, 
                   secondary_ty* sec = nullptr) const;
    
  size_t 
  copy(Ty* dest, 
       size_t sz, 
       int end = -1, 
       int beg = 0, 
       secondary_ty* sec = nullptr) const;
    
  size_t 
  copy(char** dest, 
       size_t dest_sz, 
       size_t str_sz, 
       int end = -1, 
       int beg = 0, 
       secondary_ty* sec = nullptr) const;

  generic_ty 
  operator[](int indx) const;

  both_ty
  both(int indx) const;

  generic_vector_ty 
  vector(int end = -1, int beg = 0) const;

  secondary_vector_ty 
  secondary_vector(int end = -1, int beg = 0) const;

};

             
template<typename Ty,      
         typename SecTy,
         typename GenTy,
         typename Allocator >
class DataStream<Ty, SecTy, GenTy, true, Allocator> 
  /* CONTAINS PRIMARY AND SECONDARY DEQUE */
    : public DataStream<Ty, SecTy, GenTy, false, Allocator> {

  typedef DataStream<Ty,SecTy,GenTy,true,Allocator>  _my_ty;
  typedef DataStream<Ty,SecTy,GenTy,false,Allocator> _my_base_ty;
  typedef std::deque<SecTy,Allocator>                _my_sec_impl_ty;
  
  _my_sec_impl_ty _my_sec_impl_obj;  
  
  void _push(const Ty _item, const secondary_ty sec);

public:
  typedef Ty value_type;

  DataStream(size_t sz);
  DataStream(const _my_ty & stream);
  DataStream(_my_ty && stream);

  size_t 
  bound_size(size_t sz);

  inline bool 
  uses_secondary(){ return true; }

  inline void 
  push(const Ty val, secondary_ty sec = secondary_ty()){    
    this->_count = 0;
    this->_push(val, std::move(sec));     
  }

  inline void 
  push(const generic_ty& gen, secondary_ty sec = secondary_ty()){
    this->_count = 0;
    this->_push((Ty)gen, std::move(sec));
  }
  
  size_t 
  copy(Ty* dest, 
       size_t sz, 
       int end = -1, 
       int beg = 0, 
       secondary_ty* sec = nullptr) const;

  size_t 
  copy(char** dest, 
       size_t dest_sz, 
       size_t str_sz, 
       int end = -1, 
       int beg = 0, 
       secondary_ty* sec = nullptr) const;
  
  both_ty 
  both(int indx) const;

  void 
  secondary(secondary_ty* dest, int indx) const;

  secondary_vector_ty 
  secondary_vector(int end = -1, int beg = 0) const;
};  


#include "data_stream.tpp"


class DataStreamError 
    : public std::exception{
public:
  DataStreamError(const char* info) 
    : 
      std::exception(info) 
    {
    }
};


class DataStreamTypeError 
    : public DataStreamError{
public:
  DataStreamTypeError(const char* info) 
    : 
      DataStreamError(info) 
    {
    }
};

class DataStreamSizeViolation 
    : public DataStreamError{
public:
  const size_t bound_size, deque_size;       
  DataStreamSizeViolation(const char* msg, size_t bsz, size_t dsz)
    : 
      DataStreamError(msg),
      bound_size(bsz),
      deque_size(dsz)        
    {        
    }
};

class DataStreamOutOfRange 
    : public DataStreamError{      
public:
  const int size, beg, end;
  DataStreamOutOfRange(const char* msg, int sz, int beg, int end)
    : 
      DataStreamError(msg),              
      size(sz), 
      beg(beg), 
      end(end)
    {        
    }  
};

class DataStreamInvalidArgument
    : public DataStreamError {
public: 
  DataStreamInvalidArgument(const char* msg)
    : 
      DataStreamError(msg)
    {        
    }
};

#endif
