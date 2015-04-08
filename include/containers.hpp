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

#ifndef JO_TOSDB_CONTAINERS
#define JO_TOSDB_CONTAINERS

#include <unordered_map>
#include <set>
#include <mutex>
#include <string>

template< typename T, typename Eq = std::less<T> >
class ILSet : public std::set<T, Eq> {    
 
public: /* no virtual destructor; DO NOT cast away from ILSet */

    typedef ILSet<T,Eq>     _myTy;
    typedef std::set<T, Eq> _myBaseTy;  
  
    ILSet()
        {
        }

    virtual ~ILSet()
        {
        }

    ILSet( const T& item )
        {
            this->insert(item); 
        }

    ILSet( T&& item )
        {
            this->insert( std::move(item) ); 
        }

    /*copy from same/base */
    ILSet( const _myBaseTy& set )
        : _myBaseTy(set)
        {            
        }

    /*move-construct from same 
    (explicit derived type to avoid partial destruction) */
    ILSet( _myTy&& set )
        : _myBaseTy( std::move(set) )
        {            
        }

    /*move-construct from same base */    
    ILSet( _myBaseTy&& set)
        : _myBaseTy( std::move(set) )
        {            
        }

    /*attempt to construct from like w/ func obj */
    template< typename T2, typename T3, typename Func >
    ILSet( const ILSet<T2,T3>& set, Func func )
        {
            for( auto & i : set ) 
                this->insert( func(i) ); 
        }

    /*attempt to construct from like base w/ func obj*/
    template< typename T2, typename T3, typename Func >
    ILSet( const std::set<T2,T3>& set, Func func )
        {
            for( auto & i : set ) 
                this->insert( func(i) ); 
        }

    /*attempt to construct from raw-array of like w/ func obj*/
    template< typename T2, typename Func >
    ILSet( const T2* ptr, size_t sz, Func func )
        {
            while(sz--) 
                this->insert( func(ptr[sz]) ); 
        }

    /*attempt to construct from array of like w/ func obj*/
    template< typename T2, size_t sz, typename Func >
    ILSet( const T2(&arr)[sz], Func func )
        {
            for(int i = 0; i < sz; ++i)
                this->insert( func(arr[i]) ); 
        }

    /*attempt to construct from raw-array of like*/
    template< typename T2 >
    ILSet( const T2* ptr, size_t sz )
        {
            while(sz--) 
                this->insert( ptr[sz] ); 
        }

    /*attempt to construct from array of like*/
    template< typename T2, size_t sz >
    ILSet( const T2(&arr)[sz] )
        {
            for(int i = 0; i < sz; ++i)
                this->insert( arr[i] ); 
        }

    /*attempt to construct from like*/
    template< typename T2, typename T3 >
    ILSet( const ILSet<T2, T3>& set)
        {    
            for(auto & item : set)
                this->insert( item);     
        }

    /*attempt to construct from like base*/
    template< typename T2, typename T3 >
    ILSet( const std::set<T2, T3>& set )
        {    
            for(auto & item : set)
                this->insert( item);     
        }

    /*move-assign from same 
    (explicit derived type to avoid partial destruction) */
    _myTy& operator=( _myTy&& set )
    {
        _myBaseTy::operator=( std::move( set ) );
        return *this;
    }

    /*move-assign from same/base */
    _myTy& operator=( _myBaseTy&& set )
    {
        _myBaseTy::operator=( std::move( set ) );
        return *this;
    }

    /*assign from same/base*/
    _myTy& operator=( const _myBaseTy& set )
    {
        if( (_myBaseTy)*this == set )
            return *this;
        _myBaseTy::operator=(set);
        return *this;
    }

    /*attempt to assign from like base*/
    template< typename T2, typename T3>
    _myTy& operator=( const std::set<T2,T3>& set )
    {
        this->clear();
        for(auto & item : set)
            this->insert( item);
        return *this;
    }

    /* attemp to assign from like */
    template< typename T2, typename T3>
    _myTy& operator=( const ILSet<T2,T3>& set )
    {
        this->clear();
        for(auto & item : set)
            this->insert( item);     
        return *this;
    }

    /*attempt to assign from array of like*/
    template< typename T2, size_t sz >
    _myTy& operator=( const T2(&arr)[sz] )
    {
        this->clear();
        for(int i = 0; i < sz; ++i)
            this->insert( arr[i] ); 
        return *this;
    }
};

struct str_eq {
    bool inline operator()(const char * left, const char * right) const 
    {
        return ( strcmp(left, right) == 0 );    
    }
};

struct str_less {
    bool inline operator()(const char *left, const char *right) const 
    {
        return ( strcmp(left, right) == -1 );    
    }
};

struct str_hash {
    size_t inline operator()(const char * str) const 
    {
        return std::hash<std::string>()(str);
    }
};

template< typename T >
class SmartBuffer : 
    private std::unique_ptr<T, void(*)(T*)> {

    typedef std::unique_ptr<T, void(*)(T*)> _myBaseTy;    
    typedef SmartBuffer< T > _myTy;
    size_t _bytes;

public:

    SmartBuffer()
        :
        _myBaseTy( nullptr, [](T* p){} )
        {
        }

    SmartBuffer( size_t bytes)
        :
        _bytes(bytes),
        _myBaseTy( (T*)new char[bytes], [](T* p){ delete[] (char*)p; } )
        { 
            memset( get(), 0, bytes );
        }

    virtual ~SmartBuffer()
        {
        }

    using _myBaseTy::get;

    inline size_t bytes() 
    { 
        return _bytes; 
    }

    _myTy& operator=(const SmartBuffer& right)
    {        
        if( *this != right ){
            _bytes = right._bytes;

            _myBaseTy::reset( (T*)new char[_bytes], 
                              [](T* p){ delete[] (char*)p; } );

            memcpy( get(), right.get(), _bytes);
        }
        return *this;
    }

    SmartBuffer(const SmartBuffer& right)
        :
        _bytes( right._bytes ),
        _myBaseTy( (T*)new char[_bytes], [](T* p){ delete[] (char*)p; } )
        {
            memcpy( get(), right.get(), _bytes);
        }

    _myTy& operator=(SmartBuffer&& right)
    {
        if( *this != right ){
            _bytes = right._bytes;
            _myBaseTy::operator=(std::move(right));        
        }
        return *this; 
    }

    SmartBuffer(SmartBuffer&& right)
        :
        _bytes( right._bytes ),
        _myBaseTy(std::move(right))
        {
        }
};

template< typename T1, 
          typename T2,    
          bool ThreadSafe = false,
          typename Hash1 = std::hash<T1>, 
          typename Hash2 = std::hash<T2>,
          typename Key1Eq = std::equal_to<T1>, 
          typename Key2Eq = std::equal_to<T2> >
class TwoWayHashMap {    

    static_assert( !std::is_same<T1,T2>::value, 
        "TwoWayHashMap requires different types." );    

public:

    typedef T1                                          key_type;
    typedef T2                                          value_type;
    typedef std::unordered_map< T1, T2, Hash1, Key1Eq>  map1_type; 
    typedef std::unordered_map< T2, T1, Hash2, Key2Eq>  map2_type; 
    typedef map1_type::value_type              pair1_type; 
    typedef typename map2_type::value_type              pair2_type;
    typedef typename map1_type::iterator                iterator1_type;
    typedef typename map2_type::iterator                iterator2_type;
    typedef typename map1_type::const_iterator          const_iterator1_type;
    typedef typename map2_type::const_iterator          const_iterator2_type;

    TwoWayHashMap()
        {
        }

    template< size_t sz >
    TwoWayHashMap( const pair1_type(&arr)[sz] )        
        {        
            for( int i = 0; i < sz; ++i) 
                this->_insert( arr[i] );         
        }

    TwoWayHashMap( const map1_type& map )
        {
            for( auto & m : map )
                this->_insert( m );
        }

    TwoWayHashMap( const std::map<T1,T2>& map )
        {
            for( auto & m : map )
                this->_insert( m );
        }

    void insert(T1 val1, T2 val2)
    { 
         this->_insert( pair1_type(val1, val2) );
    }

    template< size_t sz >
    void insert( const pair1_type(&arr)[sz]) 
    {
        for( int i = 0; i <sz; ++i)
            this->_insert( arr[i] ); 
    }

    void insert( const pair1_type keyVal) 
    {
        this->_insert( keyVal ); 
    }

    void remove(T1 key)
    {
        T2 tmp = this->operator[](key);
        this->_mapByT1.erase(key); 
        this->_mapByT2.erase(tmp); 
    }

    void remove(T2 key)
    {        
        T1 tmp = this->operator[](key);
        this->_mapByT2.erase(key); 
        this->_mapByT1.erase(tmp); 
    }

    T2 operator[](const T1 key) const
    {
        try{ 
            return this->_mapByT1.at(key); 

        }catch( const std::out_of_range ){ 
            return (T2)NULL; 
        }
    }

    T1 operator[](const T2 key) const 
    {
        try{            
            return this->_mapByT2.at(key);            
        
        }catch( const std::out_of_range ){            
            return (T1)NULL;
        }
    }

    const_iterator1_type find( const T1& key ) const
    {
        return _mapByT1.find( key );
    }

    const_iterator2_type find( const T2& key ) const
    {
        return _mapByT2.find( key );
    }

    iterator1_type find( const T1& key ) 
    {
        return _mapByT1.find( key );
    }

    iterator2_type find( const T2& key ) 
    {
        return _mapByT2.find( key );
    }

    iterator1_type begin()
    {
        return _mapByT1.begin(); 
    }

    iterator1_type end() 
    {
        return _mapByT1.end();
    }

    const_iterator1_type cbegin() const
    {
        return _mapByT1.cbegin();
    }

    const_iterator1_type cend() const
    {
        return _mapByT1.cend();
    }

    size_t size() const
    {
        size_t s1 = _mapByT1.size();
        if( s1 != _mapByT2.size() )
            throw std::out_of_range("hash map sizes are not equal");

        return s1;
    }

    bool empty() const 
    {        
        return ( this->_mapByT1.empty() && this->_mapByT2.empty() ); 
    }

    bool thread_safe() const
    {
        return ThreadSafe;
    }

private:

     map1_type _mapByT1;
     map2_type _mapByT2;

    void _insert( pair1_type keyVal )
    {    
        this->_mapByT1.erase( keyVal.first );
        this->_mapByT2.erase( keyVal.second );
        this->_mapByT1.insert( keyVal ); 
        this->_mapByT2.insert( pair2_type( keyVal.second, keyVal.first ) ); 
    }
};

/* thread_safe specialization */
template< typename T1, typename T2, 
          typename Hash1, typename Hash2, 
          typename Key1Eq, typename Key2Eq >
class TwoWayHashMap< T1, T2, true, Hash1, Hash2, Key1Eq, Key2Eq >  
    : public TwoWayHashMap< T1, T2, false, Hash1, Hash2, Key1Eq, Key2Eq > {

    typedef TwoWayHashMap< T1,T2,false,Hash1,Hash2,Key1Eq,Key2Eq >  _myBaseTy;
    typedef std::lock_guard< std::recursive_mutex >                 _guardTy;

    std::recursive_mutex _mtx;

public:

    TwoWayHashMap()
        : _myBaseTy()
        {
        }

    template< size_t sz >
    TwoWayHashMap( const typename _myBaseTy::pair1_type(&arr)[sz] )
        : _myBaseTy( arr )
        {                
        }

    TwoWayHashMap( const typename _myBaseTy::map1_type& map )
        : _myBaseTy( map )
        {        
        }

    TwoWayHashMap( const std::map<T1,T2>& map )
        : _myBaseTy( map )
        {        
        }

    void insert(T1 keyVal1, T2 keyVal2)
    { 
        _guardTy _lock_(this->_mtx);
         _myBaseTy::insert(keyVal1, keyVal2);
    }

    void insert( const pair1_type keyVal) 
    {
        _guardTy _lock_(this->_mtx);
         _myBaseTy::insert(keyVal);
    }

    template< size_t sz >
    void insert( const typename _myBaseTy::pair1_type(&arr)[sz]) 
    {
        _guardTy _lock_(this->_mtx);
        _myBaseTy::insert( arr );
    }

    void remove(T1 key)
    {
        _guardTy _lock_(this->_mtx);
        _myBaseTy::remove( key );
    }

    void remove(T2 key)
    {        
        _guardTy _lock_(this->_mtx);
        _myBaseTy::remove( key );
    }

    T2 operator[](const T1 key) 
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::operator[]( key );
    }

    T1 operator[](const T2 key)  
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::operator[]( key );
    }

    const_iterator1_type find( const T1& key ) const
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::find( key );
    }

    const_iterator2_type find( const T2& key ) const
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::find( key )
    }

    iterator1_type find( const T1& key ) 
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::find( key );
    }

    iterator2_type find( const T2& key ) 
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::find( key )
    }

    iterator1_type begin()
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::begin();
    }

    iterator1_type end()
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::end();
    }

    const_iterator1_type cbegin() const
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::cbegin();
    }

    const_iterator1_type cend() const
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::cend();
    }

    size_t size() const
    {
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::size();
    }

    bool empty() 
    {        
        _guardTy _lock_(this->_mtx);
        return _myBaseTy::empty();
    }

};


#endif
