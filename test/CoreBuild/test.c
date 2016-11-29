#include <stdio.h>
#include "tos_databridge.h"

void StaticAdminTests();
int DynamicAdminTests();
void GetTests();
void StreamSnapshotTests();
void FromMarkerTests();
void FrameTests();
void CloseTests();

const char* block1_id = "test_block_1";
static size_type block1_sz = 1000;
static int block1_use_dt = 1;
static size_type block1_timeout = 3000;

int 
main(int argc, char* argv[])
{
    printf("\n*** BEGIN %s BEGIN ***\n\n", argv[0]);

    StaticAdminTests();

    Sleep(500);
    if( DynamicAdminTests() )
        return 1;

    Sleep(500);
    GetTests();

    Sleep(500);
    StreamSnapshotTests();

    Sleep(500);
    CloseTests();

    printf("\n*** END %s END ***\n\n", argv[0]);
    return 0;
}

void
StaticAdminTests()
{  
    printf("+ TOSDB_GetBlockLimit() :: %u \n",   TOSDB_GetBlockLimit());
    printf("+ TOSDB_GetBlockCount() :: %u \n",   TOSDB_GetBlockCount());
    printf("+ TOSDB_GetLatency() :: %u \n",   TOSDB_GetLatency());

#ifdef __cplusplus
    printf("+ TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST) :: %u \n", 
           TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST));
    printf("+ TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST) :: %s \n", 
           TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST).c_str());
#endif
}

int
DynamicAdminTests()
{
    int ret;
    size_type i, tcount, icount;
    char** buf1;

    ret = TOSDB_Connect();
    printf("+ TOSDB_Connect() :: %i \n", ret);
    if(ret){
        printf("- Couldn't Connect \n");
        return 1;
    }

    printf("+ TOSDB_IsConnected() :: %i \n",   TOSDB_IsConnected());

    ret = TOSDB_CreateBlock(block1_id,block1_sz,block1_use_dt,block1_timeout);
    printf("+ TOSDB_CreateBlock() :: %i \n",  ret);
    printf("    block ID       : %s \n", block1_id);
    printf("    block size     : %Iu \n", block1_sz);
    printf("    block datetime : %d \n", block1_use_dt);
    printf("    block timeout  : %Iu \n", block1_timeout); 

    TOSDB_SetBlockSize(block1_id, block1_sz*2);
    printf("+ TOSDB_SetBlockSize(): %Iu \n", block1_sz*2);    
    TOSDB_GetBlockSize(block1_id, &block1_sz);
    printf("+ TOSDB_GetBlockSize(): %Iu \n", block1_sz );
    
    printf("+ TOSDB_AddTopic(): %s, %d \n", "LAST", TOSDB_AddTopic(block1_id, "LAST") );
    printf("+ TOSDB_AddTopic(): %s, %d \n", "VOLUME", TOSDB_AddTopic(block1_id, "VOLUME") );
    printf("+ TOSDB_AddItem(): %s, %d \n", "SPY", TOSDB_AddItem(block1_id, "SPY") );
    printf("+ TOSDB_AddItem(): %s, %d \n", "QQQ", TOSDB_AddItem(block1_id, "QQQ") );
    printf("+ TOSDB_AddTopic(): %s, %d \n", "LASTX", TOSDB_AddTopic(block1_id, "LASTX") );
    printf("+ TOSDB_AddTopic(): %s, %d \n", "BIDX", TOSDB_AddTopic(block1_id, "BIDX") );
    printf("+ TOSDB_RemoveTopic(): %s, %d \n", "BIDX", TOSDB_RemoveTopic(block1_id, "BIDX") );

    tcount = 0;
    TOSDB_GetTopicCount(block1_id, &tcount);
    printf("+ TOSDB_TopicCount(): %d \n", tcount);

    buf1 = NewStrings(tcount, 100);
    TOSDB_GetTopicNames(block1_id, buf1, tcount, 100);
    printf("+ TOSDB_GetTopicNames() :: ");
    for(i = 0; i < tcount; ++i)
      printf("%s ", buf1[i]);
    printf("\n");
    DeleteStrings(buf1,  tcount);

    icount = 0;
    TOSDB_GetItemCount(block1_id, &icount);
    printf("+ TOSDB_ItemCount(): %d \n", icount);

    buf1 = NewStrings(icount, 100);
    TOSDB_GetItemNames(block1_id, buf1, icount, 100);
    printf("+ TOSDB_GetItemNames() :: ");
    for(i = 0; i < icount; ++i)
      printf("%s ", buf1[i]);
    printf("\n");
    DeleteStrings(buf1, icount);

    return 0;
}


void 
GetTests()
{
   DateTimeStamp dts;
   double d1 = .0;
   long long ll1 = 0;

   TOSDB_GetDouble(block1_id,"SPY","LAST",0,&d1,NULL);
   printf("+ TOSDB_GetDouble(): %s, %s, %d, %f \n", "SPY", "LAST", 0, d1);

   TOSDB_GetLongLong(block1_id,"QQQ","VOLUME",0,&ll1,&dts);
   printf("+ TOSDB_GetLongLong(): %s, %s, %d, %lld, %d:%d:%d \n", "QQQ", "VOLUME", 0, ll1,
           dts.ctime_struct.tm_hour, dts.ctime_struct.tm_min, dts.ctime_struct.tm_sec);

#ifdef __cplusplus
   ext_price_type p = TOSDB_Get<ext_price_type,false>(block1_id,"SPY",TOS_Topics::TOPICS::LAST, 0);
   printf("+ TOSDB_Get<ext_price_type,false>(): %s, %s, %d, %f \n", "SPY", "LAST", 0, p);

   std::pair<ext_size_type, DateTimeStamp> sp = 
       TOSDB_Get<ext_size_type,true>(block1_id,"QQQ", TOS_Topics::TOPICS::VOLUME, 0);
   printf("+ TOSDB_Get<ext_size_type,true>(): %s, %s, %d, %lld, %d:%d:%d \n", "QQQ", "VOLUME", 
          0, sp.first, sp.second.ctime_struct.tm_hour, sp.second.ctime_struct.tm_min, 
          sp.second.ctime_struct.tm_sec);

   auto g = TOSDB_Get<generic_type,false>(block1_id,"SPY",TOS_Topics::TOPICS::LAST, 0);
   printf("+ TOSDB_Get<generic_type,false>(): %s, %s, %d, %f \n", "SPY", "LAST", 0, g.as_double());
   std::cout << "  Check Generic Type: \n";
   std::cout << "    size: " << g.size() << std::endl;
   std::cout << "    is_float: " << std::boolalpha<< g.is_float() << std::endl;
   std::cout << "    is_double: " << std::boolalpha<< g.is_double() << std::endl;
   std::cout << "    is_long: " << std::boolalpha<< g.is_long() << std::endl;
   std::cout << "    is_long_long: " << std::boolalpha << g.is_long_long() << std::endl;
   std::cout << "    is_string: " << std::boolalpha << g.is_string() << std::endl;
   std::cout << "    is_floating_point: " << std::boolalpha<< g.is_floating_point() << std::endl;
   std::cout << "    is_integer: " << std::boolalpha<< g.is_integer() << std::endl;
   std::cout << "    as_float: " << g.as_float() << std::endl;
   std::cout << "    as_double: " << g.as_double() << std::endl;
   std::cout << "    as_long: " << g.as_long() << std::endl;
   std::cout << "    as_long_long: " << g.as_long_long() << std::endl;
   std::cout << "    as_string: " << g.as_string() << std::endl;
 
#endif

}


void
StreamSnapshotTests()
{
   
#ifdef __cplusplus
   auto gvec = TOSDB_GetStreamSnapshot<generic_type,true>(block1_id,"SPY",TOS_Topics::TOPICS::LAST);
   printf("+ TOSDB_GetStreamSnapshot<generic_type,true>(): %s, %s \n", "SPY", "LAST");
   std::cout<< "   ";
   for(auto& i : gvec.first)
       std::cout<< i.as_string() << ' ';
   std::cout<<std::endl;
#endif
}

/*
void 
FromMarkerTests()
{

}

void
FrameTests()
{

}
*/

void
CloseTests()
{
    printf("+ TOSDB_IsConnected() :: %i \n",   TOSDB_IsConnected());
    printf("+ TOSDB_CloseBlocks() :: %i \n",   TOSDB_CloseBlocks());
    printf("+ TOSDB_Disconnect() :: %i \n",   TOSDB_Disconnect());    
    printf("+ TOSDB_IsConnected() :: %i \n",   TOSDB_IsConnected());
}




