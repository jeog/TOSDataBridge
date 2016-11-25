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

#ifndef JO_TOSDB_RAW_DATA_BLOCK
#define JO_TOSDB_RAW_DATA_BLOCK

#include "data_stream.hpp"
#include "client.hpp"
#include <memory>

/* implemented in src/raw_data_block.tpp */

#define RAW_DATA_BLOCK_TEMPLATE template<typename GenericTy, typename DateTimeTy>
#define RAW_DATA_BLOCK_CLASS RawDataBlock<GenericTy, DateTimeTy>
#define MAX_BLOCK_COUNT 10

template<typename GenericTy, typename DateTimeTy>
class RawDataBlock {        
    static size_type _block_count_;
    static size_type _max_block_count_;

    typedef DataStreamInterface<DateTimeTy, GenericTy> _my_stream_ty;  
    typedef std::unique_ptr<_my_stream_ty> _my_stream_uptr_ty; 

    typedef std::map<const TOS_Topics::TOPICS, 
                     _my_stream_uptr_ty, 
                     TOS_Topics::top_less> _my_row_ty;

    typedef std::pair<const TOS_Topics::TOPICS, _my_stream_uptr_ty> _my_row_elem_ty;
    typedef std::unique_ptr<_my_row_ty> _my_row_uptr_ty;  
    typedef std::pair<const std::string, _my_row_uptr_ty> _my_col_elem_ty;    
    typedef std::lock_guard<std::recursive_mutex>  _my_lock_guard_type;

    std::recursive_mutex* const _mtx;
    std::unordered_map<std::string, _my_row_uptr_ty> _block;

    size_type _block_sz;
    str_set_type _item_names;  
    topic_set_type _topic_enums;
    bool _datetime;  

    RawDataBlock(str_set_type sItems, 
                 topic_set_type tTopics, 
                 const size_type sz, 
                 bool datetime);

    RawDataBlock(const size_type sz, bool datetime);

    RawDataBlock(const RawDataBlock& block)
        : 
            _mtx(new std::recursive_mutex) 
        { 
            /* ++_block_count_; */ 
        }

    RawDataBlock(RawDataBlock&& block)
        :
            _mtx(new std::recursive_mutex) 
        { 
            /* */ 
        }

    RawDataBlock& 
    operator=(const RawDataBlock& block);

    RawDataBlock& 
    operator=(RawDataBlock&& block);

    void 
    _init();

    _my_row_ty*     
    _insert_topic(_my_row_ty*, TOS_Topics::TOPICS topic);

    _my_row_uptr_ty 
    _populate_tblock(_my_row_uptr_ty);

public:
    typedef GenericTy generic_type;
    typedef DateTimeTy datetime_type;
    typedef _my_stream_ty stream_type;
    typedef const _my_stream_ty* stream_const_ptr_type;
    
    typedef std::vector<generic_type> vector_type; 
    typedef std::pair<std::string, generic_type> pair_type; 
    typedef std::map<std::string, generic_type> map_type; 
    typedef std::map<std::string, map_type> matrix_type;
      
    typedef std::pair<std::vector<generic_type>, 
                      std::vector<DateTimeStamp>> vector_datetime_type;
    
    typedef std::map<std::string, 
                     std::pair<generic_type,datetime_type>> map_datetime_type;

    typedef std::map<std::string, map_datetime_type> matrix_datetime_type; 

    static RawDataBlock* const 
    CreateBlock(const str_set_type sItems, 
                const topic_set_type tTopics,               
                const size_type sz,
                const bool datetime);

    static RawDataBlock* const 
    CreateBlock(const size_type sz,const bool datetime);

    static inline size_type 
    block_count() 
    { 
        return _block_count_; 
    }

    static inline size_type 
    max_block_count() 
    { 
        return _max_block_count_; 
    }

    static inline size_type 
    max_block_count(const size_type m)
    {
        return (m >= _block_count_) ? (_max_block_count_ = m) : _max_block_count_; 
    }
    
    size_type 
    block_size(size_type b);

    inline size_type 
    block_size() const 
    { 
        return this->_block_sz; 
    }  

    inline size_type 
    item_count() const 
    { 
        return (size_type)(this->_item_names.size()); 
    }

    inline size_type 
    topic_count() const 
    { 
        return (size_type)(this->_topic_enums.size()); 
    }

    void 
    add_item(std::string item); 

    void 
    remove_item(std::string item); 

    void 
    add_topic(TOS_Topics::TOPICS topic); 

    void 
    remove_topic(TOS_Topics::TOPICS topic); 

    template<typename Val, typename DT> 
    void 
    insert_data(TOS_Topics::TOPICS topic,std::string item,Val val,DT datetime); 

    const _my_stream_ty* 
    raw_stream_ptr(std::string item, TOS_Topics::TOPICS topic) const;

    map_type 
    map_of_frame_items(TOS_Topics::TOPICS topic) const;

    map_type 
    map_of_frame_topics(std::string item) const;

    map_datetime_type 
    pair_map_of_frame_items(TOS_Topics::TOPICS) const;

    map_datetime_type 
    pair_map_of_frame_topics(std::string item) const;

    matrix_type 
    matrix_of_frame() const ;

    matrix_datetime_type 
    pair_matrix_of_frame() const ;
    
    inline topic_set_type 
    topics() const 
    { 
        return _topic_enums; 
    }

    inline str_set_type 
    items() const 
    { 
        return _item_names; 
    }

    inline bool 
    has_topic(TOS_Topics::TOPICS topic) const 
    {
        return (_topic_enums.find(topic) != _topic_enums.cend());
    }

    inline bool 
    has_item(const char* item) const 
    {
        return (_item_names.find(item) != _item_names.cend());
    }

    inline bool 
    uses_dtstamp() const 
    { 
        return this->_datetime; 
    }

    ~RawDataBlock() 
    {   /* all other deallocs are handled by unique_ptr destructors */  
        delete _mtx;        
        --_block_count_;
    }
}; 

#include "raw_data_block.tpp"

#endif
