#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "sht_table.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return -1;              \
    }                         \
  }

#define CALL_OR_DIE2(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return NULL;            \
    }                         \
  }


int SHT_CreateSecondaryIndex(char *sfileName, int buckets, char *fileName) {
    /* Init of the sht file */
    int fd1;
    BF_Block *shtBlock;
    BF_Block_Init(&shtBlock);
    CALL_OR_DIE(BF_CreateFile(sfileName))
    CALL_OR_DIE(BF_OpenFile(sfileName, &fd1));
    CALL_OR_DIE(BF_AllocateBlock(fd1, shtBlock));  // Δημιουργία καινούριου block
    void *data = BF_Block_GetData(shtBlock);

    /* We initialize the shtInfo struct that will be saved in the first block of the sht file */
    SHT_info *shtInfo = malloc(sizeof(SHT_info));
    shtInfo->fileType = "SHT";
    shtInfo->fileDesc = fd1;
    shtInfo->offset = 512 - sizeof(SHT_block_info);
    shtInfo->numOfBuckets = buckets;
    shtInfo->firstBlock = NULL;
    void *data2;
    BF_Block *shtBlock2;
    BF_Block_Init(&shtBlock2);

    /*We allocate and initialize a block for each bucket*/
    for (int i = 0; i < buckets; i++) {
        CALL_OR_DIE(BF_AllocateBlock(fd1, shtBlock2));
        data2 = BF_Block_GetData(shtBlock2);
        SHT_block_info *shtBlockInfo = malloc(sizeof(SHT_block_info));
        shtBlockInfo->blockNumber = i + 1;
        shtBlockInfo->previousBlockNumber = -1;
        shtBlockInfo->numOfRecords = 0;
        shtBlockInfo->maxRecords = (512 - sizeof(SHT_block_info)) / sizeof(shtTuple);
        memcpy(data2 + shtInfo->offset, shtBlockInfo, sizeof(SHT_block_info));

        shtInfo->bucketToLastBlock[i] = i + 1; // each bucket points to the block created above
        free(shtBlockInfo);
        BF_Block_SetDirty(shtBlock2);
        CALL_OR_DIE(BF_UnpinBlock(shtBlock2));
    }
    memcpy(data, shtInfo, sizeof(SHT_info));
    free(shtInfo);

    BF_Block_SetDirty(shtBlock);
    CALL_OR_DIE(BF_UnpinBlock(shtBlock));

    /*Destroying the blocks we used after setting them Dirty/Unpinning them*/
    BF_Block_Destroy(&shtBlock);
    BF_Block_Destroy(&shtBlock2);

    /*Closing the file after the creation*/
    CALL_OR_DIE(BF_CloseFile(fd1));

    return 0;

}

SHT_info *SHT_OpenSecondaryIndex(char *indexName) {
    /*Re-opening the file*/
    SHT_info *shtInfo;
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE2(BF_OpenFile(indexName, &fd1));

    /*Get and pin the first block and update the fileDesc/pointer-to-first-block values*/
    CALL_OR_DIE2(BF_GetBlock(fd1, 0, block));
    void *data = BF_Block_GetData(block);
    shtInfo = data;
    shtInfo->fileDesc = fd1;
    shtInfo->firstBlock = block;
    if(strcmp(shtInfo->fileType , "SHT") != 0) {
        printf("This is not a SHT file !\n");
        return NULL;
    }

    return shtInfo;
}


int SHT_CloseSecondaryIndex(SHT_info *SHT_info) {
//    printSHTEntries(SHT_info);
    if (SHT_info == NULL)
        return -1;

    /*Freeing the hash table , Setting dirty/unpinning the first block , closing the file*/
    BF_Block_SetDirty(SHT_info->firstBlock);
    CALL_OR_DIE(BF_UnpinBlock(SHT_info->firstBlock));
    BF_Block_Destroy(&SHT_info->firstBlock);
    CALL_OR_DIE(BF_CloseFile(SHT_info->fileDesc));
    return 0;
}

int SHT_SecondaryInsertEntry(SHT_info *sht_info, Record record, int block_id) {
    /*Hashing the name , finding the bucket and its last block to insert the entry */
    unsigned char *name = (unsigned char *) malloc(15);
    memcpy(name, record.name, strlen(record.name) + 1);
    int hashNumber = SHT_HashFunction(name, sht_info->numOfBuckets);
    free(name);
    int blockNumber = sht_info->bucketToLastBlock[hashNumber];

    /*Pinning the last block and its contents*/
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(sht_info->fileDesc, blockNumber, block));
    void *data = BF_Block_GetData(block);
    SHT_block_info *blockInfo = data + sht_info->offset;

    /*If the last block is full create another one*/
    if (blockInfo->numOfRecords == blockInfo->maxRecords) {
        BF_Block *newBlock;
        BF_Block_Init(&newBlock);
        CALL_OR_DIE(BF_AllocateBlock(sht_info->fileDesc, newBlock));
        void *newData = BF_Block_GetData(newBlock);
        int blocks_num;
        CALL_OR_DIE(BF_GetBlockCounter(sht_info->fileDesc, &blocks_num));

        SHT_block_info *newBlockInfo = malloc(sizeof(SHT_block_info));
        newBlockInfo->blockNumber = blocks_num - 1;
        newBlockInfo->numOfRecords = 1;
        newBlockInfo->maxRecords = (512 - sizeof(SHT_block_info)) / sizeof(shtTuple);
        newBlockInfo->previousBlockNumber = blockInfo->blockNumber;
        memcpy(newData + sht_info->offset, newBlockInfo, sizeof(SHT_block_info));
        free(newBlockInfo);

        shtTuple tuple;
        memcpy(tuple.strName, record.name, strlen(record.name) + 1);
        tuple.blockIndex = block_id;

        shtTuple *shtTuple1 = newData;
        shtTuple1[0] = tuple; //insertion of the tuple in the new Block

        sht_info->bucketToLastBlock[hashNumber] = blocks_num - 1; //updating the last block of the specific bucket

        BF_Block_SetDirty(newBlock);
        CALL_OR_DIE(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);

    } else { //If the block has space insert the tuple and update its contents
        shtTuple tuple;
        memcpy(tuple.strName, record.name, strlen(record.name) + 1);
        tuple.blockIndex = block_id;

        shtTuple *shtTuple1 = data;
        shtTuple1[blockInfo->numOfRecords] = tuple;
        blockInfo->numOfRecords += 1;
    }
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    return 0;
}

int SHT_SecondaryGetAllEntries(HT_info *ht_info, SHT_info *sht_info, char *name) {
    int readBlocks = 0; //Blocks that were read until we get all the entries with the specific name

    /*Hashing of the name , finding the last block of the specific bucket*/
    unsigned char *hashName = (unsigned char *) malloc(15);
    memcpy(hashName, name, strlen(name) + 1);
    int hashNumber = SHT_HashFunction(hashName, sht_info->numOfBuckets);
    free(hashName);
    int blockNumber = sht_info->bucketToLastBlock[hashNumber];

    BF_Block *block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_GetBlock(sht_info->fileDesc, blockNumber, block));
    void *data = BF_Block_GetData(block);
    shtTuple *shtTuple1 = data;
    SHT_block_info *shtBlockInfo = data + sht_info->offset;

    /*Setting an array that holds the visited blocks*/
    int numOfBlocks;
    CALL_OR_DIE(BF_GetBlockCounter(ht_info->fileDesc, &numOfBlocks));
    int *visitedArray = (int *) malloc(numOfBlocks * sizeof(int));
    memset(visitedArray, 0, numOfBlocks * sizeof(int)); //Setting all values with 0

    /*While that breaks when we iterate through all the blocks of the specific bucket */
    while (1) {
        readBlocks++;
        for (int i = 0; i < shtBlockInfo->numOfRecords; i++) {
            if (strcmp(name, shtTuple1[i].strName) == 0 && visitedArray[shtTuple1[i].blockIndex] == 0) {
                printf("Block %d in HT has records with the name : %s\n", shtTuple1[i].blockIndex, name);
                printSpecificHTEntries(ht_info, shtTuple1[i].blockIndex, name);
                visitedArray[shtTuple1[i].blockIndex] = 1; //Mark that this block is visited
            }
        }
        CALL_OR_DIE(BF_UnpinBlock(block));
        if (shtBlockInfo->previousBlockNumber == -1) //That means we reached the first Block of the bucket
            break;

        /*Getting and Pinning the predecessor of this block*/
        CALL_OR_DIE(BF_GetBlock(sht_info->fileDesc, shtBlockInfo->previousBlockNumber, block));
        data = BF_Block_GetData(block);
        shtBlockInfo = data + sht_info->offset;
        shtTuple1 = data;
    }
    BF_Block_Destroy(&block);
    free(visitedArray);
    return readBlocks;
}

/*Printing the Records with the specific name from the HT File block */
int printSpecificHTEntries(HT_info *htInfo, int index, char *name) {
    BF_Block *block2;
    BF_Block_Init(&block2);
    CALL_OR_DIE(BF_GetBlock(htInfo->fileDesc, index, block2));
    void *data2 = BF_Block_GetData(block2);
    HT_block_info *htBlockInfo = data2 + htInfo->offset;
    Record *rec = data2;

    for (int i = 0; i < htBlockInfo->numOfRecords; i++) {
        if (strcmp(name, rec[i].name) == 0) {
            printf("\tBlock number in HT : %d " , htBlockInfo->blockNumber);
            printRecord(rec[i]);
        }
    }
    CALL_OR_DIE(BF_UnpinBlock(block2));
    BF_Block_Destroy(&block2);
    return 0;
}

/*Auxiliary function that prints all the entries for every bucket and all their blocks*/
int printSHTEntries(SHT_info *shtInfo) {
    BF_Block *block;
    BF_Block_Init(&block);
    void *data;
    for (int i = 0; i < shtInfo->numOfBuckets; i++) {
        printf("Bucket number %d with last block number %d has these blocks and records\n", i,
               shtInfo->bucketToLastBlock[i]);
        int lastBlock = shtInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc, lastBlock, block));
        data = BF_Block_GetData(block);
        shtTuple *shtTuple1 = data;
        SHT_block_info *shtBlockInfo = data + shtInfo->offset;
        while (1) {
            printf(" Block number : %d from sht file with predecessor block %d has :\n", shtBlockInfo->blockNumber,
                   shtBlockInfo->previousBlockNumber);
            for (int j = 0; j < shtBlockInfo->numOfRecords; j++) {
                printf("\tRecord with name %s and blockID %d\n", shtTuple1[j].strName, shtTuple1[j].blockIndex);
            }
            CALL_OR_DIE(BF_UnpinBlock(block));
            if (shtBlockInfo->previousBlockNumber == -1)
                break;
            CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc, shtBlockInfo->previousBlockNumber, block));
            data = BF_Block_GetData(block);
            shtTuple1 = data;
            shtBlockInfo = data + shtInfo->offset;
        }
    }
    BF_Block_Destroy(&block);
    return 0;
}

/*Hash function , the commented out hash func might work better for bigger variety of names/more buckets*/
int SHT_HashFunction(unsigned char *string, int numOfBuckets) {
//    unsigned long hash = 5381;
//    int c;
//
//    while ((c = *string++))
//        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//    int hashNumber = (int)(hash % numOfBuckets);
//    return hashNumber;
    long hash = 0;
    int c;
    while ((c = *string++))
        hash += c;
    return (int) (hash % numOfBuckets);
}

int HashStatisticsSHT(char *filename) {
    SHT_info *shtInfo = SHT_OpenSecondaryIndex(filename);

    BF_Block *block;
    BF_Block_Init(&block);
    void *data;

    int allBlocks;
    BF_GetBlockCounter(shtInfo->fileDesc, &allBlocks);
    printf("SHT file with a total of %d buckets and %d blocks has:\n", shtInfo->numOfBuckets, allBlocks);

    int minRecords, maxRecords, averageRecords;
    int overflowBlocks;
    int totalBlocks = 0;
    int bucketBlocks;
    int bucketsWithOverFlowBlocks = 0;
    for (int i = 0; i < shtInfo->numOfBuckets; i++) {
        int lastBlock = shtInfo->bucketToLastBlock[i];
        CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc, lastBlock, block));
        data = BF_Block_GetData(block);
        SHT_block_info *shtBlockInfo = data + shtInfo->offset;

        minRecords = maxRecords = shtBlockInfo->numOfRecords;
        averageRecords = overflowBlocks = bucketBlocks = 0;
        while (1) {
            if (shtBlockInfo->numOfRecords < minRecords)
                minRecords = shtBlockInfo->numOfRecords;
            if (shtBlockInfo->numOfRecords > maxRecords)
                maxRecords = shtBlockInfo->numOfRecords;
            totalBlocks += 1;
            bucketBlocks += 1;
            averageRecords += shtBlockInfo->numOfRecords;
            CALL_OR_DIE(BF_UnpinBlock(block));
            if (shtBlockInfo->previousBlockNumber == -1)
                break;
            bucketsWithOverFlowBlocks += 1;
            overflowBlocks += 1;
            CALL_OR_DIE(BF_GetBlock(shtInfo->fileDesc, shtBlockInfo->previousBlockNumber, block));
            data = BF_Block_GetData(block);
            shtBlockInfo = data + shtInfo->offset;
        }
        printf("\tBucket number %d with a total of %d blocks( %d of them are overflow blocks) has min records: %d, max records: %d, average Records: %d\n",
               i, bucketBlocks, overflowBlocks, minRecords,
               maxRecords, averageRecords / bucketBlocks);
    }
    printf("Average number of blocks a bucket has is: %d\n\n", totalBlocks / shtInfo->numOfBuckets);
    BF_Block_Destroy(&block);
//    printSHTEntries(shtInfo);
    SHT_CloseSecondaryIndex(shtInfo);
    return 0;
}



