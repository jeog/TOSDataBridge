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

#ifndef JO_TOSDB_RAW_DATA_BLOCK
#define JO_TOSDB_RAW_DATA_BLOCK

#include "data_stream.hpp"
#include "client.hpp"
#include <memory>

template< typename GenericTy, typename DateTimeTy >
class RawDataBlock {    
        
    typedef tosdb_data_stream::Interface< DateTimeTy,
                                          GenericTy >  _stream_type;    
    typedef std::unique_ptr< _stream_type >            _stream_uptr_type;
    typedef std::map< const TOS_Topics::TOPICS, 
                      _stream_uptr_type, 
                      TOS_Topics::top_less >           _row_type;
    typedef std::pair< const TOS_Topics::TOPICS, 
                       _stream_uptr_type >             _row_elem_type;
    typedef std::unique_ptr< _row_type >               _row_uptr_type;    
    typedef std::pair< const std::string, 
                       _row_uptr_type >                _col_elem_type;    
    typedef std::lock_guard< std::recursive_mutex >    _my_lock_guard_type;

    std::recursive_mutex* const                        _mtx;
    std::unordered_map< std::string, _row_uptr_type >  _block;

    size_type       _block_sz;
    str_set_type    _item_names;    
    topic_set_type  _topic_enums;
    bool            _datetime;    
        
    static size_type _block_count_;
    static size_type _max_block_count_;

    /* 
       FORCE PUBLIC TO USE FACTORIES 
    */ 
    RawDataBlock( str_set_type sItems, 
                  topic_set_type tTopics, 
                  const size_type sz, 
                  bool datetime ) 
        :
        _item_names( sItems ),
        _topic_enums( tTopics ),
        _block_sz( sz ),
        _datetime( datetime ),
        _mtx( new std::recursive_mutex )
    {            
        init();
        ++_block_count_;
    }

    RawDataBlock( const size_type sz, bool datetime )
        : 
        _item_names(),
        _topic_enums(),    
        _block_sz( sz ),
        _datetime( datetime ),
        _mtx( new std::recursive_mutex )
    {
        ++_block_count_;
    }

    RawDataBlock( const RawDataBlock& block )
        :
        _mtx( new std::recursive_mutex )
        {
            //++_block_count_;
            /* provide semantics if necessary */
        }

    RawDataBlock( RawDataBlock&& block )
        :
        _mtx( new std::recursive_mutex )
    {
        /* provide semantics if necessary */
    }

    RawDataBlock& operator=( const RawDataBlock& block )    
    {
        //++_block_count_;
        /* provide semantics if necessary */
    }

    RawDataBlock& operator=( RawDataBlock&& block )
    {
        /* provide semantics if necessary */
    }

    void init() 
    {
        _my_lock_guard_type lock(*_mtx);

        for (auto & i : _item_names){
            _block.insert( 
                _col_elem_type( i, 
                                std::move( populateTBlock( 
                                  std::move(_row_uptr_type(new _row_type)))))); 
        }
    }

    _row_type*      insertTopic( _row_type*, TOS_Topics::TOPICS topic);
    _row_uptr_type  populateTBlock( _row_uptr_type );

public:
    typedef GenericTy           generic_type;
    typedef DateTimeTy          datetime_type;
    typedef _stream_type        stream_type;
    typedef const _stream_type* stream_const_ptr_type;
    
    typedef std::vector< generic_type>             vector_type; 
    typedef std::pair< std::string, generic_type > pair_type; 
    typedef std::map< std::string, generic_type >  map_type; 
    typedef std::map< std::string, map_type >      matrix_type;
        
    typedef std::pair< std::vector< generic_type >, 
                       std::vector< DateTimeStamp > >   vector_datetime_type;
    typedef std::map< std::string, 
                      std::pair< generic_type, 
                                 datetime_type > >      map_datetime_type;
    typedef std::map< std::string, map_datetime_type >  matrix_datetime_type; 

    /*
        FACTORIES
    */
    static RawDataBlock* const 
    CreateBlock( const str_set_type sItems, 
                 const topic_set_type tTopics,                             
                 const size_type sz,
                 const bool datetime ) 
    {
        if( sItems.empty() || tTopics.empty() )    
            throw TOSDB_DataBlockError(
                "RawDataBlock<>::CreateBlock() : sItems and/or tTopics empty"); 
          
        if ( _block_count_ < _max_block_count_ ) 
            return new RawDataBlock( inames, itopics, sz, datetime ) ; 
        else        
            throw TOSDB_DataBlockLimitError( _max_block_count_ );           
        return nullptr;
    }

    static RawDataBlock* const 
    CreateBlock( const size_type sz, const bool datetime ) 
    {
        if ( _block_count_ < _max_block_count_ ) 
            return new RawDataBlock( sz, datetime );
        else    
            throw TOSDB_DataBlockLimitError( _max_block_count_ );   
        return nullptr;
    }

    /* 
        PLACE-NEW 
    */
    static void CreateBlock( RawDataBlock* dest,
                             const str_set_type sItems, 
                             const topic_set_type tTopics,                             
                             const size_type sz,
                             const bool datetime ) 
    {
        if( sItems.empty() || tTopics.empty() )    
            throw TOSDB_DataBlockError(
                "RawDataBlock<>::CreateBlock() : sItems and/or tTopics empty");    

        if ( _block_count_ < _max_block_count_ ) 
            new(dest) RawDataBlock( inames, itopics, sz, datetime );
        else
            throw TOSDB_DataBlockLimitError( _max_block_count_ );
        return nullptr;
    }

    static void CreateBlock( RawDataBlock* dest, 
                             const size_type sz,
                             const bool datetime ) 
    {
        if ( _block_count_ < _max_block_count_ ) 
            new(dest) RawDataBlock( sz, datetime ); 
        else
            throw TOSDB_DataBlockLimitError( _max_block_count_ );
        return nullptr;
    }

    static size_type block_count() 
    {
        return _block_count_;
    }

    static size_type max_block_count()
    {
        return _max_block_count_;
    }

    static size_type max_block_count(const size_type mbc)
    {
        return (mbc >= _block_count_) 
                ? (_max_block_count_ = mbc) 
                : _max_block_count_ ; 
    }
    
    size_type block_size( size_type b )
    {
        _my_lock_guard_type lock(*_mtx);

        if( b > INT_MAX ) /* don't allow larger blocks */
            b = INT_MAX; 

        for(auto & rawPr : _block)
            for(auto & tPr : *(rawPr.second) )
                tPr.second->bound_size(b);

        return (_block_sz = b);
    }

    size_type block_size() const 
    {
        return _block_sz;
    }    

    size_type item_count() const
    {
        return (size_type)(_item_names.size());
    }

    size_type topic_count() const
    {
        return (size_type)(_topic_enums.size());
    }

    void add_item( std::string item ); 

    void remove_item( std::string item ); 

    void add_topic( TOS_Topics::TOPICS topic ); 

    void remove_topic( TOS_Topics::TOPICS topic ); 

    template < typename Val, typename DT > 
    void insertData( TOS_Topics::TOPICS topic, 
                     std::string item, 
                     Val val, 
                     DT&& datetime ); 

    const _stream_type* raw_stream_ptr( std::string item, 
                                        TOS_Topics::TOPICS topic ) const ;

    map_type map_of_frame_items(TOS_Topics::TOPICS topic) const ;

    map_type map_of_frame_topics( std::string item ) const ;

    map_datetime_type pair_map_of_frame_items( TOS_Topics::TOPICS ) const ;

    map_datetime_type pair_map_of_frame_topics( std::string item ) const ;

    matrix_type matrix_of_frame() const ;

    matrix_datetime_type pair_matrix_of_frame() const ;
    
    topic_set_type topics() const
    {
        return _topic_enums;
    }

    str_set_type items() const
    {
        return _item_names;
    }

    bool has_topic( TOS_Topics::TOPICS topic) const
    {
        return (_topic_enums.find(topic) != _topic_enums.cend());
    }

    bool has_item( const char* item) const
    {
        return (_item_names.find(item) != _item_names.cend());
    }

    bool uses_dtstamp() const
    {
        return this->_datetime;
    }

    ~RawDataBlock() 
    {
        delete _mtx;        
        /* 
           All the heap deallocs are handled by unique_ptr destructors 
        */    
        --_block_count_;
    }
}; 

template< typename T, typename T2 > 
size_type RawDataBlock<T,T2>::_block_count_ = 0;

template< typename T, typename T2 > 
size_type RawDataBlock< T, T2>::_max_block_count_ = 10;

template< typename TG, typename TD >
template < typename Val, typename DT > 
void RawDataBlock<TG,TD>::insertData( TOS_Topics::TOPICS topic, 
                                      std::string item, 
                                      Val val, 
                                      DT&& datetime ) 
{
    _stream_type *stream; 
    _row_type    *topics;     

    try{        
        _my_lock_guard_type lock(*_mtx);    
        topics = _block.at(item).get(); 

        if ( !topics ) 
            throw TOSDB_DataBlockError(
                "insertData() _block returned nullptr for this item" );   

        stream = (topics->at( topic )).get();
        if ( !stream ) 
            throw TOSDB_DataBlockError(
                "insertData() _block returned nullptr for this topic" );  
          
        stream->push( val, std::move(datetime) );                   
    }catch( const std::out_of_range& ){        
        TOSDB_LogH("DataBlock"," DDE_DATA<T> rejected from block ");
        throw;
    }catch( const TOSDB_Error& ){    
        throw;
    }catch( const tosdb_data_stream::error& e){        
        throw TOSDB_DataStreamError(e, "insertData()", e.what());
    }catch( const std::exception & e){        
        throw TOSDB_DataBlockError(e, "insertData()");
    }catch( ... ){        
        throw TOSDB_DataBlockError("insertData(); Unknown Exception");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::add_item( std::string item ) 
{        
    try{
        _my_lock_guard_type lock(*_mtx);

        if( _item_names.insert( item ).second ){

            _block.erase( item );
            _block.insert( 
                _col_elem_type(item, 
                                std::move( populateTBlock( 
                                  std::move(_row_uptr_type(new _row_type))))));
        }   
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "add_item()");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::remove_item( std::string item )
{    
    try{
        _my_lock_guard_type lock(*_mtx);

        if( _item_names.erase( item ) ){
            _row_type * row = _block.at(item).get();

            if( row ){
                for(auto & elem : *row)
                    elem.second.reset();
                    
                _block.at(item).reset();
                _block.erase(item);            
            }            
        }       
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "remove_item(): out_of_range exception; probably invalid "
                    "item parameter" );
        throw TOSDB_DataBlockError(e, "remove_item() probably invalid item");    
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "remove_item()");
    }    
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::add_topic( TOS_Topics::TOPICS topic) 
{    
    try{
        _my_lock_guard_type lock(*_mtx);

        if( _topic_enums.insert( topic ).second ) 
            for (auto & elem : _block)
                insertTopic( elem.second.get(), topic );         
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "add_topic()");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::remove_topic( TOS_Topics::TOPICS topic)
{    
    try{
        _my_lock_guard_type lock(*_mtx);

        if( _topic_enums.erase( topic ) ) 
            for(auto & elem : _block) {
                _row_type *row = elem.second.get();

                if( row ){
                    (row->at(topic)).reset();
                    row->erase(topic);
                }
            }                
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "remove_topic(): out_of_range exception; probably "
                    "invalid topic parameter" );
        throw TOSDB_DataBlockError( e, "remove_item() probably invalid topic");
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "remove_topic()");        
    }    
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::_row_type* 
RawDataBlock<TG,TD>::insertTopic( typename RawDataBlock<TG,TD>::_row_type* row, 
                                  TOS_Topics::TOPICS topic)
{        
    _stream_type *stream;

    switch( TOS_Topics::TypeBits(topic) ){ 
    case TOSDB_STRING_BIT :
        stream = this->_datetime 
            ? new tosdb_data_stream::Object< std::string, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _block_sz ) 
            : new tosdb_data_stream::Object< std::string, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _block_sz );
        break;
    case TOSDB_INTGR_BIT :
        stream = this->_datetime 
            ? new tosdb_data_stream::Object< def_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _block_sz ) 
            : new tosdb_data_stream::Object< def_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _block_sz );
        break;
    case TOSDB_QUAD_BIT :
        stream = this->_datetime 
            ? new tosdb_data_stream::Object< ext_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _block_sz ) 
            : new tosdb_data_stream::Object< ext_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _block_sz );
        break;
    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :
        stream = this->_datetime 
            ? new tosdb_data_stream::Object< ext_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _block_sz )
            : new tosdb_data_stream::Object< ext_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _block_sz );
        break;
    default :
        stream = this->_datetime 
            ? new tosdb_data_stream::Object< def_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _block_sz ) 
            : new tosdb_data_stream::Object< def_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _block_sz );
    }
    row->erase(topic);

    try{
        row->insert( _row_elem_type( topic, 
                                     _stream_uptr_type( std::move(stream) )));
    }catch( ... ){
        TOSDB_LogH("DataBlock","insertTopic(): Problem inserting t-block");
        if(stream)
            delete stream;
    }

    return row;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::_row_uptr_type 
RawDataBlock<TG,TD>::populateTBlock(_row_uptr_type tBlock)
{        
    for(auto elem : _topic_enums)
        insertTopic( tBlock.get(), elem );

    return std::move(tBlock);
}

template< typename TG, typename TD > 
const typename RawDataBlock<TG,TD>::_stream_type*
RawDataBlock<TG,TD>::raw_stream_ptr( std::string item, 
                                     TOS_Topics::TOPICS topic ) const 
{
    const _stream_type * stream = nullptr;

    try{
        _my_lock_guard_type lock(*_mtx);
        _row_type* row = _block.at(item).get();

        if( row )                                        
            stream = (row->at(topic)).get();

    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "raw_stream_ptr(): out_of_range exception; "
                    "probably invalid item and/or topic" );
        throw TOSDB_DataBlockError( e, "DataStream does not exist in block" );
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "raw_stream_ptr()");
    }

    if( !stream )
        throw TOSDB_DataBlockError("DataStream does not exist in block");    

    return stream;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_type
RawDataBlock<TG,TD>::map_of_frame_topics( std::string item ) const 
{
    map_type map;

    try{
        _my_lock_guard_type lock(*_mtx);
        _row_type * row = _block.at(item).get();        

        if ( row )
            for( auto & elem : *row)
                map.insert( std::move( 
                    pair_type( TOS_Topics::map[elem.first], 
                               elem.second->operator[](0) )));

    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "map_of_frame_topics(): out_of_range exception; "
                    "probably invalid item param" );
        throw TOSDB_DataBlockError( e,
            "map_of_frame_topics(); probably invalid item param" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::map_of_frame_topics()" );
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError(e, "map_of_frame_topics()");
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_type
RawDataBlock<TG,TD>::map_of_frame_items(TOS_Topics::TOPICS topic) const 
{
    map_type map; 

    try{
        _my_lock_guard_type lock(*_mtx);

        for( auto & elem : _block)
            map.insert( std::move( 
                pair_type( elem.first, 
                           elem.second->at(topic)->operator[](0) )));  

    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "map_of_frame_items(): out_of_range exception; "
                    "probably invalid topic param" );
        throw TOSDB_DataBlockError( e, 
            "map_of_frame_items(); probably invalid topic" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::map_of_frame_items()" );
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "map_of_frame_items()");
    }

    return map;
}


template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_datetime_type
RawDataBlock<TG,TD>::pair_map_of_frame_topics( std::string item ) const 
{
    map_datetime_type map;

    try{
        _my_lock_guard_type lock(*_mtx);
        _row_type * row = _block.at(item).get();        

        if ( row )
            for( auto & elem : *row)
                map.insert( std::move(  
                    map_datetime_type::value_type(
                        TOS_Topics::map[elem.first], 
                        elem.second->both(0) )));

    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "pair_map_of_frame_topics(): out_of_range exception; "
                    "probably invalid item param" );
        throw TOSDB_DataBlockError( e, 
            "pair_map_of_frame_topics(); probably invalid item" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::pair_map_of_frame_topics()" );
    }catch(const std::exception & e){
        throw TOSDB_DataBlockError( e, "pair_map_of_frame_topics()" );
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_datetime_type
RawDataBlock<TG,TD>::pair_map_of_frame_items(TOS_Topics::TOPICS topic) const 
{
    map_datetime_type map; 

    try{
        _my_lock_guard_type lock(*_mtx);

        for( auto & elem : _block)
            map.insert( std::move( 
                map_datetime_type::value_type( 
                    elem.first, elem.second->at(topic)->both(0) )));             

    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "pair_map_of_frame_items(): out_of_range exception; "
                    "probably invalid topic param" );
        throw TOSDB_DataBlockError( e, 
            "pair_map_of_frame_items(); probably invalid topic");
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::pair_map_of_frame_items()" );
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "pair_map_of_frame_items()");
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::matrix_type
RawDataBlock<TG,TD>::matrix_of_frame() const 
{
    matrix_type matrix;

    try{
        _my_lock_guard_type lock(*_mtx);

        for(auto & items : _block){            
            map_type map; 
            for(auto & tops : *items.second)
                map.insert( std::move( 
                    map_type::value_type( 
                        std::move( TOS_Topics::map[tops.first] ),
                        tops.second->operator[](0) ))); 

            matrix.insert( std::move( 
                matrix_type::value_type( items.first, std::move(map) )));
        }        
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::matrix_of_frame()" );
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "matrix_of_frame()");
    }

    return matrix; 
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::matrix_datetime_type
RawDataBlock<TG,TD>::pair_matrix_of_frame() const 
{ 
    matrix_datetime_type matrix;

    try{
        _my_lock_guard_type lock(*_mtx);

        for(auto & items : _block){            
            map_datetime_type map; 
            for(auto & tops : *items.second)
                map.insert( std::move( 
                    map_datetime_type::value_type( 
                        std::move( TOS_Topics::map[tops.first] ),
                        std::move( tops.second->both(0) )  ))); 

            matrix.insert( std::move( 
                matrix_datetime_type::value_type(items.first,std::move(map))));
        }        
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, "tosdb_data_stream error caught and "
            "encapsulated in RawDataBlock::pair_matrix_of_frame()" );
    }catch( const std::exception & e ){
        throw TOSDB_DataBlockError(e, "matrix_of_frame()");
    }

    return matrix; 
}

#endif
