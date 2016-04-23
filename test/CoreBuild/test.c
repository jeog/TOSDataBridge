#include <stdio.h>
#include "tos_databridge.h"

void TestConnected();
void StaticTests();

int 
main(int argc, char* argv[])
{
    printf("\n*** BEGIN %s BEGIN ***\n", argv[0]);
    StaticTests();
    printf("*** END %s END ***\n\n", argv[0]);
    return 0;
}

void
TestConnected()
{
    const char* bid = "test_block_1";
    size_type sz = 1000;
    int use_dt = 1;
    size_type to = 3000;
    DateTimeStamp dts;
    double d1 = .0;
    long long ll1 = 0;

    printf("+ TOSDB_IsConnected() :: %i \n",   TOSDB_IsConnected());
    printf("+ TOSDB_CreateBlock() :: %zu, %i \n",   TOSDB_CreateBlock(bid,sz,use_dt,to));
    printf("+    block ID       : %s \n", bid);
    printf("+    block size     : %zu \n", sz);
    printf("+    block datetime : %d \n", use_dt);
    printf("+    block timeout  : %zu \n", to);    
    TOSDB_SetBlockSize(bid, sz*2);
    printf("+ TOSDB_SetBlockSize(): %zu \n", sz*2);    
    TOSDB_GetBlockSize(bid, &sz);
    printf("+ TOSDB_GetBlockSize(): %zu \n", sz );
    
    printf("+ TOSDB_AddTopic(): %s, %d \n", "LAST", TOSDB_AddTopic(bid, "LAST") );
    printf("+ TOSDB_AddTopic(): %s, %d \n", "VOLUME", TOSDB_AddTopic(bid, "VOLUME") );
    printf("+ TOSDB_AddItem(): %s, %d \n", "SPY", TOSDB_AddItem(bid, "SPY") );
    printf("+ TOSDB_AddItem(): %s, %d \n", "QQQ", TOSDB_AddItem(bid, "QQQ") );
    
    char** buf1 = NewStrings(3, 100);
    TOSDB_GetTopicNames(bid, buf1, 3, 100);
    printf("+ TOSDB_GetTopicNames() :: %s, %s, %s \n", buf1[0], buf1[1], buf1[2]);
    DeleteStrings(buf1, 3);

    Sleep(500);
    
    TOSDB_GetDouble(bid,"SPY","LAST",0,&d1,NULL);
    printf(" TOSD_GetDouble(): %s, %s, %d, %f, no datetime \n", "SPY", "LAST", 0, d1);
    TOSDB_GetLongLong(bid,"QQQ","VOLUME",0,&ll1,&dts);
    printf(" TOSD_GetDouble(): %s, %s, %d, %lld, %d:%d:%d \n", "QQQ", "VOLUME", 0, ll1,
           dts.ctime_struct.tm_hour, dts.ctime_struct.tm_min, dts.ctime_struct.tm_sec);
}


void 
StaticTests()
{
    int ret; 

    printf("+ TOSDB_GetBlockLimit() :: %u \n",   TOSDB_GetBlockLimit());
    printf("+ TOSDB_GetBlockCount() :: %u \n",   TOSDB_GetBlockCount());
    printf("+ TOSDB_GetLatency() :: %u \n",   TOSDB_GetLatency());

#ifdef __cplusplus

    printf("+ TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST) :: %u \n", 
           TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST));

    printf("+ TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST) :: %s \n", 
           TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST).c_str());

#endif

    ret = TOSDB_Connect();
    printf("+ TOSDB_Connect() :: %i \n", ret);
    if(ret == 0)
        TestConnected();

    printf("+ TOSDB_Disconnect() :: %i \n",   TOSDB_Disconnect());
    printf("+ TOSDB_CloseBlocks() :: %i \n",   TOSDB_CloseBlocks());
}





