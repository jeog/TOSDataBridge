/* 
Copyright (C) 2014 Jonathon Ogden   <jeog.dev@gmail.com>

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

template<typename T, typename Eq = std::less<T>>
class ILSet 
        : public std::set<T, Eq> {  
    typedef ILSet<T,Eq> _my_ty;
    typedef std::set<T,Eq> _my_base_ty;  

public:   
    ILSet() 
        {
        }

    virtual 
    ~ILSet() 
        {
        }

    ILSet(const T& item) 
        {
            insert(item); 
        }

    ILSet(T&& item)
        {
            insert(std::move(item)); // do we need the move ?
        }

    /* copy from same/base */
    ILSet(const _my_base_ty& set)
        : 
            _my_base_ty(set)
        {      
        }

    // 2020-09-06 make copy construct explicit
    ILSet( const _my_ty& set )
        :
            _my_base_ty( (const _my_base_ty&)set )
        {
        }

    /* move-construct from same 
      (explicit derived type to avoid partial destruction) */
    ILSet(_my_ty&& set)
        : 
            _my_base_ty(std::move(set))
        {      
        }

    /*move-construct from same base */  
    ILSet(_my_base_ty&& set)
        : 
            _my_base_ty(std::move(set))
        {      
        }

    /*attempt to construct from like w/ func obj */
    template<typename T2, typename T3, typename Func>
    ILSet(const ILSet<T2,T3>& set, Func func)
        {
            for(auto & i : set) 
                insert(func(i)); 
        }

    /*attempt to construct from like base w/ func obj*/
    template<typename T2, typename T3, typename Func>
    ILSet(const std::set<T2,T3>& set, Func func)
        {
            for(auto & i : set) 
                insert(func(i)); 
        }

    /*attempt to construct from raw-array of like w/ func obj*/
    template<typename T2, typename Func>
    ILSet(const T2* ptr, size_t sz, Func func)
        {
            while(sz--) 
                insert(func(ptr[sz])); 
        }

    /*attempt to construct from array of like w/ func obj*/
    template<typename T2, size_t sz, typename Func>
    ILSet(const T2(&arr)[sz], Func func)
        {
            for(size_t i = 0; i <sz; ++i)
                insert(func(arr[i])); 
        }

    /*attempt to construct from raw-array of like*/
    template<typename T2>
    ILSet(const T2* ptr, size_t sz)
        {
            while(sz--) 
                insert(ptr[sz]); 
        }

    /*attempt to construct from array of like*/
    template<typename T2, size_t sz>
    ILSet(const T2(&arr)[sz])
        {
            for(size_t i = 0; i <sz; ++i)
                insert(arr[i]); 
        }

    /*attempt to construct from like*/
    template<typename T2, typename T3>
    ILSet(const ILSet<T2, T3>& set)
        {  
            for(auto & item : set)
                insert(item);   
        }

    /*attempt to construct from like base*/
    template<typename T2, typename T3>
    ILSet(const std::set<T2, T3>& set)
        {  
            for(auto & item : set)
                insert(item);   
        }

    /*move-assign from same 
     (explicit derived type to avoid partial destruction) */
    _my_ty& 
    operator=(_my_ty&& set)
    {
        _my_base_ty::operator=(std::move(set));
        return *this;
    }

    /*move-assign from same/base */
    _my_ty& 
    operator=(_my_base_ty&& set)
    {
        _my_base_ty::operator=(std::move(set));
        return *this;
    }

    /*assign from same/base*/
    _my_ty& 
    operator=(const _my_base_ty& set)
    {
        if((_my_base_ty)(*this) == set)
            return *this;
        _my_base_ty::operator=(set);
        return *this;
    }

    // 2020-09-06 make assignment explicit
    _my_ty& 
    operator=(const _my_ty& set)
    {
        return operator=((const _my_base_ty&)set);
    }

    /*attempt to assign from like base*/
    template<typename T2, typename T3>
    _my_ty& 
    operator=(const std::set<T2,T3>& set)
    {
        clear();
        for(auto & item : set)
            insert(item);
        return *this;
    }

    /* attemp to assign from like */
    template<typename T2, typename T3>
    _my_ty& 
    operator=(const ILSet<T2,T3>& set)
    {
        clear();
        for(auto & item : set)
            insert(item);     
        return *this;
    }

    /*attempt to assign from array of like*/
    template<typename T2, size_t sz>
    _my_ty& 
    operator=(const T2(&arr)[sz])
    {
        clear();
        for(size_t i = 0; i <sz; ++i)
            insert(arr[i]); 
        return *this;
    }
};

struct str_eq {
    inline bool 
    operator()(const char * left, const char * right) const 
    {
        return (strcmp(left, right) == 0);    
    }
};

struct str_less {
    inline bool 
    operator()(const char *left, const char *right) const 
    {
        return (strcmp(left, right) == -1);    
    }
};

struct str_hash {
    inline size_t 
    operator()(const char * str) const 
    {
        return std::hash<std::string>()(str);
    }
};

template<typename T>
class SmartBuffer 
        : private std::unique_ptr<T, void(*)(T*)> {
    typedef SmartBuffer<T> _my_ty;
    typedef std::unique_ptr<T, void(*)(T*)> _my_base_ty;        
    size_t _bytes;

public:
    SmartBuffer()
        : 
            _my_base_ty(nullptr, [](T* p){})
        {
        }

    SmartBuffer(size_t bytes)
        :
            _bytes(bytes),
            _my_base_ty((T*)new char[bytes], [](T* p){ delete[] (char*)p; })
        { 
            memset(get(), 0, bytes);
        }

    virtual ~SmartBuffer() 
        {
        }

    using _my_base_ty::get;

    inline size_t 
    bytes()
    { 
        return _bytes; 
    }

    _my_ty& 
    operator=(const SmartBuffer& right)
    {        
        if(*this != right){
            _bytes = right._bytes;            
            _my_base_ty::reset((T*)new char[_bytes]); //, [](T* p){ delete[] (char*)p;});
            memcpy(get(), right.get(), _bytes);
        }
        return *this;
    }

    SmartBuffer(const SmartBuffer& right)
        :
            _bytes(right._bytes),
            _my_base_ty((T*)new char[_bytes], [](T* p){ delete[] (char*)p; })
        {
            memcpy(get(), right.get(), _bytes);
        }

    _my_ty& 
    operator=(SmartBuffer&& right)
    {
        if(*this != right){
            _bytes = right._bytes;
            _my_base_ty::operator=(std::move(right));        
        }
        return *this; 
    }

    SmartBuffer(SmartBuffer&& right)
        :
            _bytes(right._bytes),
            _my_base_ty(std::move(right))
        {
        }
};

template<typename T1, 
         typename T2,    
         bool IsThreadSafe = false,
         typename Hash1 = std::hash<T1>, 
         typename Hash2 = std::hash<T2>,
         typename Key1Eq = std::equal_to<T1>, 
         typename Key2Eq = std::equal_to<T2>>
class TwoWayHashMap {    
    static_assert(!std::is_same<T1,T2>::value, 
                  "TwoWayHashMap requires different types");    
public:
    typedef T1 key_type;
    typedef T2 mapped_type;
    
    typedef std::unordered_map<T1, T2, Hash1, Key1Eq> map1_type; 
    typedef std::unordered_map<T2, T1, Hash2, Key2Eq> map2_type; 

    /* Dec 8 2016, for initializer_chain compatability */
    typedef typename map1_type::value_type value_type;

    typedef typename map1_type::value_type pair1_type; 
    typedef typename map2_type::value_type pair2_type;
        
    typedef typename map1_type::iterator iterator1_type;
    typedef typename map2_type::iterator iterator2_type;

    typedef typename map1_type::const_iterator const_iterator1_type;
    typedef typename map2_type::const_iterator const_iterator2_type;

    TwoWayHashMap() 
        {
        }

    virtual ~TwoWayHashMap() 
        {
        }

    template<size_t sz>
    TwoWayHashMap(const pair1_type(&arr)[sz])        
        {        
            for(size_t i = 0; i <sz; ++i) 
                _insert(arr[i]);         
        }

    TwoWayHashMap(const map1_type& map)
        {
            for(auto & m : map)
                _insert(m);
        }

    TwoWayHashMap(const std::map<T1,T2>& map)
        {
            for(auto & m : map)
                _insert(m);
        }

    inline void 
    insert(T1 k, T2 v)
    { 
         _insert(pair1_type(k, v));
    }

    template<size_t sz>
    void 
    insert(const pair1_type(&arr)[sz])
    {
        for(size_t i = 0; i <sz; ++i)
            _insert(arr[i]); 
    }

    inline void 
    insert(const pair1_type p) 
    {
        _insert(p); 
    }

    /* Dec 8 2016, for initializer_chain compatability */
    inline void 
    insert(iterator1_type dummy, const pair1_type p) 
    {
        _insert(p); 
    }

    /* Dec 21 2016 - convert to at() and catch exceptions */
    void 
    remove(T1 k)
    {        
        try{
            T2 tmp = _map1.at(k);
            _map1.erase(k); 
            _map2.erase(tmp); 
        }catch(...){
        }
    }

    void 
    remove(T2 k)
    {        
        try{
            T1 tmp = _map2.at(k);
            _map2.erase(k); 
            _map1.erase(tmp); 
        }catch(...){
        }
    }

    /* behavior of operator[] can be problem with NULL elems */
    T2 
    operator[](const T1 k) const
    {
        try{ 
            return _map1.at(k); 
        }catch(const std::out_of_range){ 
            return (T2)NULL; 
        }
    }

    /* behavior of operator[] can be problem with NULL elems */
    T1 
    operator[](const T2 k) const 
    {
        try{            
            return _map2.at(k);                 
        }catch(const std::out_of_range){            
            return (T1)NULL;
        }
    }

    inline const_iterator1_type 
    find(const T1& k) const 
    { 
        return _map1.find(k); 
    }

    inline const_iterator2_type 
    find(const T2& k) const 
    { 
        return _map2.find(k); 
    }

    inline iterator1_type             
    find(const T1& k) 
    { 
        return _map1.find(k); 
    }

    inline iterator2_type             
    find(const T2& k) 
    { 
        return _map2.find(k); 
    }

    inline iterator1_type             
    begin() 
    { 
        return _map1.begin(); 
    }

    inline iterator1_type             
    end() 
    { 
        return _map1.end(); 
    }

    inline const_iterator1_type 
    cbegin() const 
    { 
        return _map1.cbegin(); 
    }

    inline const_iterator1_type 
    cend() const 
    { 
        return _map1.cend(); 
    }

    size_t 
    size() const
    {
        size_t s = _map1.size();
        if(s != _map2.size())
            throw std::out_of_range("hash map sizes are not equal");
        return s;
    }

    inline bool 
    empty() const 
    { 
        return (_map1.empty() && _map2.empty()); 
    }

    inline bool 
    thread_safe() const 
    { 
        return IsThreadSafe; 
    }

private:
     map1_type _map1;
     map2_type _map2;

    void 
    _insert(pair1_type p)
    {    
        _map1.erase(p.first);
        _map2.erase(p.second);
        _map1.insert(p); 
        _map2.insert(pair2_type(p.second, p.first)); 
    }
};


template<typename T1, 
         typename T2, 
         typename Hash1, 
         typename Hash2, 
         typename Key1Eq, 
         typename Key2Eq>
class TwoWayHashMap<T1, T2, true, Hash1, Hash2, Key1Eq, Key2Eq>    
        : public TwoWayHashMap<T1, T2, false, Hash1, Hash2, Key1Eq, Key2Eq> {
    /* thread_safe specialization */
    typedef TwoWayHashMap<T1,T2,false,Hash1,Hash2,Key1Eq, Key2Eq> _my_base_ty;    
    typedef std::lock_guard<std::recursive_mutex>  _my_lock_guard_type;

    std::recursive_mutex _mtx;

public:
    TwoWayHashMap()
        : 
            _my_base_ty()
        {
        }

    template<size_t sz>
    TwoWayHashMap(const pair1_type(&arr)[sz])
        : 
            _my_base_ty(arr)
        {                
        }

    TwoWayHashMap(const map1_type& map)
        : 
            _my_base_ty(map)
        {        
        }

    TwoWayHashMap(const std::map<T1,T2>& map)
        : 
            _my_base_ty(map)
        {        
        }

    void 
    insert(T1 k, T2 v)
    { 
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::insert(k, v);
    }

    void 
    insert(const pair1_type p) 
    {
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::insert(p);
    }

    /* Dec 8 2016, for initializer_chain compatability */
    void 
    insert(iterator1_type dummy, const pair1_type p) 
    {
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::insert(dummy, p);
    }

    template<size_t sz>
    void 
    insert(const pair1_type(&arr)[sz]) 
    {
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::insert(arr);
    }

    void 
    remove(T1 k)
    {
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::remove(k);
    }

    void 
    remove(T2 k)
    {        
        _my_lock_guard_type lock(_mtx);
        _my_base_ty::remove(k);
    }

    T2 
    operator[](const T1 k) 
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::operator[](k);
    }

    T1 
    operator[](const T2 k)    
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::operator[](k);
    }

    const_iterator1_type 
    find(const T1& k) const
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::find(k);
    }

    const_iterator2_type 
    find(const T2& k) const
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::find(k);
    }

    iterator1_type 
    find(const T1& k) 
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::find(k);
    }

    iterator2_type 
    find(const T2& k) 
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::find(k);
    }

    iterator1_type 
    begin()
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::begin();
    }

    iterator1_type 
    end()
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::end();
    }

    const_iterator1_type 
    cbegin() const
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::cbegin();
    }

    const_iterator1_type 
    cend() const
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::cend();
    }

    size_t 
    size() const
    {
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::size();
    }

    bool 
    empty() 
    {        
        _my_lock_guard_type lock(_mtx);
        return _my_base_ty::empty();
    }

};


#endif
