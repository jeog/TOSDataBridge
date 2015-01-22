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
                                          GenericTy >    _iStreamTy;    
    typedef std::unique_ptr< _iStreamTy >                _iStreamPtrTy;
    typedef std::map< const TOS_Topics::TOPICS, 
                      _iStreamPtrTy, 
                      TOS_Topics::top_less >             _tRowTy;
    typedef std::pair< const TOS_Topics::TOPICS, 
                       _iStreamPtrTy >                   _tRowElemTy;
    typedef std::unique_ptr< _tRowTy >                   _tRowPtrTy;    
    typedef std::pair< const std::string , _tRowPtrTy >  _rawBlockElemTy;    
    typedef std::lock_guard< std::recursive_mutex >      _guardTy;

    std::recursive_mutex* const                          _mtxR;
    std::unordered_map< std::string, _tRowPtrTy >        _rawBlock;

    size_type       _blockSize;
    str_set_type    _itemNames;    
    topic_set_type  _topicEnums;
    bool            _dtFlag;    
        
    static size_type _blockCount_;
    static size_type _maxBlockCount_;

    /* 
       FORCE PUBLIC TO USE FACTORIES 
    */ 
    RawDataBlock( str_set_type sItems, 
                  topic_set_type tTopics, 
                  const size_type sz, 
                  bool datetime ) 
        :
        _itemNames( sItems ),
        _topicEnums( tTopics ),
        _blockSize( sz ),
        _dtFlag( datetime ),
        _mtxR( new std::recursive_mutex )
    {            
        init();
        ++_blockCount_;
    }

    RawDataBlock( const size_type sz, bool datetime )
        : 
        _itemNames(),
        _topicEnums(),    
        _blockSize( sz ),
        _dtFlag( datetime ),
        _mtxR( new std::recursive_mutex )
    {
        ++_blockCount_;
    }

    RawDataBlock( const RawDataBlock& block )
        :
        _mtxR( new std::recursive_mutex )
        {
            //++_blockCount_;
            /* provide semantics if necessary */
        }

    RawDataBlock( RawDataBlock&& block )
        :
        _mtxR( new std::recursive_mutex )
    {
        /* provide semantics if necessary */
    }

    RawDataBlock& operator=( const RawDataBlock& block )    
    {
        //++_blockCount_;
        /* provide semantics if necessary */
    }

    RawDataBlock& operator=( RawDataBlock&& block )
    {
        /* provide semantics if necessary */
    }

    void init() 
    {
        _guardTy _lock_(*_mtxR);

        for (auto & i : _itemNames){
            _rawBlock.insert( 
                _rawBlockElemTy( i, 
                                 std::move( 
                                   populateTBlock( 
                                     std::move( _tRowPtrTy(new _tRowTy)))))); 
        }
    }

    _tRowTy*   insertTopic( _tRowTy*, TOS_Topics::TOPICS topic);
    _tRowPtrTy populateTBlock( _tRowPtrTy );

public:
    typedef GenericTy         generic_type;
    typedef DateTimeTy        datetime_type;
    typedef _iStreamTy        stream_type;
    typedef const _iStreamTy* stream_const_ptr_type;
    
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
        if( sItems.empty() || tTopics.empty() ){    
            throw TOSDB_data_block_error("RawDataBlock<>::CreateBlock(...) : " 
                                         "sItems and tTopics can not be empty"); 
        }
           
        if ( _blockCount_ < _maxBlockCount_ ) 
            return new RawDataBlock( inames, itopics, sz, datetime ) ; 
        else        
            throw TOSDB_data_block_limit_error( _maxBlockCount_ );  
          
        return nullptr;
    }

    static RawDataBlock* const 
    CreateBlock( const size_type sz, const bool datetime ) 
    {
        if ( _blockCount_ < _maxBlockCount_ ) 
            return new RawDataBlock( sz, datetime );
        else    
            throw TOSDB_data_block_limit_error( _maxBlockCount_ ); 
   
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
        if( sItems.empty() || tTopics.empty() ){    
            throw TOSDB_data_block_error("RawDataBlock<>::CreateBlock(...) : " 
                                         "sItems and tTopics can not be empty");    
        }

        if ( _blockCount_ < _maxBlockCount_ ) 
            new(dest) RawDataBlock( inames, itopics, sz, datetime );
        else
            throw TOSDB_data_block_limit_error( _maxBlockCount_ );

        return nullptr;
    }

    static void CreateBlock( RawDataBlock* dest, 
                             const size_type sz,
                             const bool datetime ) 
    {
        if ( _blockCount_ < _maxBlockCount_ ) 
            new(dest) RawDataBlock( sz, datetime ); 
        else
            throw TOSDB_data_block_limit_error( _maxBlockCount_ );

        return nullptr;
    }

    static size_type block_count() 
    {
        return _blockCount_;
    }

    static size_type max_block_count()
    {
        return _maxBlockCount_;
    }

    static size_type 
    max_block_count(const size_type mbc)
    {
        return (mbc >= _blockCount_) 
                ? (_maxBlockCount_ = mbc) 
                : _maxBlockCount_ ; 
    }
    
    size_type block_size( size_type b )
    {
        _guardTy _lock_(*_mtxR);

        if( b > INT_MAX ) /* don't allow larger blocks */
            b = INT_MAX; 

        for(auto & rawPr : _rawBlock)
            for(auto & tPr : *(rawPr.second) )
                tPr.second->bound_size(b);

        return (_blockSize = b);
    }

    size_type block_size() const 
    {
        return _blockSize;
    }    

    size_type item_count() const
    {
        return (size_type)(_itemNames.size());
    }

    size_type topic_count() const
    {
        return (size_type)(_topicEnums.size());
    }

    void add_item( std::string sItem ); 

    void remove_item( std::string sItem ); 

    void add_topic( TOS_Topics::TOPICS tTopic ); 

    void remove_topic( TOS_Topics::TOPICS tTopic ); 

    template < typename Val, typename DT > 
    void insertData( TOS_Topics::TOPICS tTopic, 
                     std::string sItem, 
                     Val val, 
                     DT&& datetime ); 

    const _iStreamTy*   
    raw_stream_ptr( std::string sItem, TOS_Topics::TOPICS tTopic ) const ;

    map_type 
    map_of_frame_items(TOS_Topics::TOPICS tTopic) const ;

    map_type 
    map_of_frame_topics( std::string sItem ) const ;

    map_datetime_type 
    pair_map_of_frame_items( TOS_Topics::TOPICS ) const ;

    map_datetime_type 
    pair_map_of_frame_topics( std::string sItem ) const ;

    matrix_type 
    matrix_of_frame() const ;

    matrix_datetime_type     
    pair_matrix_of_frame() const ;
    
    topic_set_type topics() const
    {
        return _topicEnums;
    }

    str_set_type items() const
    {
        return _itemNames;
    }

    bool has_topic( TOS_Topics::TOPICS tTopic) const
    {
        return (_topicEnums.find(tTopic) != _topicEnums.cend());
    }

    bool has_item( const char* sItem) const
    {
        return (_itemNames.find(sItem) != _itemNames.cend());
    }

    bool uses_dtstamp() const
    {
        return this->_dtFlag;
    }

    ~RawDataBlock() 
    {
        delete _mtxR;        
        /* 
           All the heap deallocs are handled by unique_ptr destructors 
        */    
        --_blockCount_;
    }
}; 

template< typename T, typename T2 > 
size_type RawDataBlock<T,T2>::_blockCount_ = 0;

template< typename T, typename T2 > 
size_type RawDataBlock< T, T2>::_maxBlockCount_ = 10;

template< typename TG, typename TD >
template < typename Val, typename DT > 
void RawDataBlock<TG,TD>::insertData( TOS_Topics::TOPICS tTopic, 
                                      std::string sItem, 
                                      Val val, 
                                      DT&& datetime ) 
{
    _iStreamTy *dBlck; 
    _tRowTy    *tBlck;     

    try{        
        _guardTy _lock_(*_mtxR);    
        tBlck = _rawBlock.at(sItem).get(); 

        if ( !tBlck ) 
            throw TOSDB_data_block_error("insertData() _rawBlock returned " 
                                         "nullptr for this item");   

        dBlck = (tBlck->at( tTopic )).get();
        if ( !dBlck ) 
            throw TOSDB_data_block_error("insertData() _rawBlock returned " 
                                         "nullptr for this topic");  
          
        dBlck->push( val, std::move(datetime) );                   
    }catch( const std::out_of_range& ){        
        TOSDB_LogH("DataBlock"," DDE_DATA<T> rejected from block ");
        throw;
    }catch( const TOSDB_error& ){    
        throw;
    }catch( const tosdb_data_stream::error& e){        
        throw TOSDB_data_stream_error(e, "insertData()", e.what());
    }catch( const std::exception & e){        
        throw TOSDB_data_block_error(e, "insertData()");
    }catch( ... ){        
        throw TOSDB_data_block_error("insertData(); Unknown Exception");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::add_item( std::string sItem ) 
{        
    try{
        _guardTy _lock_(*_mtxR);

        if( _itemNames.insert( sItem ).second ){

            _rawBlock.erase( sItem );
            _rawBlock.insert( 
                _rawBlockElemTy( sItem, 
                                 std::move( 
                                   populateTBlock( 
                                     std::move( _tRowPtrTy( new _tRowTy ))))));
        }   
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "add_item()");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::remove_item( std::string sItem )
{    
    try{
        _guardTy _lock_(*_mtxR);

        if( _itemNames.erase( sItem ) ){
            _tRowTy * tBlck = _rawBlock.at(sItem).get();

            if( tBlck ){
                for(auto & tBpair : *tBlck)
                    tBpair.second.reset();
                    
                _rawBlock.at(sItem).reset();
                _rawBlock.erase(sItem);            
            }            
        }       
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "remove_item(): out_of_range exception; probably invalid "
                    "item parameter" );
        throw TOSDB_data_block_error(e, "remove_item() probably invalid item");    
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "remove_item()");
    }    
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::add_topic( TOS_Topics::TOPICS tTopic) 
{    
    try{
        _guardTy _lock_(*_mtxR);

        if( _topicEnums.insert( tTopic ).second ) 
            for (auto & i : _rawBlock)
                insertTopic( i.second.get(), tTopic );         
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "add_topic()");
    }
}

template< typename TG, typename TD > 
void RawDataBlock<TG,TD>::remove_topic( TOS_Topics::TOPICS tTopic)
{    
    try{
        _guardTy _lock_(*_mtxR);

        if( _topicEnums.erase( tTopic ) ) 
            for(auto & rBpair : _rawBlock) {
                _tRowTy *tBlck = rBpair.second.get();

                if( tBlck ){
                    (tBlck->at(tTopic)).reset();
                    tBlck->erase(tTopic);
                }
            }                
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "remove_topic(): out_of_range exception; probably "
                    "invalid topic parameter" );
        throw TOSDB_data_block_error( e, 
                                      "remove_item() probably invalid topic ");
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "remove_topic()");        
    }    
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::_tRowTy* 
RawDataBlock<TG,TD>::insertTopic( typename RawDataBlock<TG,TD>::_tRowTy* tBlock, 
                                  TOS_Topics::TOPICS topic)
{        
    _iStreamTy *dataBlock;

    switch( TOS_Topics::TypeBits(topic) ){ 
    case TOSDB_STRING_BIT :
        dataBlock = this->_dtFlag 
            ? new tosdb_data_stream::Object< std::string, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _blockSize ) 
            : new tosdb_data_stream::Object< std::string, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _blockSize );
        break;
    case TOSDB_INTGR_BIT :
        dataBlock = this->_dtFlag 
            ? new tosdb_data_stream::Object< def_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _blockSize ) 
            : new tosdb_data_stream::Object< def_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _blockSize );
        break;
    case TOSDB_QUAD_BIT :
        dataBlock = this->_dtFlag 
            ? new tosdb_data_stream::Object< ext_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _blockSize ) 
            : new tosdb_data_stream::Object< ext_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _blockSize );
        break;
    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :
        dataBlock = this->_dtFlag 
            ? new tosdb_data_stream::Object< ext_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _blockSize )
            : new tosdb_data_stream::Object< ext_size_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _blockSize );
        break;
    default :
        dataBlock = this->_dtFlag 
            ? new tosdb_data_stream::Object< def_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             true > ( _blockSize ) 
            : new tosdb_data_stream::Object< def_price_type, 
                                             datetime_type, 
                                             generic_type, 
                                             false > ( _blockSize );
    }
    tBlock->erase(topic);

    try{
        tBlock->insert( _tRowElemTy( topic, 
                                     _iStreamPtrTy( std::move(dataBlock) )));
    }catch( ... ){
        TOSDB_LogH("DataBlock","insertTopic(): Problem inserting t-block");
        if(dataBlock)
            delete dataBlock;
    }

    return tBlock;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::_tRowPtrTy 
RawDataBlock<TG,TD>::populateTBlock(_tRowPtrTy tBlock)
{        
    for(auto e : _topicEnums)
        insertTopic( tBlock.get(), e );

    return std::move(tBlock);
}

template< typename TG, typename TD > 
const typename RawDataBlock<TG,TD>::_iStreamTy*
RawDataBlock<TG,TD>::raw_stream_ptr( std::string sItem, 
                                     TOS_Topics::TOPICS tTopic ) const 
{
    const _iStreamTy * tmpFrame = nullptr;

    try{
        _guardTy _lock_(*_mtxR);

        _tRowTy* tBlck = _rawBlock.at(sItem).get();
        if( tBlck )                                        
            tmpFrame = (tBlck->at(tTopic)).get();
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "raw_stream_ptr(): out_of_range exception; probably " 
                    "invalid item and/or topic" );
        throw TOSDB_data_block_error( e, 
                                      "DataStream does not exist in block; "
                                      "probably invalid item and/or topic");
    }catch(const std::exception & e){
        throw TOSDB_data_block_error(e, "raw_stream_ptr()");
    }

    if( !tmpFrame )
        throw TOSDB_data_block_error("DataStream does not exist in block");    

    return tmpFrame;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_type
RawDataBlock<TG,TD>::map_of_frame_topics( std::string sItem ) const 
{
    map_type map;

    try{
        _guardTy _lock_(*_mtxR);

        _tRowTy * tBlck = _rawBlock.at(sItem).get();        
        if ( tBlck )
            for( auto & i : *tBlck)
                map.insert( 
                    std::move( pair_type( TOS_Topics::globalTopicMap[i.first], 
                                          i.second->operator[](0) )));
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "map_of_frame_topics(): out_of_range exception; "
                    "probably invalid item param" );
        throw TOSDB_data_block_error( e, 
                                      "map_of_frame_topics(); probably "
                                      "invalid item param" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "RawDataBlock::map_of_frame_topics()" );
    }catch(const std::exception & e){
        throw TOSDB_data_block_error(e, "map_of_frame_topics()");
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_type
RawDataBlock<TG,TD>::map_of_frame_items(TOS_Topics::TOPICS tTopic) const 
{
    map_type map; 

    try{
        _guardTy _lock_(*_mtxR);

        for( auto & i : _rawBlock)
            map.insert( 
                std::move( pair_type( i.first, 
                                      i.second->at(tTopic)->operator[](0) )));             
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "map_of_frame_items(): out_of_range exception; "
                    "probably invalid topic param" );
        throw TOSDB_data_block_error( e, 
                                      "map_of_frame_items(); "
                                      "probably invalid topic" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "RawDataBlock::map_of_frame_items()" );
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "map_of_frame_items()");
    }

    return map;
}


template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_datetime_type
RawDataBlock<TG,TD>::pair_map_of_frame_topics( std::string sItem ) const 
{
    map_datetime_type map;

    try{
        _guardTy _lock_(*_mtxR);

        _tRowTy * tBlck = _rawBlock.at(sItem).get();        
        if ( tBlck )
            for( auto & i : *tBlck)
                map.insert( std::move( map_datetime_type::value_type(
                                           TOS_Topics::globalTopicMap[i.first], 
                                           i.second->both(0) 
                                           )));
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "pair_map_of_frame_topics(): out_of_range exception; "
                    "probably invalid item param" );
        throw TOSDB_data_block_error( e, 
                                      "pair_map_of_frame_topics(); "
                                      "probably invalid item" );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in RawDataBlock::"
                                       "pair_map_of_frame_topics()" );
    }catch(const std::exception & e){
        throw TOSDB_data_block_error( e, "pair_map_of_frame_topics()" );
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::map_datetime_type
RawDataBlock<TG,TD>::pair_map_of_frame_items(TOS_Topics::TOPICS tTopic) const 
{
    map_datetime_type map; 

    try{
        _guardTy _lock_(*_mtxR);

        for( auto & i : _rawBlock)
            map.insert( std::move( map_datetime_type::value_type( 
                                       i.first, 
                                       i.second->at(tTopic)->both(0) 
                                       )));             
    }catch( const std::out_of_range& e ){
        TOSDB_LogH( "DataBlock", 
                    "pair_map_of_frame_items(): out_of_range exception; "
                    "probably invalid topic param" );
        throw TOSDB_data_block_error( e, 
                                      "pair_map_of_frame_items(); "
                                      "probably invalid topic");
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in RawDataBlock::"
                                       "pair_map_of_frame_items(...)" );
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "pair_map_of_frame_items()");
    }

    return map;
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::matrix_type
RawDataBlock<TG,TD>::matrix_of_frame() const 
{
    matrix_type matrix;

    try{
        _guardTy _lock_(*_mtxR);

        for(auto & items : _rawBlock){            
            map_type map; 
            for(auto & tops : *items.second)
                map.insert( 
                    std::move( 
                        map_type::value_type( 
                           std::move( TOS_Topics::globalTopicMap[tops.first] ),
                           tops.second->operator[](0) 
                           ))); 

            matrix.insert( std::move( matrix_type::value_type( items.first, 
                                                               std::move(map) 
                                                               )));
        }        
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in RawDataBlock::"
                                       "matrix_of_frame(...)" );
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "matrix_of_frame()");
    }

    return matrix; 
}

template< typename TG, typename TD > 
typename RawDataBlock<TG,TD>::matrix_datetime_type
RawDataBlock<TG,TD>::pair_matrix_of_frame() const 
{ 
    matrix_datetime_type matrix;

    try{
        _guardTy _lock_(*_mtxR);

        for(auto & items : _rawBlock){            
            map_datetime_type map; 
            for(auto & tops : *items.second)
                map.insert( 
                    std::move( 
                        map_datetime_type::value_type( 
                            std::move( TOS_Topics::globalTopicMap[tops.first] ),
                            std::move( tops.second->both(0) )
                            ))); 

            matrix.insert( std::move( 
                               matrix_datetime_type::value_type( items.first, 
                                                                 std::move(map) 
                                                                 )));
        }        
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_data_stream_error( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in RawDataBlock::"
                                       "pair_matrix_of_frame(...)" );
    }catch( const std::exception & e ){
        throw TOSDB_data_block_error(e, "matrix_of_frame()");
    }

    return matrix; 
}

#endif
