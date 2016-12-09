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

#include "tos_databridge.h"
#include "initializer_chain.hpp"

namespace{
  
/* since TwoWayHashMap doesn't extend the full iterator interface we 
   need to pass in our own 'inserter' object that overloads assignment
   
   (woule be better to get rid alot of the garbage in TwoWayHashMap and
    just extend the iterator interface to it) */

template<typename T>
class TopicInserter {
  /* be sure we have a reference to the object or we just insert to a temp copy */
  T& _t; 
public:  
   /* can safely ignore the 'where' iter, its a map */
  TopicInserter(T& t, typename T::iterator1_type i) 
      : 
          _t(t)            
      {
      }
 
  TopicInserter&
  operator=(typename T::pair1_type p)
  {
      _t.insert(p);
      return *this;
  }
};

};

template<> const TOS_Topics::topic_map_type TOS_Topics::map = 
    InitializerChain< TOS_Topics::topic_map_type,                                          
                      NoTransform,
                      TopicInserter >
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::NULL_TOPIC, " ") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::HIGH52, "52HIGH") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LOW52, "52LOW") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::ASK, "ASK") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::ASKX, "ASKX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::ASK_SIZE, "ASK_SIZE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::AX, "AX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BACK_EX_MOVE, "BACK_EX_MOVE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BACK_VOL, "BACK_VOL") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BA_SIZE, "BA_SIZE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BETA, "BETA") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BID, "BID") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BIDX, "BIDX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BID_SIZE, "BID_SIZE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::BX, "BX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::CALL_VOLUME_INDEX, "CALL_VOLUME_INDEX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::CLOSE, "CLOSE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::DELTA, "DELTA") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::DESCRIPTION, "DESCRIPTION") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::DIV, "DIV") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::DIV_FREQ, "DIV_FREQ") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::DT, "DT") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EPS, "EPS") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EXCHANGE, "EXCHANGE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EXPIRATION, "EXPIRATION") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EXTRINSIC, "EXTRINSIC") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EX_DIV_DATE, "EX_DIV_DATE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::EX_MOVE_DIFF, "EX_MOVE_DIFF") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::FRONT_EX_MOVE, "FRONT_EX_MOVE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::FRONT_VOL, "FRONT_VOL") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::FX_PAIR, "FX_PAIR") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::GAMMA, "GAMMA") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::HIGH, "HIGH") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::HTB_ETB, "HTB_ETB") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::IMPL_VOL, "IMPL_VOL") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::INTRINSIC, "INTRINSIC") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LAST, "LAST") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LASTX, "LASTX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LAST_SIZE, "LAST_SIZE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LOW, "LOW") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::LX, "LX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MARK, "MARK") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MARKET_CAP, "MARKET_CAP") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MARK_CHANGE, "MARK_CHANGE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MARK_PERCENT_CHANGE, "MARK_PERCENT_CHANGE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MRKT_MKR_MOVE, "MRKT_MKR_MOVE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::MT_NEWS, "MT_NEWS") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::NET_CHANGE, "NET_CHANGE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::OPEN, "OPEN") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::OPEN_INT, "OPEN_INT") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::OPTION_VOLUME_INDEX, "OPTION_VOLUME_INDEX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::PE, "PE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::PERCENT_CHANGE, "PERCENT_CHANGE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::PUT_CALL_RATIO, "PUT_CALL_RATIO") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::PUT_VOLUME_INDEX, "PUT_VOLUME_INDEX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::RHO, "RHO") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::SHARES, "SHARES") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::STRENGTH_METER, "STRENGTH_METER") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::STRIKE, "STRIKE") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::SYMBOL, "SYMBOL") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::THETA, "THETA") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::VEGA, "VEGA") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::VOLUME, "VOLUME") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::VOL_DIFF, "VOL_DIFF") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::VOL_INDEX, "VOL_INDEX") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::WEIGHTED_BACK_VOL, "WEIGHTED_BACK_VOL") )
        (TOS_Topics::topic_map_entry_type(TOS_Topics::TOPICS::YIELD, "YIELD") );


