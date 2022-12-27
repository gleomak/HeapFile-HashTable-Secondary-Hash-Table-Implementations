#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }


int HT_CreateFile(char *fileName,  int buckets){
    int fd1;
    BF_Block *htBlock;
    BF_Block_Init(&htBlock);
    CALL_OR_DIE(BF_CreateFile(fileName))
    CALL_OR_DIE(BF_OpenFile(fileName, &fd1));
    CALL_OR_DIE(BF_AllocateBlock(fd1, htBlock));  // Δημιουργία καινούριου block
    void* data = BF_Block_GetData(htBlock);

    printf("Size of HT info is : %lu\n", sizeof(HT_info));
    HT_info* htInfo = malloc(sizeof(HT_info));
    htInfo->fileDesc = fd1;
    htInfo->offset = 512 - sizeof(HT_block_info);
    htInfo->isHP = 0;
    htInfo->numOfBuckets = buckets;
    htInfo->bucketToLastBlock = (int*) malloc(buckets * sizeof(int));
    htInfo->firstBlock = NULL;
    void* data2;
    BF_Block* htBlock2;
    BF_Block_Init(&htBlock2);
    for(int i = 0 ; i < buckets ; i++){
        CALL_OR_DIE(BF_AllocateBlock(fd1, htBlock2));  // Δημιουργία καινούριου block
        data2 = BF_Block_GetData(htBlock2);
        HT_block_info* htBlockInfo = malloc(sizeof(HT_block_info));
        htBlockInfo->blockNumber = i + 1;
        htBlockInfo->numOfRecords = 0;
        htBlockInfo->maxRecords = (512 - sizeof(HT_block_info)) / sizeof(Record);
        htBlockInfo->previousBlockNumber = -1;
        memcpy(data2 + htInfo->offset , htBlockInfo , sizeof(HT_block_info));
        htInfo->bucketToLastBlock[i] = i + 1;
//        printf("bucket has last block number : %d\n", htInfo->bucketToLastBlock[i]);
        free(htBlockInfo);
        BF_Block_SetDirty(htBlock2);
        CALL_OR_DIE(BF_UnpinBlock(htBlock2));
    }
    memcpy(data , htInfo , sizeof(HT_info));
//    free(htInfo->bucketToLastBlock);
    free(htInfo);
    void* d = BF_Block_GetData(htBlock);
    HT_info* debug = d;
    for(int i = 0 ; i < 10 ; i++){
        printf("Last block is : %d\n" , debug->bucketToLastBlock[i]);
    }

    BF_Block_SetDirty(htBlock);
    CALL_OR_DIE(BF_UnpinBlock(htBlock));

    BF_Block_Destroy(&htBlock);
    BF_Block_Destroy(&htBlock2);

    CALL_OR_DIE(BF_CloseFile(fd1));

    return 0;
}

HT_info* HT_OpenFile(char *fileName){
    HT_info* htInfo;
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_OpenFile(fileName, &fd1));
    void* data;
    CALL_OR_DIE(BF_GetBlock(fd1, 0, block));
    data = BF_Block_GetData(block);
    htInfo = data;
    htInfo->firstBlock = block;
    if(htInfo->isHP != 0){
        printf("This is not a HT file\n");
        return NULL;
    }
    return htInfo;
}


int HT_CloseFile( HT_info* HT_info ){
    printEntries(HT_info);
    free(HT_info->bucketToLastBlock);
    BF_Block_SetDirty(HT_info->firstBlock);
    CALL_OR_DIE(BF_UnpinBlock(HT_info->firstBlock));
    BF_Block_Destroy(&HT_info->firstBlock);
    CALL_OR_DIE(BF_CloseFile(HT_info->fileDesc));
    return 0;
}

int HT_InsertEntry(HT_info* ht_info, Record record){
    int hashNumber = record.id % ht_info->numOfBuckets;
    int blockNumber = ht_info->bucketToLastBlock[hashNumber];
    int insertedInBlock;
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc , blockNumber , block));
    void* data = BF_Block_GetData(block);
    HT_block_info* blockInfo = data + ht_info->offset;

    if(blockInfo->numOfRecords == blockInfo->maxRecords){
        BF_Block* newBlock;
        BF_Block_Init(&newBlock);
        CALL_OR_DIE(BF_AllocateBlock(ht_info->fileDesc, newBlock));  // Δημιουργία καινούριου block
        void* newData = BF_Block_GetData(newBlock);
        int blocks_num;
        CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc , &blocks_num));

        HT_block_info* newBlockInfo = malloc(sizeof(HT_block_info));
        newBlockInfo->blockNumber = blocks_num - 1 ;
        newBlockInfo->numOfRecords = 1;
        newBlockInfo->maxRecords = (512 - sizeof(HT_block_info)) / sizeof(Record);
        newBlockInfo->previousBlockNumber = blockInfo->blockNumber;
        memcpy(newData + ht_info->offset , newBlockInfo , sizeof(HT_block_info));
        free(newBlockInfo);

        Record* rec = newData;
        rec[0] = record;
        insertedInBlock = blocks_num - 1;

//        printf("Before is hashnumber : %d , has lastBlock : %d\n" , hashNumber , ht_info->bucketToLastBlock[hashNumber]);
        ht_info->bucketToLastBlock[hashNumber] = blocks_num - 1;
//        printf("After is hashnumber : %d , has lastBlock : %d\n" , hashNumber , ht_info->bucketToLastBlock[hashNumber]);

        BF_Block_SetDirty(newBlock);
        CALL_OR_DIE(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);
    }else{
        Record* rec = data;
        rec[blockInfo->numOfRecords] = record;
        blockInfo->numOfRecords += 1;
        insertedInBlock  = blockInfo->blockNumber;
    }
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return insertedInBlock;
}

void printEntries(HT_info* htInfo){
    BF_Block* block;
    BF_Block_Init(&block);
    void* data;
    for(int i = 0 ; i < htInfo->numOfBuckets ; i++){
        printf("Bucket number %d with last bucket number %d has these blocks and records \n",i,htInfo->bucketToLastBlock[i]);
        int lastBlock = htInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc , lastBlock , block));
        data = BF_Block_GetData(block);
        Record* rec = data;
        HT_block_info* htBlockInfo = data + htInfo->offset;
        while(1){
            for(int j = 0 ; j < htBlockInfo->numOfRecords ; j++){
                printf("\tRecord number %d of Block %d is ",j , htBlockInfo->blockNumber);
                printRecord(rec[j]);
            }
            CALL_OR_DIE(BF_UnpinBlock(block));
            if(htBlockInfo->previousBlockNumber == -1)
                break;
            CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc , htBlockInfo->previousBlockNumber , block));
            data = BF_Block_GetData(block);
            htBlockInfo = data + htInfo->offset;
            rec = data;
        }
    }
    BF_Block_Destroy(&block);
}

int HT_GetAllEntries(HT_info* ht_info, int value ){
    BF_Block* block;
    BF_Block_Init(&block);
    void* data;
    int hashNumber = value % ht_info->numOfBuckets;
    int lastBlock = ht_info->bucketToLastBlock[hashNumber];
    CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc , lastBlock , block));
    data = BF_Block_GetData(block);
    Record* rec = data;
    HT_block_info* htBlockInfo = data + ht_info->offset;
    int blocksRead = 0;
    while(1){
        blocksRead++;
        for(int i = 0 ; i < htBlockInfo->numOfRecords ; i++){
            if(rec[i].id == value){
                printf("Found value: %d,in block: %d, of hash %d\n",value,htBlockInfo->blockNumber , hashNumber);
                printRecord(rec[i]);
                CALL_OR_DIE(BF_UnpinBlock(block));
                BF_Block_Destroy(&block);
                return blocksRead;
            }
        }
        CALL_OR_DIE(BF_UnpinBlock(block));
        if(htBlockInfo->previousBlockNumber == -1){
            break;
        }
        CALL_OR_DIE(BF_GetBlock(ht_info->fileDesc , htBlockInfo->previousBlockNumber , block));
        data = BF_Block_GetData(block);
        htBlockInfo = data + ht_info->offset;
        rec = data;
    }
    BF_Block_Destroy(&block);
    return -1;
}




