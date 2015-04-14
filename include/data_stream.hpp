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


    
/*    The interface to tosdb_data_stream     */
template< typename SecTy, typename GenTy >
class Interface {
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

        auto dstr = [sz]( char** _pptr){ DeallocStrArray( _pptr, sz); };

        std::unique_ptr<char*,decltype(dstr)> strMat( AllocStrArray(sz,STR_DATA_SZ), 
                                                      dstr );
        ret = this->copy( strMat.get(), sz, STR_DATA_SZ , end, beg, sec);                
        std::copy_n( strMat.get(), sz, dest );            
        return ret;
    }

    /**************************/

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
    _throw_type_error< _inTy* >( "->copy()", _dropBool ); \
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
        _throw_type_error< std::string* >( "->copy()", false );  
        return 0;
    }

    virtual long long copy_from_marker( std::string* dest, 
                                        size_t sz,                                         
                                        int beg = 0, 
                                        secondary_type* sec = nullptr) const
    {
        long long ret;
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        auto dstr = [sz]( char** _pptr){ DeallocStrArray( _pptr, sz); };

        std::unique_ptr<char*,decltype(dstr)> strMat( AllocStrArray(sz,STR_DATA_SZ), 
                                                      dstr );
        ret = this->copy_from_marker( strMat.get(), sz, STR_DATA_SZ , 
                                              beg, sec );                
        std::copy_n( strMat.get(), sz, dest );   
        return ret;
    }

    /******************/
    
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
    
    typedef Object< Ty, SecTy, GenTy, UseSecondary, Allocator>   _myTy;
    typedef Interface< SecTy, GenTy >                            _myBase;
    typedef std::deque< Ty, Allocator >                          _myImplTy;            
    typedef typename _myImplTy::const_iterator::difference_type  _myImplDiffTy;    

    _myTy& operator=(const _myTy &);
    
    void _push(const Ty _item) 
    {   /* if can't obtain lock indicate other threads should yield to us */       
        _push_has_priority = _mtx->try_lock(); /*O.K. push/pop doesn't throw*/
        if( !_push_has_priority )
            _mtx->lock(); /* block regardless */  

        _myImplObj.push_front(_item); 
        _myImplObj.pop_back();       
        _incr_intrnl_counts();

        _mtx->unlock();
    } 

protected:

    typedef std::lock_guard<std::recursive_mutex >  _guardTy;
    
    std::recursive_mutex* const  _mtx; 
    volatile bool                _push_has_priority;
    bool* const                  _mrkOverset;    
    size_t                       _qCount, _qBound;
    long long* const             _mrkCount;    
    _myImplTy                    _myImplObj;

    void _yld_to_push() const
    {    
        if( _push_has_priority )
            return;
        std::this_thread::yield();
    }

    template< typename T > 
    bool _check_adj( int& end, 
                     int& beg, 
                     const std::deque< T, Allocator >& impl ) const
    {  /* O.K. sz can't be > INT_MAX  */
        int sz = (int)impl.size();

        if ( _qBound != sz )    
            throw size_violation( 
                "Internal size/bounds violation in tosdb_data_stream", _qBound, sz );          
  
        if ( end < 0 ) end += sz; 
        if ( beg < 0 ) beg += sz;
        if ( beg >= sz || end >= sz || beg < 0 || end < 0 )    
            throw out_of_range( "adj index value out of range in tosdb_data_stream",
                                sz, beg, end );
        else if ( beg > end )     
            throw invalid_argument( 
                "adj beg index value > end index value in tosdb_data_stream" );

        return true;
    }

    void _incr_intrnl_counts()
    {
        long long penult = (long long)_qBound -1; 

        if( _qCount < _qBound )
            ++_qCount;
                
        if( *_mrkCount == penult )
            *_mrkOverset = true;
        else if( *_mrkCount < penult )
            ++(*_mrkCount);
        else           
            /* 
             *WE CANT THROW SO ATTEMP TO GET _mrkCount back in line
            */
            --(*_mrkCount);
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
            std::min< size_t >( sz + beg, std::min< size_t >( ++end, _qCount ));

        if( bIter < eIter )
            return std::copy( bIter, eIter, dest ) - dest;  
        else
            return 0;
    }

public:

    typedef _myBase  interface_type;
    typedef Ty       value_type;

    Object(size_t sz )
        : 
        _myImplObj( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1) ),
        _qBound( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1) ),
        _qCount( 0 ),
        _mrkCount( new long long(-1) ),
        _mrkOverset( new bool(false) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {            
        }

    Object(const _myTy & stream )
        : 
        _myImplObj( stream._myImplObj ),
        _qBound( stream._qBound ),
        _qCount( stream._qCount ),
        _mrkCount( new long long(*stream._mrkCount) ),
        _mrkOverset( new bool(*stream._mrkOverset) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {            
        }

    Object( _myTy && stream)
        : 
        _myImplObj( std::move( stream._myImplObj) ),
        _qBound( stream._qBound ),
        _qCount( stream._qCount ),   
        _mrkCount( new long long(*stream._mrkCount) ),
        _mrkOverset( new bool(*stream._mrkOverset) ),
        _mtx( new std::recursive_mutex ),
        _push_has_priority( true )
        {                
        }

    virtual ~Object()
        {
            delete this->_mtx;  
            delete this->_mrkCount;
            delete this->_mrkOverset;
        }

    bool       inline empty()           const { return _myImplObj.empty(); }
    size_t     inline size()            const { return _qCount; }
    bool       inline uses_secondary()  const { return false; }
    long long  inline marker_position() const { return *_mrkCount; }
    size_t     inline bound_size()      const { return _qBound; }
   
    size_t bound_size( size_t sz )
    {
        sz = std::max<size_t>( std::min<size_t>(sz,MAX_BOUND_SIZE),1);

        _guardTy _lock_( *_mtx );
        _myImplObj.resize(sz);

        if( sz < _qBound ){
            /* IF bound is 'clipped' from the left(end) */
            _myImplObj.shrink_to_fit();    
            if( (long long)sz <= *_mrkCount ){
                /* IF marker is 'clipped' from the left(end) */
                *_mrkCount = (long long)sz -1;
                *_mrkOverset = true;
            }
        }        
        _qBound = sz;
        if( sz < _qCount )
            /* IF count is 'clipped' from the left(end) */
            _qCount = sz;

        return _qBound;
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
        * 2) we can't access 'beg' before copy calls _check_adj()
        * 3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX
        */   
        long long copy_sz;
        long long old_count = *_mrkCount;
        bool was_overset = *_mrkOverset;

        if( old_count < 0 ) /* IF mark hasn't moved */
            return 0;         
                   
        copy_sz = (long long)copy( dest, sz, old_count, beg, sec);

        if( was_overset || copy_sz < (old_count - (long long)beg) )
           /*
            * IF mark is overset (i.e hits back of stream) or we
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
         * 2) we can't access beg before copy calls _check_adj()
         * 3) casts to long long O.K as long as MAX_BOUND_SIZE == INT_MAX
         */
        long long copy_sz;
        long long old_count = *_mrkCount;
        bool was_overset = *_mrkOverset;

        if( old_count < 0 ) /* IF mark hasn't moved */
            return 0;         
                            
        copy_sz = (long long)copy( dest, destSz, strSz, old_count, beg, sec);

        if( was_overset || copy_sz < (old_count - (long long)beg) )
           /*
            * IF mark is overset (i.e hits back of stream) or we
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
        _guardTy _lock_( *_mtx );
        _check_adj( end, beg, _myImplObj );                     

        if( end == beg ){
            *dest = beg ? _myImplObj.at(beg) : _myImplObj.front();
            ret = 1;
        }else 
            ret = _copy_to_ptr(_myImplObj,dest,sz,end,beg);         
        
        *_mrkCount = beg - 1;   
        *_mrkOverset = false;

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
        _myImplTy::const_iterator bIter, eIter;
        size_t i;
 
        if( !dest )
            throw invalid_argument( "->copy(): *dest argument can not be null");

        _yld_to_push();        
        _guardTy _lock_( *_mtx );                    
        _check_adj( end, beg, _myImplObj);                        

        bIter = _myImplObj.cbegin() + beg; 
        eIter = _myImplObj.cbegin() + std::min< size_t >(++end, _qCount);

        for( i = 0; (i < destSz) && (bIter < eIter); ++bIter, ++i )
        {             
            std::string genS = generic_type( *bIter ).as_string();                
            strncpy_s( dest[i], strSz, genS.c_str(), 
                       std::min<size_t>( strSz-1, genS.length() ) );                                  
        }    

        *_mrkCount = beg - 1; 
        *_mrkOverset = false;

        return i;
    }

    generic_type operator[]( int indx) const
    {
        int dummy = 0;

        _guardTy _lock_( *_mtx );
        if ( !indx ){ /* optimize for indx == 0 */
            *_mrkCount = -1;
            *_mrkOverset = false;
            return generic_type( _myImplObj.front() ); 
        }

        _check_adj( indx, dummy, _myImplObj ); 

        *_mrkCount = indx - 1; 
        *_mrkOverset = false;

        return generic_type( _myImplObj.at(indx) );         
    }

    both_type both( int indx ) const                        
    {    
        _guardTy _lock_( *_mtx );
        return both_type( operator[](indx), secondary_type() );
    }

    generic_vector_type 
    vector(int end = -1, int beg = 0) const 
    {
        _myImplTy::const_iterator bIter, eIter;        
        generic_vector_type tmp;  
      
        _yld_to_push();        
        _guardTy _lock_( *_mtx );
        _check_adj(end, beg, _myImplObj);
                
        bIter = _myImplObj.cbegin() + beg;
        eIter = _myImplObj.cbegin() + std::min< size_t >(++end, _qCount);  
      
        if( bIter < eIter )                   
            std::transform( bIter, eIter, 
                /* have to use slower insert_iterator approach, 
                   generic_type doesn't allow default construction */
                std::insert_iterator< generic_vector_type >( tmp, tmp.begin() ), 
                []( Ty x ){ return generic_type(x); } );   

        *_mrkCount = beg - 1; 
        *_mrkOverset = false;
         
        return tmp;    
    }

    secondary_vector_type
    secondary_vector( int end = -1, int beg = 0 ) const
    {                
        _check_adj(end, beg, _myImplObj);                                            
        return secondary_vector_type( std::min< size_t >(++end - beg, _qCount));
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
    typedef Object< Ty, SecTy, GenTy, true, Allocator>   _myTy;
    typedef Object< Ty, SecTy, GenTy, false, Allocator>  _myBase;
    typedef std::deque< SecTy, Allocator >               _myImplSecTy;
    
    _myImplSecTy _myImplSecObj;    
    
    void _push(const Ty _item, const secondary_type&& sec) 
    {    
        _push_has_priority = _mtx->try_lock();
        if( !_push_has_priority )
            _mtx->lock();

        _myBase::_myImplObj.push_front(_item); 
        _myBase::_myImplObj.pop_back();
        _myImplSecObj.push_front( std::move(sec) );
        _myImplSecObj.pop_back();
        _incr_intrnl_counts();

        _mtx->unlock();
    } 

public:

    typedef Ty    value_type;

    Object(size_t sz )
        : 
        _myImplSecObj(std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1)),
        _myBase( std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1))
        {
        }

    Object(const _myTy & stream)
        : 
        _myImplSecObj( stream._myImplSecObj ),
        _myBase( stream )
        { 
        }

    Object(_myTy && stream)
        : 
        _myImplSecObj( std::move( stream._myImplSecObj) ),
        _myBase( std::move(stream) )
        {
        }

    size_t bound_size(size_t sz)
    {
        sz = std::max<size_t>(std::min<size_t>(sz,MAX_BOUND_SIZE),1);

        _guardTy _lock_( *_mtx );   
     
        _myImplSecObj.resize(sz);
        if (sz < _qCount)
            _myImplSecObj.shrink_to_fit();  
  
        return _myBase::bound_size(sz);        
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

        _guardTy _lock_( *_mtx ); 

        ret = _myBase::copy(dest, sz, end, beg); /*_mrkCount reset by _myBase*/
          
        if( !sec )
            return ret;
                
        _check_adj( end, beg, _myImplSecObj ); /*repeat to update index vals */ 
 
        if( end == beg ){    
            *sec = beg ? _myImplSecObj.at(beg) : _myImplSecObj.front();
            ret = 1;
        }else    
            ret = _copy_to_ptr( _myImplSecObj, sec, sz, end, beg);    
        /*
         * check ret vs. the return value of _myBase::copy for consistency ??
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

        _guardTy _lock_( *_mtx );

        ret = _myBase::copy(dest, destSz, strSz, end, beg);

        if( !sec )
            return ret;
        
        _check_adj( end, beg, _myImplSecObj ); /*repeat to update index vals*/ 

        if( end == beg ){
            *sec = beg ? _myImplSecObj.at(beg) : _myImplSecObj.front();
            ret = 1;
        }else
            ret = _copy_to_ptr( _myImplSecObj, sec, destSz, end, beg);        
        /*
         * check ret vs. the return value of _myBase::copy for consistency ??
        */
        return ret;
    }
    
    both_type both( int indx ) const                        
    {        
        int dummy = 0;   
     
        _guardTy _lock_( *_mtx );  
      
        generic_type gen = operator[](indx); /* _mrkCount reset by _myBase */
        if( !indx )
            return both_type( gen, _myImplSecObj.front() );

        _check_adj(indx, dummy, _myImplSecObj );            
        return both_type( gen, _myImplSecObj.at(indx) );
    }

    void secondary( secondary_type* dest, int indx ) const
    {
        int dummy = 0;

        _guardTy _lock_( *_mtx );

        _check_adj(indx, dummy, _myImplSecObj );
        if( !indx )
            *dest = _myImplSecObj.front();
        else
            *dest = _myImplSecObj.at( indx );    

        *_mrkCount = indx - 1; /* _mrkCount NOT reset by _myBase */
        *_mrkOverset = false;
    }

    secondary_vector_type
    secondary_vector( int end = -1, int beg = 0 ) const
    {
        _myImplSecTy::const_iterator bIter, eIter;
        _myImplSecTy::const_iterator::difference_type iterDiff;
        secondary_vector_type tmp; 
       
        _yld_to_push();        
        _guardTy _lock_( *_mtx );
        _check_adj(end, beg, _myImplSecObj);  
              
        bIter = _myImplSecObj.cbegin() + beg;
        eIter = _myImplSecObj.cbegin() + std::min< size_t >(++end, _qCount);    
        iterDiff = eIter - bIter;

        if( iterDiff > 0 ){ 
        /* do this manually; insert iterators too slow */
            tmp.resize(iterDiff); 
            std::copy( bIter, eIter, tmp.begin() );
        }

        *_mrkCount = beg - 1; /* _mrkCount NOT reset by _myBase */
        *_mrkOverset = false;

        return tmp;    
    }
};    
};

template< typename T, typename T2 >
std::ostream& 
operator<<(std::ostream&, const tosdb_data_stream::Interface<T,T2>&);

#endif
