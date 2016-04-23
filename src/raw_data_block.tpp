

RAW_DATA_BLOCK_TEMPLATE
size_type RAW_DATA_BLOCK_CLASS::_block_count_ = 0;

RAW_DATA_BLOCK_TEMPLATE
size_type RAW_DATA_BLOCK_CLASS::_max_block_count_ = MAX_BLOCK_COUNT;


RAW_DATA_BLOCK_TEMPLATE
RAW_DATA_BLOCK_CLASS::RawDataBlock(str_set_type sItems, 
                                   topic_set_type tTopics, 
                                   const size_type sz, 
                                   bool datetime) 
    :
        _item_names(sItems),
        _topic_enums(tTopics),
        _block_sz(sz),
        _datetime(datetime),
        _mtx(new std::recursive_mutex)
    {      
        this->_init();
        ++_block_count_;
    }

RAW_DATA_BLOCK_TEMPLATE
RAW_DATA_BLOCK_CLASS::RawDataBlock(const size_type sz, bool datetime)
    : 
        _item_names(),
        _topic_enums(),  
        _block_sz(sz),
        _datetime(datetime),
        _mtx(new std::recursive_mutex)
    {
        ++_block_count_;
    }

RAW_DATA_BLOCK_TEMPLATE
void
RAW_DATA_BLOCK_CLASS::_init() 
{
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    for(auto & i : this->_item_names) {
        _my_row_uptr_ty tmp = this->_populate_tblock(_my_row_uptr_ty(new _my_row_ty));
        this->_block.insert(_my_col_elem_ty(i, std::move(tmp)));   
    }
    /* --- CRITICAL SECTION --- */
}

RAW_DATA_BLOCK_TEMPLATE 
typename RAW_DATA_BLOCK_CLASS::_my_row_ty* 
RAW_DATA_BLOCK_CLASS::_insert_topic(typename RAW_DATA_BLOCK_CLASS::_my_row_ty* row, 
                                   TOS_Topics::TOPICS topic)
{    
  _my_stream_ty *stream; /*
  stream = new DataStream< TOS_Topics::Type<topic>::type, 
                           datetime_type, generic_type, this->_datetime >(_block_sz);

  */
  switch(TOS_Topics::TypeBits(topic)){ 
  case TOSDB_STRING_BIT :
    stream = this->_datetime 
        ? new DataStream<std::string, datetime_type, generic_type, true>(_block_sz) 
        : new DataStream<std::string, datetime_type, generic_type, false>(_block_sz);
    break;
  case TOSDB_INTGR_BIT :
    stream = this->_datetime 
        ? new DataStream<def_size_type, datetime_type, generic_type, true>(_block_sz) 
        : new DataStream<def_size_type, datetime_type, generic_type, false>(_block_sz);
    break;
  case TOSDB_QUAD_BIT :
    stream = this->_datetime 
        ? new DataStream<ext_price_type, datetime_type, generic_type, true>(_block_sz) 
        : new DataStream<ext_price_type, datetime_type, generic_type, false>(_block_sz);
    break;
  case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :
    stream = this->_datetime 
        ? new DataStream<ext_size_type, datetime_type, generic_type, true>(_block_sz)
        : new DataStream<ext_size_type, datetime_type, generic_type, false>(_block_sz);
    break;
  default :
    stream = this->_datetime 
        ? new DataStream<def_price_type, datetime_type, generic_type, true>(_block_sz) 
        : new DataStream<def_price_type, datetime_type, generic_type, false>(_block_sz);
  } 
  try{
    row->erase(topic);
    row->insert(_my_row_elem_ty(topic,_my_stream_uptr_ty(stream)));
  }catch(...){
    TOSDB_LogH("RawDataBlock","problem inserting t-block");
    if(stream)
        delete stream;
  }
  return row;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::_my_row_uptr_ty 
RAW_DATA_BLOCK_CLASS::_populate_tblock(typename RAW_DATA_BLOCK_CLASS::_my_row_uptr_ty tBlock)
{    
  for(auto elem : this->_topic_enums)
      this->_insert_topic(tBlock.get(), elem);

  return tBlock;
}

RAW_DATA_BLOCK_TEMPLATE
RAW_DATA_BLOCK_CLASS* const
RAW_DATA_BLOCK_CLASS::CreateBlock(const str_set_type sItems, 
                                  const topic_set_type tTopics,               
                                  const size_type sz,
                                  const bool datetime) 
{
    if(sItems.empty())
        throw TOSDB_DataBlockError("sItems empty"); 
    
    if(tTopics.empty())
        throw TOSDB_DataBlockError("tTopics empty");       

    if (_block_count_ >= _max_block_count_) 
        throw TOSDB_DataBlockLimitError(_max_block_count_); 

    return new RawDataBlock(sItems, tTopics, sz, datetime) ;              
}

RAW_DATA_BLOCK_TEMPLATE
RAW_DATA_BLOCK_CLASS* const
RAW_DATA_BLOCK_CLASS::CreateBlock(const size_type sz,const bool datetime) 
{
    if (_block_count_ >= _max_block_count_) 
        throw TOSDB_DataBlockLimitError(_max_block_count_);
      
    return new RawDataBlock(sz, datetime);             
}

RAW_DATA_BLOCK_TEMPLATE
size_type
RAW_DATA_BLOCK_CLASS::block_size(size_type b)
{
    _my_lock_guard_type lock(*_mtx);
    /* --- CRITICAL SECTION --- */
    if(b > TOSDB_MAX_BLOCK_SZ)
        b = TOSDB_MAX_BLOCK_SZ; 

    for(auto& col : this->_block){
        for(auto& row : *(col.second))
            row.second->bound_size(b);
    }

    return (_block_sz = b);
    /* --- CRITICAL SECTION --- */
}

 
RAW_DATA_BLOCK_TEMPLATE
template<typename ValTy, typename DtTy>
void
RAW_DATA_BLOCK_CLASS::insert_data(TOS_Topics::TOPICS topic, 
                                  std::string item, 
                                  ValTy val, 
                                  DtTy datetime) 
{
    _my_stream_ty *stream; 
    _my_row_ty  *topics; 
        
    _my_lock_guard_type lock(*_mtx);  
    /* --- CRITICAL SECTION --- */
    try{
        topics = this->_block.at(item).get();
        if (!topics)
            throw std::out_of_range("item not in block");
    }catch(const std::out_of_range& e){    
        TOSDB_LogH("RawDataBlock", e.what());
        throw TOSDB_DataBlockError(e.what()); 
    }

    try{
        stream = (topics->at(topic)).get();
        if (!stream) 
            throw std::out_of_range("topic not in block");
    }catch(const std::out_of_range& e){    
        TOSDB_LogH("RawDataBlock", e.what());
        throw TOSDB_DataBlockError(e.what()); 
    } 
          
    try{      
        stream->push(val, std::move(datetime)); 
    }catch(const DataStreamError& e){    
        throw TOSDB_DataStreamError(e, "insert_data");
    }
    /* --- CRITICAL SECTION --- */
}

RAW_DATA_BLOCK_TEMPLATE
void
RAW_DATA_BLOCK_CLASS::add_item(std::string item) 
{    
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        if( !(this->_item_names.insert(item).second) )
            return;
        
        this->_block.erase(item);
        _my_row_uptr_ty tmp = 
            this->_populate_tblock(_my_row_uptr_ty(new _my_row_ty));
        this->_block.insert(_my_col_elem_ty(item,std::move(tmp)));           
        /* --- CRITICAL SECTION --- */
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "add_item");
    }
}

RAW_DATA_BLOCK_TEMPLATE
void
RAW_DATA_BLOCK_CLASS::remove_item(std::string item)
{  
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        if( !(this->_item_names.erase(item)) )
            return;
        
        _my_row_ty *row = this->_block.at(item).get();
        if(!row)
            return;

        for(auto & elem : *row)
            elem.second.reset();                   

        this->_block.at(item).reset();
        this->_block.erase(item);   
                   
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "remove_item out_of_range exception");
        throw TOSDB_DataBlockError(e, "remove_item out_of_range");  
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "remove_item");
    }  
}

RAW_DATA_BLOCK_TEMPLATE
void
RAW_DATA_BLOCK_CLASS::add_topic(TOS_Topics::TOPICS topic) 
{  
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        if( !(this->_topic_enums.insert(topic).second) )
            return;
        
        for(auto & elem : this->_block)
            this->_insert_topic(elem.second.get(), topic);         
        /* --- CRITICAL SECTION --- */
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "add_topic");
    }
}

RAW_DATA_BLOCK_TEMPLATE
void
RAW_DATA_BLOCK_CLASS::remove_topic(TOS_Topics::TOPICS topic)
{  
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        if( !(this->_topic_enums.erase(topic)) )
            return;
        
        for(auto & elem : this->_block){
            _my_row_ty *row = elem.second.get();
            if(row){
                (row->at(topic)).reset();
                row->erase(topic);
            }
        }           
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "remove_topic out_of_range exception");
        throw TOSDB_DataBlockError(e, "remove_topic out_of_range");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "remove_topic");    
    }  
}

RAW_DATA_BLOCK_TEMPLATE
const typename RAW_DATA_BLOCK_CLASS::_my_stream_ty*
RAW_DATA_BLOCK_CLASS::raw_stream_ptr(std::string item, 
                                     TOS_Topics::TOPICS topic) const 
{
    const _my_stream_ty * stream = nullptr;
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        _my_row_ty* row = this->_block.at(item).get();
        if(row)                    
            stream = (row->at(topic)).get();
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "raw_stream_ptr out_of_range exception");
        throw TOSDB_DataBlockError(e, "raw_stream_ptr out_of_range");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "raw_stream_ptr");
    }

    if(!stream)
        throw TOSDB_DataBlockError("stream does not exist in block");  

    return stream;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::map_type
RAW_DATA_BLOCK_CLASS::map_of_frame_topics(std::string item) const 
{
    map_type map;
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        _my_row_ty * row = this->_block.at(item).get();    
        if(!row)
            return map;
            
        for(auto & elem : *row){
            map.insert( pair_type( TOS_Topics::map[elem.first],
                                   elem.second->operator[](0)) );
        }        
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "map_of_frame_topics out_of_range exception");
        throw TOSDB_DataBlockError(e, "map_of_frame_topics out_of_range");
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "map_of_frame_topics");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "map_of_frame_topics");
    }

    return map;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::map_type
RAW_DATA_BLOCK_CLASS::map_of_frame_items(TOS_Topics::TOPICS topic) const 
{
    map_type map; 
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        for(auto & elem : this->_block){
            map.insert( pair_type( elem.first,
                                   elem.second->at(topic)->operator[](0)) );  
        }
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "map_of_frame_items out_of_range exception");
        throw TOSDB_DataBlockError(e, "map_of_frame_items out_of_range");
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "map_of_frame_items");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "map_of_frame_items");
    }

    return map;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::map_datetime_type
RAW_DATA_BLOCK_CLASS::pair_map_of_frame_topics(std::string item) const 
{
    map_datetime_type map;
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        _my_row_ty * row = this->_block.at(item).get(); 
        if(!row)
            return map;

        for(auto & elem : *row){
            map.insert( map_datetime_type::value_type(
                            TOS_Topics::map[elem.first],
                            elem.second->both(0)) );
        }        
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "pair_map_of_frame_topics out_of_range exception");
        throw TOSDB_DataBlockError(e, "pair_map_of_frame_topics out_of_range");
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "pair_map_of_frame_topics");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "pair_map_of_frame_topics");
    }
    
    return map;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::map_datetime_type
RAW_DATA_BLOCK_CLASS::pair_map_of_frame_items(TOS_Topics::TOPICS topic) const 
{
    map_datetime_type map; 
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        for(auto & elem : this->_block){
            map.insert( map_datetime_type::value_type(
                            elem.first, 
                            elem.second->at(topic)->both(0)) );       
        }
        /* --- CRITICAL SECTION --- */
    }catch(const std::out_of_range& e){
        TOSDB_LogH("RawDataBlock", "pair_map_of_frame_items out_of_range exception");
        throw TOSDB_DataBlockError(e, "pair_map_of_frame_items out_of_range");
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "pair_map_of_frame_items");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "pair_map_of_frame_items");
    }
    
    return map;
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::matrix_type
RAW_DATA_BLOCK_CLASS::matrix_of_frame() const 
{
    matrix_type matrix;
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        for(auto & items : this->_block){         
            map_type map; 
            for(auto & tops : *items.second){
                map.insert( map_type::value_type(
                                TOS_Topics::map[tops.first],
                                tops.second->operator[](0)) ); 
            }
            matrix.insert(matrix_type::value_type(items.first, std::move(map)));
        }    
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "matrix_of_frame");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "matrix_of_frame");
    }

    return matrix; 
}

RAW_DATA_BLOCK_TEMPLATE
typename RAW_DATA_BLOCK_CLASS::matrix_datetime_type
RAW_DATA_BLOCK_CLASS::pair_matrix_of_frame() const 
{ 
    matrix_datetime_type matrix;
    try{
        _my_lock_guard_type lock(*_mtx);
        /* --- CRITICAL SECTION --- */
        for(auto & items : this->_block){      
            map_datetime_type map; 
            for(auto & tops : *items.second){
                map.insert( map_datetime_type::value_type(
                                TOS_Topics::map[tops.first],
                                tops.second->both(0)) ); 
            }
            matrix.insert(
                matrix_datetime_type::value_type(items.first,std::move(map)));
        }    
        /* --- CRITICAL SECTION --- */
    }catch(const DataStreamError& e){
        throw TOSDB_DataStreamError(e, "pair_matrix_of_frame");
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "pair_matrix_of_frame");
    }
    
    return matrix; 
}
