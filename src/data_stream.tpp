#include "data_stream.hpp"

DATASTREAM_INTERFACE_TEMPLATE
template<typename InTy, typename OutTy>
size_t 
DATASTREAM_INTERFACE_CLASS::_copy(OutTy* dest, 
                                  size_t sz, 
                                  int end, 
                                  int beg, 
                                  typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec) const
{  
    size_t ret;

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");   

    std::unique_ptr<InTy,void(*)(InTy*)>  tmp(new InTy[sz], [](InTy* p){ delete[] p; });

    ret = copy(tmp.get(), sz, end, beg, sec);
    for(size_t i = 0; i < sz; ++i)      
        dest[i] = (OutTy)tmp.get()[i];                

    return ret;
}  

DATASTREAM_INTERFACE_TEMPLATE
template<typename InTy, typename OutTy>
long long 
DATASTREAM_INTERFACE_CLASS::_copy_using_atomic_marker(OutTy* dest, 
                                                      size_t sz,         
                                                      int beg, 
                                                      typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec) const
{  
    long long ret;

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");   
  
    std::unique_ptr<InTy,void(*)(InTy*)>  tmp(new InTy[sz], [](InTy* p){ delete[] p; });

    ret = copy_from_marker(tmp.get(), sz, beg, sec);
    for(size_t i = 0; i < sz; ++i)      
        dest[i] = (OutTy)tmp.get()[i];  

    return ret;
}  

DATASTREAM_INTERFACE_TEMPLATE
size_t 
DATASTREAM_INTERFACE_CLASS::copy(char** dest, 
                                 size_t dest_sz, 
                                 size_t str_sz, 
                                 int end = -1, 
                                 int beg = 0 , 
                                 typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec = nullptr) const 
{ 
    BuildThrowTypeError<std::string*,false>("copy()");  
    return 0;
}

DATASTREAM_INTERFACE_TEMPLATE
size_t 
DATASTREAM_INTERFACE_CLASS::copy(std::string* dest, 
                                 size_t sz, 
                                 int end = -1, 
                                 int beg = 0, 
                                 typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec = nullptr) const
{
    size_t ret;

    if(!dest)
      throw DataStreamInvalidArgument("NULL dest argument");

    auto dstr = [sz](char** p){ DeleteStrings(p, sz); };
    std::unique_ptr<char*,decltype(dstr)> sptr(NewStrings(sz,STR_DATA_SZ), dstr);

    ret = copy(sptr.get(), sz, STR_DATA_SZ , end, beg, sec);        
    std::copy_n(sptr.get(), sz, dest); 

    return ret;
}

DATASTREAM_INTERFACE_TEMPLATE
long long 
DATASTREAM_INTERFACE_CLASS::copy_from_marker(char** dest, 
                                             size_t dest_sz, 
                                             size_t str_sz,             
                                             int beg = 0, 
                                             typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec = nullptr) const 
{ 
    BuildThrowTypeError<std::string*,false>("copy_from_marker()");  
    return 0;
}

DATASTREAM_INTERFACE_TEMPLATE
long long 
DATASTREAM_INTERFACE_CLASS::copy_from_marker(std::string* dest, 
                                             size_t sz,                     
                                             int beg = 0, 
                                             typename DATASTREAM_INTERFACE_CLASS::secondary_ty* sec = nullptr) const
{
    long long ret;

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");

    auto dstr = [sz](char** pptr){ DeleteStrings(pptr, sz); };
    std::unique_ptr<char*,decltype(dstr)>  sptr(NewStrings(sz,STR_DATA_SZ), dstr);

    ret = copy_from_marker(sptr.get(), sz, STR_DATA_SZ, beg, sec);        
    std::copy_n(sptr.get(), sz, dest);   

    return ret;
}


DATASTREAM_PRIMARY_TEMPLATE
void 
DATASTREAM_PRIMARY_CLASS::_push(const Ty _item) 
{  /* 
    * if can't obtain lock indicate other threads should yield to us 
    */     
    _push_has_priority = _mtx->try_lock(); /*O.K. push/pop doesn't throw*/
    if(!_push_has_priority) 
        _mtx->lock(); /* block regardless */  
    /* --- CRITICAL SECTION --- */
    _my_impl_obj.push_front(_item); 
    _my_impl_obj.pop_back();     
    _incr_internal_counts();
    /* --- CRITICAL SECTION --- */
    _mtx->unlock();
} 

DATASTREAM_PRIMARY_TEMPLATE
template<typename T>
bool
DATASTREAM_PRIMARY_CLASS::_check_adj(int& end, 
                                     int& beg, 
                                     const std::deque<T,Allocator>& impl) const
{ 
    int sz = (int)impl.size(); /* O.K. sz can't be > INT_MAX  */
    if(_q_bound != sz)
        throw DataStreamSizeViolation("internal size/bounds violation", _q_bound, sz);      
    
    if(end < 0) 
        end += sz; 

    if(beg < 0) 
        beg += sz;

    if(beg >= sz || end >= sz || beg < 0 || end < 0)  
        throw DataStreamOutOfRange("adj index value out of range", sz, beg, end);    
    else if(beg > end)   
        throw DataStreamInvalidArgument("adjusted beging index > end index");

    return true;
}

DATASTREAM_PRIMARY_TEMPLATE
void
DATASTREAM_PRIMARY_CLASS::_incr_internal_counts()
{
    long long penult = (long long)(_q_bound) -1; 

    if(_q_count < _q_bound)
        ++_q_count;
        
    if(*_mark_count == penult)
        *_mark_is_dirty = true;
    else if(*_mark_count < penult)
        ++(*_mark_count);
    else{/* 
          * WE CANT THROW so attempt to get _mark_count back in line;
          * also set dirty flag to alert caller of possible data issue
          */
        --(*_mark_count);
        *_mark_is_dirty = true;
    }
}

DATASTREAM_PRIMARY_TEMPLATE
template<typename ImplTy, typename DestTy> 
size_t 
DATASTREAM_PRIMARY_CLASS::_copy_to_ptr(ImplTy& impl, 
                                       DestTy* dest, 
                                       size_t sz, 
                                       unsigned int end, 
                                       unsigned int beg) const
{  
    auto b_iter = impl.cbegin() + beg;
    auto e_iter = impl.cbegin() + std::min<size_t>(sz+beg, std::min<size_t>(++end, _q_count));
    
    return (b_iter < e_iter) ? (std::copy(b_iter, e_iter, dest) - dest) : 0;     
}

DATASTREAM_PRIMARY_TEMPLATE
DATASTREAM_PRIMARY_CLASS::DataStream(size_t sz)
    : 
        _my_impl_obj(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1)),
        _q_bound(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1)),
        _q_count(0),
        _mark_count(new long long(-1)),
        _mark_is_dirty(new bool(false)),      
        _push_has_priority(true),
        _mtx(new std::recursive_mutex)
    {      
    }

DATASTREAM_PRIMARY_TEMPLATE
DATASTREAM_PRIMARY_CLASS::DataStream(const typename DATASTREAM_PRIMARY_CLASS::_my_ty & stream)
    : 
        _my_impl_obj(stream._my_impl_obj),
        _q_bound(stream._q_bound),
        _q_count(stream._q_count),
        _mark_count(new long long(*(stream._mark_count))),
        _mark_is_dirty(new bool(*(stream._mark_is_dirty))),    
        _push_has_priority(true),
        _mtx(new std::recursive_mutex)
    {      
    }

DATASTREAM_PRIMARY_TEMPLATE
DATASTREAM_PRIMARY_CLASS::DataStream(typename DATASTREAM_PRIMARY_CLASS::_my_ty && stream)
    : 
        _my_impl_obj(std::move(stream._my_impl_obj)),
        _q_bound(stream._q_bound),
        _q_count(stream._q_count),   
        _mark_count(stream._mark_count),
        _mark_is_dirty(stream._mark_is_dirty),    
        _push_has_priority(true),
        _mtx(stream._mtx) // ??
    {      
        stream._mark_count = nullptr;
        stream._mark_is_dirty = nullptr;
        stream._mtx = nullptr;
    }

DATASTREAM_PRIMARY_TEMPLATE
DATASTREAM_PRIMARY_CLASS::~DataStream()
{
    if(_mtx) 
        delete _mtx;  

    if(_mark_count) 
        delete _mark_count;

    if(_mark_is_dirty) 
        delete _mark_is_dirty;
 }


DATASTREAM_PRIMARY_TEMPLATE
size_t
DATASTREAM_PRIMARY_CLASS::bound_size(size_t sz)
{
    sz = std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1);

    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    _my_impl_obj.resize(sz);

    if(sz < _q_bound){
        /* IF bound is 'clipped' from the left(end) */
        _my_impl_obj.shrink_to_fit();  

        if( (long long)sz <= *_mark_count ){
            /* IF marker is 'clipped' from the left(end) */
            *_mark_count = (long long)sz -1;
            *_mark_is_dirty = true;
        }
    }    

    _q_bound = sz;
    if(sz < _q_count) /* IF count is 'clipped' from the left(end) */
        _q_count = sz;

    return _q_bound;
    /* --- CRITICAL SECTION --- */
}  


DATASTREAM_PRIMARY_TEMPLATE
long long
DATASTREAM_PRIMARY_CLASS::copy_from_marker(Ty* dest, 
                                           size_t sz,              
                                           int beg = 0, 
                                           typename DATASTREAM_PRIMARY_CLASS::secondary_ty* sec = nullptr) const 
{         
    /* 1) we need to cache mark vals before copy changes state
       2) adjust beg here; _check_adj requires a ref that we can't pass
       3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX */   

    long long copy_sz, req_sz;    

    _yld_to_push();
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    bool was_dirty = *_mark_is_dirty;
          
    if(beg < 0)       
        beg += (int)size();

    req_sz = *_mark_count - (long long)beg + 1;     
    if(beg < 0 || req_sz < 1) 
        /* if beg is still invalid or > marker ... CALLER'S PROBLEM
           req_sz needs to account for inclusive range by adding 1 */
        return 0;

    /* CAREFUL: we cant have a negative *_mark_count past this point */
    copy_sz = (long long)copy(dest, sz, (int)(*_mark_count), beg, sec);          

    if(was_dirty || copy_sz < req_sz)
        /* IF mark is dirty (i.e hits back of stream) or we
           don't copy enough(sz is too small) return negative size */            
        copy_sz *= -1;      

    return copy_sz;
    /* --- CRITICAL SECTION --- */
}
  
DATASTREAM_PRIMARY_TEMPLATE
long long
DATASTREAM_PRIMARY_CLASS::copy_from_marker(char** dest, 
                                           size_t dest_sz, 
                                           size_t str_sz,                
                                           int beg = 0, 
                                           typename DATASTREAM_PRIMARY_CLASS::secondary_ty* sec = nullptr) const 
{  
   /* 1) we need to cache mark vals before copy changes state
      2) adjust beg here; _check_adj requires a ref that we can't pass
      3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX */   

    long long copy_sz, req_sz;    

    _yld_to_push();
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    bool was_dirty = *_mark_is_dirty;
          
    if(beg < 0)       
        beg += (int)size();

    req_sz = *_mark_count - (long long)beg + 1;     
    if(beg < 0 || req_sz < 1) 
        /* if beg is still invalid or > marker ... CALLER'S PROBLEM
           req_sz needs to account for inclusive range by adding 1 */
        return 0;

    /* CAREFUL: we cant have a negative *_mark_count past this point */
    copy_sz = (long long)copy(dest, dest_sz, str_sz, (int)(*_mark_count), beg, sec);          

    if(was_dirty || copy_sz < req_sz)
        /* IF mark is dirty (i.e hits back of stream) or we
           don't copy enough(sz is too small) return negative size */            
        copy_sz *= -1;      

    return copy_sz;
    /* --- CRITICAL SECTION --- */
}
    
DATASTREAM_PRIMARY_TEMPLATE
size_t
DATASTREAM_PRIMARY_CLASS::copy(Ty* dest, 
                               size_t sz, 
                               int end = -1, 
                               int beg = 0, 
                               typename DATASTREAM_PRIMARY_CLASS::secondary_ty* sec = nullptr) const 
{  
    size_t ret;

    static_assert(!std::is_same<Ty,char>::value, "copy doesn't accept char*");   

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");

    _yld_to_push();
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    _check_adj(end, beg, _my_impl_obj);           

    if(end == beg){
        *dest = beg ? _my_impl_obj.at(beg) : _my_impl_obj.front();
        ret = 1;
    }else 
        ret = _copy_to_ptr(_my_impl_obj, dest, sz, end, beg);     
    
    *_mark_count = beg - 1;   
    *_mark_is_dirty = false;

    return ret;
    /* --- CRITICAL SECTION --- */
}
    
DATASTREAM_PRIMARY_TEMPLATE
size_t
DATASTREAM_PRIMARY_CLASS::copy(char** dest, 
                               size_t dest_sz, 
                               size_t str_sz, 
                               int end = -1, 
                               int beg = 0, 
                               typename DATASTREAM_PRIMARY_CLASS::secondary_ty* sec = nullptr) const 
{  /* 
    * slow(er), has to go thru generic_ty to get strings 
    * note: if sz <= gstr.length() the string is truncated 
    */
    _my_impl_ty::const_iterator b_iter;
    _my_impl_ty::const_iterator e_iter;
    size_t i;
 
    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");

    _yld_to_push();    
    _my_lock_guard_type lock(*_mtx); 
    /* --- CRITICAL SECTION --- */
    _check_adj(end, beg, _my_impl_obj);            

    b_iter = _my_impl_obj.cbegin() + beg; 
    e_iter = _my_impl_obj.cbegin() + std::min<size_t>(++end, _q_count);

    for( i = 0; 
         (i < dest_sz) && (b_iter < e_iter); 
         ++b_iter, ++i )
    {       
        std::string gstr = generic_ty(*b_iter).as_string();        
        strncpy_s(dest[i], str_sz, gstr.c_str(), std::min<size_t>(str_sz-1, gstr.length()));                  
    }  

    *_mark_count = beg - 1; 
    *_mark_is_dirty = false;

    return i;
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_PRIMARY_TEMPLATE
typename DATASTREAM_PRIMARY_CLASS::generic_ty
DATASTREAM_PRIMARY_CLASS::operator[](int indx) const
{
    int dummy = 0;

    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    if(!indx){ 
        /* optimize for indx == 0 */
        *_mark_count = -1;
        *_mark_is_dirty = false;
        return generic_ty(_my_impl_obj.front()); 
    }

    _check_adj(indx, dummy, _my_impl_obj); 

    *_mark_count = indx - 1; 
    *_mark_is_dirty = false;

    return generic_ty(_my_impl_obj.at(indx));   
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_PRIMARY_TEMPLATE
typename DATASTREAM_PRIMARY_CLASS::both_ty
DATASTREAM_PRIMARY_CLASS::both(int indx) const            
{  
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    return both_ty(operator[](indx), secondary_ty());
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_PRIMARY_TEMPLATE
typename DATASTREAM_PRIMARY_CLASS::generic_vector_ty
DATASTREAM_PRIMARY_CLASS::vector(int end = -1, int beg = 0) const 
{
    _my_impl_ty::const_iterator b_iter;
    _my_impl_ty::const_iterator e_iter;    
    generic_vector_ty tmp;  
    
    _yld_to_push();    
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    _check_adj(end, beg, _my_impl_obj);
        
    b_iter = _my_impl_obj.cbegin() + beg;
    e_iter = _my_impl_obj.cbegin() + std::min<size_t>(++end, _q_count);  
    
    if(b_iter < e_iter){          
        std::transform(
            b_iter, 
            e_iter, 
            /* have to use slower insert_iterator approach, 
               generic_ty doesn't allow default construction */
            std::insert_iterator<generic_vector_ty>(tmp, tmp.begin()), 
            [](Ty x){ return generic_ty(x); }
        );   
    }

    *_mark_count = beg - 1; 
    *_mark_is_dirty = false;
     
    return tmp;  
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_PRIMARY_TEMPLATE
typename DATASTREAM_PRIMARY_CLASS::secondary_vector_ty
DATASTREAM_PRIMARY_CLASS::secondary_vector(int end = -1, int beg = 0) const
{        
    _check_adj(end, beg, _my_impl_obj);                      
    return secondary_vector_ty(std::min< size_t >(++end - beg, _q_count));
}


DATASTREAM_SECONDARY_TEMPLATE
void
DATASTREAM_SECONDARY_CLASS::_push(const Ty _item, 
                                  const typename DATASTREAM_SECONDARY_CLASS::secondary_ty sec) 
{  
    _push_has_priority = _mtx->try_lock();
    if(!_push_has_priority) 
        _mtx->lock();
    /* --- CRITICAL SECTION --- */
    _my_base_ty::_my_impl_obj.push_front(_item); 
    _my_base_ty::_my_impl_obj.pop_back();
    _my_sec_impl_obj.push_front(std::move(sec));
    _my_sec_impl_obj.pop_back();
    _incr_internal_counts();
    /* --- CRITICAL SECTION --- */
    _mtx->unlock();
} 

DATASTREAM_SECONDARY_TEMPLATE
DATASTREAM_SECONDARY_CLASS::DataStream(size_t sz)
    : 
        _my_sec_impl_obj(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1)),
        _my_base_ty(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1))
    {
    }

DATASTREAM_SECONDARY_TEMPLATE
DATASTREAM_SECONDARY_CLASS::DataStream(const typename DATASTREAM_SECONDARY_CLASS::_my_ty & stream)
    : 
        _my_sec_impl_obj(stream._my_sec_impl_obj),
        _my_base_ty(stream)
    { 
    }

DATASTREAM_SECONDARY_TEMPLATE
DATASTREAM_SECONDARY_CLASS::DataStream(typename DATASTREAM_SECONDARY_CLASS::_my_ty && stream)
    : 
        _my_sec_impl_obj(std::move(stream._my_sec_impl_obj)),
        _my_base_ty(std::move(stream))
    {
    }


DATASTREAM_SECONDARY_TEMPLATE
size_t
DATASTREAM_SECONDARY_CLASS::bound_size(size_t sz)
{
    sz = std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1);

    _my_lock_guard_type lock(*_mtx);   
    /* --- CRITICAL SECTION --- */
    _my_sec_impl_obj.resize(sz);
    if (sz < _q_count)
        _my_sec_impl_obj.shrink_to_fit();  

    return _my_base_ty::bound_size(sz);   
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_SECONDARY_TEMPLATE
size_t
DATASTREAM_SECONDARY_CLASS::copy(Ty* dest, 
                                 size_t sz, 
                                 int end = -1, 
                                 int beg = 0, 
                                 typename DATASTREAM_SECONDARY_CLASS::secondary_ty* sec = nullptr) const 
{   
    size_t ret;

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");

    _my_lock_guard_type lock(*_mtx); 
    /* --- CRITICAL SECTION --- */
    ret = _my_base_ty::copy(dest, sz, end, beg); /*_mark_count reset by _my_base_ty*/
      
    if(!sec)
        return ret;
        
    _check_adj(end, beg, _my_sec_impl_obj); /*repeat to update index vals */ 
 
    if(end == beg){  
        *sec = beg ? _my_sec_impl_obj.at(beg) : _my_sec_impl_obj.front();
        ret = 1;
    }else  
        ret = _copy_to_ptr(_my_sec_impl_obj, sec, sz, end, beg);  

    /* check ret vs. the return value of _my_base_ty::copy for consistency ? */
    return ret;
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_SECONDARY_TEMPLATE
size_t
DATASTREAM_SECONDARY_CLASS::copy(char** dest, 
                                 size_t dest_sz, 
                                 size_t str_sz, 
                                 int end = -1, 
                                 int beg = 0, 
                                 typename DATASTREAM_SECONDARY_CLASS::secondary_ty* sec = nullptr) const 
{    
    size_t ret;

    if(!dest)
        throw DataStreamInvalidArgument("NULL dest argument");

    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    ret = _my_base_ty::copy(dest, dest_sz, str_sz, end, beg);

    if(!sec)
        return ret;
    
    _check_adj(end, beg, _my_sec_impl_obj); /*repeat to update index vals*/ 

    if(end == beg){
        *sec = beg ? _my_sec_impl_obj.at(beg) : _my_sec_impl_obj.front();
        ret = 1;
    }else
        ret = _copy_to_ptr(_my_sec_impl_obj, sec, dest_sz, end, beg);    

    /* check ret vs. the return value of _my_base_ty::copy for consistency ? */
    /* --- CRITICAL SECTION --- */
    return ret;
}
  
DATASTREAM_SECONDARY_TEMPLATE
typename DATASTREAM_SECONDARY_CLASS::both_ty
DATASTREAM_SECONDARY_CLASS::both(int indx) const            
{    
    int dummy = 0;   
   
    _my_lock_guard_type lock(*_mtx);  
    /* --- CRITICAL SECTION --- */
    generic_ty gen = operator[](indx); /* _mark_count reset by _my_base_ty */
    if(!indx)
        return both_ty(gen, _my_sec_impl_obj.front());

    _check_adj(indx, dummy, _my_sec_impl_obj); 
     
    return both_ty(gen, _my_sec_impl_obj.at(indx));
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_SECONDARY_TEMPLATE
void
DATASTREAM_SECONDARY_CLASS::secondary(typename DATASTREAM_SECONDARY_CLASS::secondary_ty* dest, 
                                      int indx) const
{
    int dummy = 0;

    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    _check_adj(indx, dummy, _my_sec_impl_obj);

    if(!indx)
        *dest = _my_sec_impl_obj.front();
    else
        *dest = _my_sec_impl_obj.at(indx);  

    *_mark_count = indx - 1; /* _mark_count NOT reset by _my_base_ty */
    *_mark_is_dirty = false;
    /* --- CRITICAL SECTION --- */
}

DATASTREAM_SECONDARY_TEMPLATE
typename DATASTREAM_SECONDARY_CLASS::secondary_vector_ty
DATASTREAM_SECONDARY_CLASS::secondary_vector(int end = -1, int beg = 0) const
{
    _my_sec_impl_ty::const_iterator b_iter;
    _my_sec_impl_ty::const_iterator e_iter;
    _my_sec_impl_ty::const_iterator::difference_type ndiff;
    secondary_vector_ty tmp; 
     
    _yld_to_push();    
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    _check_adj(end, beg, _my_sec_impl_obj);  
        
    b_iter = _my_sec_impl_obj.cbegin() + beg;
    e_iter = _my_sec_impl_obj.cbegin() + std::min<size_t>(++end, _q_count);  
    ndiff = e_iter - b_iter;

    if(ndiff > 0){ 
      /* do this manually; insert iterators too slow */
        tmp.resize(ndiff); 
        std::copy(b_iter, e_iter, tmp.begin());
    }

    *_mark_count = beg - 1; /* _mark_count NOT reset by _my_base_ty */
    *_mark_is_dirty = false;

    return tmp;  
    /* --- CRITICAL SECTION --- */
}
