/* 
Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >

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
#define STR_DATA_SZ ((unsigned long)0xFF)
#endif

#include <deque>
#include <string>
#include <vector>
#include <mutex>

/*    
 *  tosdb_data_stream is interfaced differently than the typical templatized 
 *  cotainer. Since it provides its own java-style interface, and exceptions, 
 *  it should also provide a non-templatized namespace for access to generic, 
 *  static 'stuff' while making it clear that an interface is used / expected.
 *   
 *  The interface is of type ::Interface< type, type >
 *  The container is of type ::Object< type, type, type, bool, type > 
 *
 *  Hard-coded a max bound size as INT_MAX to avoid some of the corner cases.
*/

namespace tosdb_data_stream {

class error : public std::exception{
public:
    error( const char* info ) 
        : 
        std::exception( info ) 
        {
        }
};

class type_error : public error{
public:
    type_error( const char* info ) 
        : 
        error( info ) 
        { 
        }
};

class size_violation : public error{
public:
    const size_t boundSize, dequeSize;   
         
    size_violation( const char* msg, size_t bSz, size_t dSz )
        : 
        error( msg ),
        boundSize( bSz ),
        dequeSize( dSz )                
        {                
        }
};

/* 
 *  can't make std::exception a virtual base from the 
 *  stdexcept path so double construction of std::exception 
*/
class out_of_range : public error, public std::out_of_range {            
public:
    const int size, beg, end;

    out_of_range( const char* msg, int sz, int beg, int end )
        : 
        std::out_of_range( msg ), 
        error( msg ),                            
        size( sz ), 
        beg( beg ), 
        end( end )
        {                
        }

    virtual const char* what() const 
    { 
        return error::what(); 
    }        
};

class invalid_argument : public error, public std::invalid_argument {
public: 
    invalid_argument( const char* msg )
        : 
        std::invalid_argument( msg ), 
        error( msg )
        {                
        }

    virtual const char* what() const 
    {
        return error::what();
    }
};
    

template< typename SecTy, typename GenTy >
class Interface {
/*    
 * The interface to tosdb_data_stream     
 */
public:

    typedef GenTy                     generic_type;
    typedef SecTy                     secondary_type;
    typedef std::pair< GenTy, SecTy>  both_type;
    typedef std::vector< GenTy >      generic_vector_type;
    typedef std::vector< SecTy >      secondary_vector_type;

    static const size_t MAX_BOUND_SIZE = INT_MAX;

private:

    template < typename _inTy >
    void _throw_type_error( const char* method, bool fThru = false ) const
    {         
        std::ostringstream msgStrm;  
        msgStrm << "tosdb_data_stream: Invalid argument < "  
                << ( fThru ? "UNKNOWN" : typeid(_inTy).name() ) 
                << " > passed to method " << method 
                <<" for this template instantiation.";
        throw type_error( msgStrm.str().c_str() );        
    }

    template < typename _inTy, typename _outTy >
    size_t _copy( _outTy* dest, 
                  size_t sz, 
                  int end, 
                  int beg, 
                  secondary_type* sec ) const
    {    
        size_t ret;
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");     
    
        std::unique_ptr<_inTy,void(*)(_inTy*)> tmp( new _inTy[sz], 
                                                    [](_inTy* _ptr){ 
                                                        delete[] _ptr; 
                                                    } );
        ret = copy( tmp.get(), sz, end, beg, sec );
        for( size_t i = 0; i < sz; ++i )            
            dest[i] = (_outTy)tmp.get()[i];                                

        return ret;
    }  

    template < typename _inTy, typename _outTy >
    long long _copy_using_atomic_marker( _outTy* dest, 
                                         size_t sz,                 
                                         int beg, 
                                         secondary_type* sec ) const
    {    
        long long ret;
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");     
    
        std::unique_ptr<_inTy,void(*)(_inTy*)> tmp( new _inTy[sz], 
                                                    [](_inTy* _ptr){
                                                        delete[] _ptr;
                                                    } );
        ret = copy_from_marker( tmp.get(), sz, beg, sec );
        for( size_t i = 0; i < sz; ++i )            
            dest[i] = (_outTy)tmp.get()[i];  

        return ret;
    }  

protected:

    unsigned short _count1;

    Interface()
        : _count1( 0 )
        { 
        }

public:

    virtual size_t                bound_size() const = 0;
    virtual size_t                bound_size( size_t ) = 0;
    virtual size_t                size() const = 0;
    virtual bool                  empty() const = 0;    
    virtual bool                  uses_secondary() const = 0;
    virtual long long             marker_position() const = 0;
    virtual bool                  is_marker_dirty() const = 0;
    virtual generic_type          operator[]( int ) const = 0;
    virtual both_type             both( int ) const = 0;    
    virtual generic_vector_type   vector( int end = -1, int beg = 0 ) const = 0;
    virtual secondary_vector_type secondary_vector( int end = -1, 
                                                    int beg = 0 ) const = 0;    
    
    virtual void push( const generic_type& obj, 
                       secondary_type&& sec = secondary_type() ) = 0;

    virtual void secondary( secondary_type* dest, int indx ) const 
    { 
        dest = nullptr; /* SHOULD WE THROW? */        
    }
    
    /* 
     *  Avoid constructing GenTy if possible: the following mess provides
     * something of a recursive / drop-thru, safe, type-finding mechanism 
     *  to do that. Throws if it can't reconcile the passed type at runtime( 
     *  obviously this is not ideal, but seems to be necessary for what's 
     *  trying to be accomplished) 
     */
#define virtual_void_push_2arg_DROP( _inTy, _outTy ) \
virtual void push( const _inTy val, secondary_type&& sec = secondary_type()) \
{ \
    push( (_outTy)val, std::move(sec) ); \
} 
#define virtual_void_push_2arg_BREAK( _inTy ) \
virtual void push( const _inTy val, secondary_type&& sec = secondary_type()) \
{ \
    push( std::to_string(val) , std::move(sec) ); \
} 
#define virtual_void_push_2arg_LOOP( _inTy, _loopOnC1 ) \
virtual void push( const _inTy str, secondary_type&& sec = secondary_type()) \
{ \
    if( _count1++ ){ \
        _count1 = 0; \
        _throw_type_error< const _inTy >( "->push()", true ); \
    } \
    push( _loopOnC1, std::move(sec) ); \
} 
    virtual_void_push_2arg_DROP( float, double )
    virtual_void_push_2arg_BREAK( double )
    virtual_void_push_2arg_DROP( unsigned char, unsigned short )
    virtual_void_push_2arg_DROP( unsigned short, unsigned int )
    virtual_void_push_2arg_DROP( unsigned int, unsigned long )
    virtual_void_push_2arg_DROP( unsigned long, unsigned long long )
    virtual_void_push_2arg_BREAK( unsigned long long )
    virtual_void_push_2arg_DROP( char, short )
    virtual_void_push_2arg_DROP( short, int )
    virtual_void_push_2arg_DROP( int, long )
    virtual_void_push_2arg_DROP( long, long long )
    virtual_void_push_2arg_BREAK( long long )
    virtual_void_push_2arg_LOOP( std::string, str.c_str() )
    virtual_void_push_2arg_LOOP( char*, std::string( str ) )

#define virtual_void_copy_2arg_DROP( _inTy, _outTy ) \
virtual size_t copy( _inTy* dest, size_t sz, int end = -1, int beg = 0, \
                     secondary_type* sec = nullptr) const \
{ \
    return _copy< _outTy >( dest, sz, end, beg, sec ); \
} 
#define virtual_void_copy_2arg_BREAK( _inTy, _dropBool ) \
virtual size_t copy( _inTy* dest, size_t sz, int end = -1, int beg = 0, \
                     secondary_type* sec = nullptr) const \
{ \
    _throw_type_error< _inTy* >( "->copy()", _dropBool ); \
    return 0; \
}

    virtual_void_copy_2arg_DROP( long long, long )
    virtual_void_copy_2arg_DROP( long, int )
    virtual_void_copy_2arg_DROP( int, short )
    virtual_void_copy_2arg_DROP( short, char )
    virtual_void_copy_2arg_BREAK( char, true )
    virtual_void_copy_2arg_DROP( unsigned long long, unsigned long )
    virtual_void_copy_2arg_DROP( unsigned long, unsigned int )
    virtual_void_copy_2arg_DROP( unsigned int, unsigned short )
    virtual_void_copy_2arg_DROP( unsigned short, unsigned char )
    virtual_void_copy_2arg_BREAK( unsigned char, true )
    virtual_void_copy_2arg_DROP( double, float )
    virtual_void_copy_2arg_BREAK( float, false ) 

    virtual size_t copy( char** dest, 
                         size_t destSz, 
                         size_t strSz, 
                         int end = -1, 
                         int beg = 0 , 
                         secondary_type* sec = nullptr ) const 
    { 
        _throw_type_error< std::string* >( "->copy()", false );    
        return 0;
    }

    virtual size_t copy( std::string* dest, 
                         size_t sz, 
                         int end = -1, 
                         int beg = 0, 
                         secondary_type* sec = nullptr ) const
    {
        size_t ret;
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        auto dstr = [sz]( char** pptr){ DeallocStrArray( pptr, sz); };
        char** sarray = AllocStrArray(sz,STR_DATA_SZ);

        std::unique_ptr<char*,decltype(dstr)> sptr( sarray, dstr);

        ret = this->copy( sptr.get(), sz, STR_DATA_SZ , end, beg, sec);                
        std::copy_n( sptr.get(), sz, dest ); 

        return ret;
    }

#define virtual_void_marker_copy_2arg_DROP( _inTy, _outTy ) \
virtual long long copy_from_marker( _inTy* dest, size_t sz, int beg = 0, \
                                    secondary_type* sec = nullptr) const \
{ \
    return _copy_using_atomic_marker< _outTy >( dest, sz, beg, sec ); \
} 
#define virtual_void_marker_copy_2arg_BREAK( _inTy, _dropBool ) \
virtual long long copy_from_marker( _inTy* dest, size_t sz, int beg = 0, \
                                    secondary_type* sec = nullptr) const \
{ \
    _throw_type_error< _inTy* >( "->copy_from_marker()", _dropBool ); \
    return 0; \
}

    virtual_void_marker_copy_2arg_DROP( long long, long )
    virtual_void_marker_copy_2arg_DROP( long, int )
    virtual_void_marker_copy_2arg_DROP( int, short )
    virtual_void_marker_copy_2arg_DROP( short, char )
    virtual_void_marker_copy_2arg_BREAK( char, true )
    virtual_void_marker_copy_2arg_DROP( unsigned long long, unsigned long )
    virtual_void_marker_copy_2arg_DROP( unsigned long, unsigned int )
    virtual_void_marker_copy_2arg_DROP( unsigned int, unsigned short )
    virtual_void_marker_copy_2arg_DROP( unsigned short, unsigned char )
    virtual_void_marker_copy_2arg_BREAK( unsigned char, true )
    virtual_void_marker_copy_2arg_DROP( double, float )
    virtual_void_marker_copy_2arg_BREAK( float, false ) 

    virtual long long copy_from_marker( char** dest, 
                                        size_t destSz, 
                                        size_t strSz,                         
                                        int beg = 0, 
                                        secondary_type* sec = nullptr) const 
    { 
        _throw_type_error< std::string* >( "->copy_from_marker()", false );  
        return 0;
    }

    virtual long long copy_from_marker( std::string* dest, 
                                        size_t sz,                                         
                                        int beg = 0, 
                                        secondary_type* sec = nullptr) const
    {
        long long ret;
        if( !dest )
            throw invalid_argument( 
                "->copy_from_marker(): *dest argument can not be null" );

        auto dstr = [sz]( char** pptr){ DeallocStrArray( pptr, sz); };
        char** sarray= AllocStrArray(sz,STR_DATA_SZ);

        std::unique_ptr<char*,decltype(dstr)> sptr( sarray, dstr);

        ret = this->copy_from_marker( sptr.get(), sz, STR_DATA_SZ, beg, sec);                
        std::copy_n( sptr.get(), sz, dest );   

        return ret;
    }
    
    virtual ~Interface()
        {
        }
};

template < typename Ty,
           typename SecTy,
           typename GenTy,            
           bool UseSecondary = false,
           typename Allocator = std::allocator<Ty> >
class Object
    : public Interface<SecTy, GenTy>{
/*    
 * The container object w/o secondary deque 
 */

    class{
        static const bool valid = GenTy::Type_Check<Ty>::value;    
        static_assert( valid, "tosdb_data_stream::object can not be compiled; "
                              "Ty failed GenTy's type-check;" ); 
    }_inst_check_;    
    
    typedef Object<Ty,SecTy,GenTy,UseSecondary,Allocator> _my_type;
    typedef Interface<SecTy,GenTy>                        _my_base_type;
    typedef std::deque<Ty,Allocator>                      _my_impl_type;            
    typedef typename _my_impl_type::const_iterator
                                  ::difference_type       _my_iterdiff_type;    

    _my_type& operator=(const _my_type &);
    
    void _push(const Ty _item) 
    {   /* if can't obtain lock indicate other threads should yield to us */       
        _push_has_priority = _mtx->try_lock(); /*O.K. push/pop doesn't throw*/
        if( !_push_has_priority )
            _mtx->lock(); /* block regardless */  

        _my_impl_obj.push_front(_item); 
        _my_impl_obj.pop_back();       
        _incr_intrnl_counts();

        _mtx->unlock();
    } 

protected:

    typedef std::lock_guard<std::recursive_mutex >  _my_lock_guard_type;
    
    std::recursive_mutex* const  _mtx; 
    volatile bool                _push_has_priority;
    bool* const                  _mark_is_dirty;    
    size_t                       _q_count, _q_bound;
    long long* const             _mark_count;    
    _my_impl_type                _my_impl_obj;

    void inline _yld_to_push() const
    {    
        if( !_push_has_priority )            
            std::this_thread::yield();
    }

    template< typename T > 
    bool _check_adj( int& end, 
                     int& beg, 
                     const std::deque< T, Allocator >& impl ) const
    {  /* O.K. sz can't be > INT_MAX  */
        int sz = (int)impl.size();

        if ( _q_bound != sz )    
            throw size_violation( 
                "Internal size/bounds violation in tosdb_data_stream", 
                _q_bound, sz );          
  
        if ( end < 0 ) end += sz; 
        if ( beg < 0 ) beg += sz;
        if ( beg >= sz || end >= sz || beg < 0 || end < 0 )    
            throw out_of_range( 
                "adj index value out of range in tosdb_data_stream",
                sz, beg, end );
        else if ( beg > end )     
            throw invalid_argument( 
                "adj beg index value > end index value in tosdb_data_stream" );

        return true;
    }

    void _incr_intrnl_counts()
    {
        long long penult = (long long)_q_bound -1; 

        if( _q_count < _q_bound )
            ++_q_count;
                
        if( *_mark_count == penult )
            *_mark_is_dirty = true;
        else if( *_mark_count < penult )
            ++(*_mark_count);
        else{        
           /* 
            * WE CANT THROW so attempt to get _mark_count back in line
            * also set overset flag to alert caller of possible data issue
            */
            --(*_mark_count);
            *_mark_is_dirty = true;
        }
    }

    template< typename ImplTy, typename DestTy > 
    size_t _copy_to_ptr( ImplTy& impl, 
                         DestTy* dest, 
                         size_t sz, 
                         unsigned int end, 
                         unsigned int beg) const
    {    
        ImplTy::const_iterator bIter = impl.cbegin() + beg;
        ImplTy::const_iterator eIter = 
            impl.cbegin() + 
            std::min< size_t >( sz + beg, std::min< size_t >( ++end, _q_count ));
        
        return (bIter < eIter) ? (std::copy( bIter, eIter, dest ) - dest) : 0;       
    }

public:

    typedef _my_base_type  interface_type;
    typedef Ty             value_type;

    Object(size_t sz )
        : 
        _my_impl_obj( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1) ),
        _q_bound( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1) ),
        _q_count( 0 ),
        _mark_count( new long long(-1) ),
        _mark_is_dirty( new bool(false) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {            
        }

    Object(const _my_type & stream )
        : 
        _my_impl_obj( stream._my_impl_obj ),
        _q_bound( stream._q_bound ),
        _q_count( stream._q_count ),
        _mark_count( new long long(*stream._mark_count) ),
        _mark_is_dirty( new bool(*stream._mark_is_dirty) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {            
        }

    Object( _my_type && stream)
        : 
        _my_impl_obj( std::move( stream._my_impl_obj) ),
        _q_bound( stream._q_bound ),
        _q_count( stream._q_count ),   
        _mark_count( new long long(*stream._mark_count) ),
        _mark_is_dirty( new bool(*stream._mark_is_dirty) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {                
        }

    virtual ~Object()
        {
            delete this->_mtx;  
            delete this->_mark_count;
            delete this->_mark_is_dirty;
        }

    bool       inline empty()           const { return _my_impl_obj.empty(); }
    size_t     inline size()            const { return _q_count; }
    bool       inline uses_secondary()  const { return false; }
    bool       inline is_marker_dirty() const { return *_mark_is_dirty; }
    long long  inline marker_position() const { return *_mark_count; }   
    size_t     inline bound_size()      const { return _q_bound; }
   
    size_t bound_size( size_t sz )
    {
        sz = std::max<size_t>( std::min<size_t>(sz,MAX_BOUND_SIZE),1);

        _my_lock_guard_type lock( *_mtx );
        _my_impl_obj.resize(sz);

        if( sz < _q_bound ){
            /* IF bound is 'clipped' from the left(end) */
            _my_impl_obj.shrink_to_fit();    
            if( (long long)sz <= *_mark_count ){
                /* IF marker is 'clipped' from the left(end) */
                *_mark_count = (long long)sz -1;
                *_mark_is_dirty = true;
            }
        }        
        _q_bound = sz;
        if( sz < _q_count )
            /* IF count is 'clipped' from the left(end) */
            _q_count = sz;

        return _q_bound;
    }    
        
    void push(const Ty val, secondary_type&& sec = secondary_type() ) 
    {
        _count1 = 0;        
        _push( val);        
    }

    void push( const generic_type& gen, 
               secondary_type&& sec = secondary_type() )
    {
        _count1 = 0;
        _push( (Ty)gen );        
    }
    
    /*
     *
     *  TODO handle neg returns, should we keep old mark to 
     * give caller a chance to recall if the pass small buffer ???    
     *
     */

    long long copy_from_marker( Ty* dest, 
                                size_t sz,                            
                                int beg = 0, 
                                secondary_type* sec = nullptr) const 
    {   /*           
        * 1) we need to cache mark vals before copy changes state
        * 2) adjust beg here; _check_adj requires a ref that we can't pass
        * 3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX
        */   
        long long copy_sz, req_sz;        

        _yld_to_push();
        _my_lock_guard_type lock( *_mtx );

        bool was_dirty = *_mark_is_dirty;
                  
        if( beg < 0 )             
            beg += (int)size();

        req_sz = *_mark_count - (long long)beg + 1;         
        if( beg < 0 || req_sz < 1) 
            /*
             * if beg is still invalid or > marker ... CALLER'S PROBLEM
             * req_sz needs to account for inclusive range by adding 1
             */
            return 0;

        /* CAREFUL: we cant have a negative *_mark_count past this point */
        copy_sz = (long long)copy( dest, sz, *_mark_count, beg, sec);                  

        if( was_dirty || copy_sz < req_sz )
           /*
            * IF mark is dirty (i.e hits back of stream) or we
            * don't copy enough(sz is too small) return negative size
            */                      
           copy_sz *= -1;            

        return copy_sz;
    }
    
    long long copy_from_marker( char** dest, 
                                size_t destSz, 
                                size_t strSz,                              
                                int beg = 0, 
                                secondary_type* sec = nullptr) const 
    {   /*           
        * 1) we need to cache mark vals before copy changes state
        * 2) adjust beg here; _check_adj requires a ref that we can't pass
        * 3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX
        */   
        long long copy_sz, req_sz;        

        _yld_to_push();
        _my_lock_guard_type lock( *_mtx );

        bool was_dirty = *_mark_is_dirty;
                  
        if( beg < 0 )             
            beg += (int)size();

        req_sz = *_mark_count - (long long)beg + 1;         
        if( beg < 0 || req_sz < 1) 
            /*
             * if beg is still invalid or > marker ... CALLER'S PROBLEM
             * req_sz needs to account for inclusive range by adding 1
             */
            return 0;

        /* CAREFUL: we cant have a negative *_mark_count past this point */
        copy_sz = (long long)copy( dest, destSz, strSz, *_mark_count, beg, sec);                  

        if( was_dirty || copy_sz < req_sz )
           /*
            * IF mark is dirty (i.e hits back of stream) or we
            * don't copy enough(sz is too small) return negative size
            */                      
           copy_sz *= -1;            

        return copy_sz;
    }
        
    size_t copy( Ty* dest, 
                 size_t sz, 
                 int end = -1, 
                 int beg = 0, 
                 secondary_type* sec = nullptr) const 
    {    
        size_t ret;
        static_assert( !std::is_same<Ty,char>::value, 
                       "->copy() accepts char**, not char*" );        
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        _yld_to_push();
        _my_lock_guard_type lock( *_mtx );
        _check_adj( end, beg, _my_impl_obj );                     

        if( end == beg ){
            *dest = beg ? _my_impl_obj.at(beg) : _my_impl_obj.front();
            ret = 1;
        }else 
            ret = _copy_to_ptr(_my_impl_obj,dest,sz,end,beg);         
        
        *_mark_count = beg - 1;   
        *_mark_is_dirty = false;

        return ret;
    }
        
    size_t copy( char** dest, 
                size_t destSz, 
                size_t strSz, 
                int end = -1, 
                int beg = 0, 
                secondary_type* sec = nullptr) const 
    {   /* 
         * slow(er), has to go thru generic_type to get strings 
         * note: if sz <= genS.length() the string is truncated 
         */
        _my_impl_type::const_iterator bIter, eIter;
        size_t i;
 
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        _yld_to_push();        
        _my_lock_guard_type lock( *_mtx );                    
        _check_adj( end, beg, _my_impl_obj);                        

        bIter = _my_impl_obj.cbegin() + beg; 
        eIter = _my_impl_obj.cbegin() + std::min< size_t >(++end, _q_count);

        for( i = 0; (i < destSz) && (bIter < eIter); ++bIter, ++i )
        {             
            std::string genS = generic_type( *bIter ).as_string();                
            strncpy_s( dest[i], strSz, genS.c_str(), 
                       std::min<size_t>( strSz-1, genS.length() ) );                                  
        }    

        *_mark_count = beg - 1; 
        *_mark_is_dirty = false;

        return i;
    }

    generic_type operator[]( int indx) const
    {
        int dummy = 0;

        _my_lock_guard_type lock( *_mtx );
        if ( !indx ){ /* optimize for indx == 0 */
            *_mark_count = -1;
            *_mark_is_dirty = false;
            return generic_type( _my_impl_obj.front() ); 
        }

        _check_adj( indx, dummy, _my_impl_obj ); 

        *_mark_count = indx - 1; 
        *_mark_is_dirty = false;

        return generic_type( _my_impl_obj.at(indx) );         
    }

    both_type both( int indx ) const                        
    {    
        _my_lock_guard_type lock( *_mtx );
        return both_type( operator[](indx), secondary_type() );
    }

    generic_vector_type 
    vector(int end = -1, int beg = 0) const 
    {
        _my_impl_type::const_iterator bIter, eIter;        
        generic_vector_type tmp;  
      
        _yld_to_push();        
        _my_lock_guard_type lock( *_mtx );
        _check_adj(end, beg, _my_impl_obj);
                
        bIter = _my_impl_obj.cbegin() + beg;
        eIter = _my_impl_obj.cbegin() + std::min< size_t >(++end, _q_count);  
      
        if( bIter < eIter )                   
            std::transform( bIter, eIter, 
                /* have to use slower insert_iterator approach, 
                   generic_type doesn't allow default construction */
                std::insert_iterator< generic_vector_type >( tmp, tmp.begin() ), 
                []( Ty x ){ return generic_type(x); } );   

        *_mark_count = beg - 1; 
        *_mark_is_dirty = false;
         
        return tmp;    
    }

    secondary_vector_type
    secondary_vector( int end = -1, int beg = 0 ) const
    {                
        _check_adj(end, beg, _my_impl_obj);                                            
        return secondary_vector_type( std::min< size_t >(++end - beg, _q_count));
    }

};

template < typename Ty,            
           typename SecTy,
           typename GenTy,
           typename Allocator >
class Object< Ty, SecTy, GenTy, true, Allocator >
    : public Object< Ty, SecTy, GenTy, false, Allocator >{
/*   
 * The container object w/ secondary deque   
 */
    typedef Object<Ty,SecTy,GenTy,true,Allocator>   _my_type;
    typedef Object<Ty,SecTy,GenTy,false,Allocator>  _my_base_type;
    typedef std::deque<SecTy,Allocator>             _my_sec_impl_type;
    
    _my_sec_impl_type _my_sec_impl_obj;    
    
    void _push(const Ty _item, const secondary_type&& sec) 
    {    
        _push_has_priority = _mtx->try_lock();
        if( !_push_has_priority )
            _mtx->lock();

        _my_base_type::_my_impl_obj.push_front(_item); 
        _my_base_type::_my_impl_obj.pop_back();
        _my_sec_impl_obj.push_front( std::move(sec) );
        _my_sec_impl_obj.pop_back();
        _incr_intrnl_counts();

        _mtx->unlock();
    } 

public:

    typedef Ty value_type;

    Object(size_t sz )
        : 
        _my_sec_impl_obj(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1)),
        _my_base_type( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1))
        {
        }

    Object(const _my_type & stream)
        : 
        _my_sec_impl_obj( stream._my_sec_impl_obj ),
        _my_base_type( stream )
        { 
        }

    Object(_my_type && stream)
        : 
        _my_sec_impl_obj( std::move( stream._my_sec_impl_obj) ),
        _my_base_type( std::move(stream) )
        {
        }

    size_t bound_size(size_t sz)
    {
        sz = std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1);

        _my_lock_guard_type lock( *_mtx );   
     
        _my_sec_impl_obj.resize(sz);
        if (sz < _q_count)
            _my_sec_impl_obj.shrink_to_fit();  
  
        return _my_base_type::bound_size(sz);        
    }

    bool inline uses_secondary(){ return true; }

    void push( const Ty val, secondary_type&& sec = secondary_type() ) 
    {        
        _count1 = 0;
        _push( val, std::move(sec) );   
    
    }

    void push( const generic_type& gen, 
               secondary_type&& sec = secondary_type() )
    {
        _count1 = 0;
        _push( (Ty)gen, std::move(sec) );
    }
    
    size_t copy( Ty* dest, 
                 size_t sz, 
                 int end = -1, 
                 int beg = 0, 
                 secondary_type* sec = nullptr) const 
    {     
        size_t ret;
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        _my_lock_guard_type lock( *_mtx ); 

        ret = _my_base_type::copy(dest, sz, end, beg); /*_mark_count reset by _my_base_type*/
          
        if( !sec )
            return ret;
                
        _check_adj( end, beg, _my_sec_impl_obj ); /*repeat to update index vals */ 
 
        if( end == beg ){    
            *sec = beg ? _my_sec_impl_obj.at(beg) : _my_sec_impl_obj.front();
            ret = 1;
        }else    
            ret = _copy_to_ptr( _my_sec_impl_obj, sec, sz, end, beg);    
        /*
         * check ret vs. the return value of _my_base_type::copy for consistency ??
        */
        return ret;
    }

    size_t copy( char** dest, 
                 size_t destSz, 
                 size_t strSz, 
                 int end = -1, 
                 int beg = 0, 
                 secondary_type* sec = nullptr) const 
    {        
        size_t ret;
        if( !dest  )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        _my_lock_guard_type lock( *_mtx );

        ret = _my_base_type::copy(dest, destSz, strSz, end, beg);

        if( !sec )
            return ret;
        
        _check_adj( end, beg, _my_sec_impl_obj ); /*repeat to update index vals*/ 

        if( end == beg ){
            *sec = beg ? _my_sec_impl_obj.at(beg) : _my_sec_impl_obj.front();
            ret = 1;
        }else
            ret = _copy_to_ptr( _my_sec_impl_obj, sec, destSz, end, beg);        
        /*
         * check ret vs. the return value of _my_base_type::copy for consistency ??
        */
        return ret;
    }
    
    both_type both( int indx ) const                        
    {        
        int dummy = 0;   
     
        _my_lock_guard_type lock( *_mtx );  
      
        generic_type gen = operator[](indx); /* _mark_count reset by _my_base_type */
        if( !indx )
            return both_type( gen, _my_sec_impl_obj.front() );

        _check_adj(indx, dummy, _my_sec_impl_obj );            
        return both_type( gen, _my_sec_impl_obj.at(indx) );
    }

    void secondary( secondary_type* dest, int indx ) const
    {
        int dummy = 0;

        _my_lock_guard_type lock( *_mtx );

        _check_adj(indx, dummy, _my_sec_impl_obj );
        if( !indx )
            *dest = _my_sec_impl_obj.front();
        else
            *dest = _my_sec_impl_obj.at( indx );    

        *_mark_count = indx - 1; /* _mark_count NOT reset by _my_base_type */
        *_mark_is_dirty = false;
    }

    secondary_vector_type
    secondary_vector( int end = -1, int beg = 0 ) const
    {
        _my_sec_impl_type::const_iterator bIter, eIter;
        _my_sec_impl_type::const_iterator::difference_type iterDiff;
        secondary_vector_type tmp; 
       
        _yld_to_push();        
        _my_lock_guard_type lock( *_mtx );
        _check_adj(end, beg, _my_sec_impl_obj);  
              
        bIter = _my_sec_impl_obj.cbegin() + beg;
        eIter = _my_sec_impl_obj.cbegin() + std::min< size_t >(++end, _q_count);    
        iterDiff = eIter - bIter;

        if( iterDiff > 0 ){ 
        /* do this manually; insert iterators too slow */
            tmp.resize(iterDiff); 
            std::copy( bIter, eIter, tmp.begin() );
        }

        *_mark_count = beg - 1; /* _mark_count NOT reset by _my_base_type */
        *_mark_is_dirty = false;

        return tmp;    
    }
};    
};

template< typename T, typename T2 >
std::ostream& 
operator<<(std::ostream&, const tosdb_data_stream::Interface<T,T2>&);

#endif
