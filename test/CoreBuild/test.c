#include <stdio.h>
#include "tos_databridge.h"

void TOSDB_StaticTests()
{
  printf("+ TOSDB_Connect() :: ");
  printf("%i \n", TOSDB_Connect());

  printf("+ TOSDB_Disconnect() :: ");
  printf("%i \n",   TOSDB_Disconnect());

  printf("+ TOSDB_CloseBlocks() :: ");
  printf("%i \n",   TOSDB_CloseBlocks());

  printf("+ TOSDB_GetBlockLimit() :: ");
  printf("%u \n",   TOSDB_GetBlockLimit());

  printf("+ TOSDB_GetBlockCount() :: ");
  printf("%u \n",   TOSDB_GetBlockCount());

  printf("+ TOSDB_GetLatency() :: ");
  printf("%u \n",   TOSDB_GetLatency());

#ifdef __cplusplus

  printf("+ TOSDB_GetBlockIDs() :: ");
  if(TOSDB_GetBlockIDs().empty())
    printf("EMPTY \n");

  printf("+ TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST) :: ");
  printf("%u \n", TOSDB_GetTypeBits(TOS_Topics::TOPICS::LAST));

  printf("+ TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST) :: ");
  printf("%s \n", TOSDB_GetTypeString(TOS_Topics::TOPICS::LAST).c_str());

#endif
}


int main(int argc, char* argv[])
{
  printf("\n*** BEGIN %s BEGIN ***\n", argv[0]);
  TOSDB_StaticTests();
  printf("*** END %s END ***\n\n", argv[0]);
  return 0;
}


