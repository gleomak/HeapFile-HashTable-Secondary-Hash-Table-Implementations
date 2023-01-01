#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

/*Used exclusively for HP_OpenFile*/
#define CALL_BF2(call)      \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return NULL;            \
  }                         \
}


int HP_CreateFile(char *fileName) {
    /*Creating the file */
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_CreateFile(fileName))
    CALL_BF(BF_OpenFile(fileName, &fd1));
    void *data;

    /*Initializing the HPInfo/HPBlockInfo data for the first block */
    CALL_BF(BF_AllocateBlock(fd1, block));  // Δημιουργία καινούριου block
    data = BF_Block_GetData(block);
    HP_info *info = malloc(sizeof(HP_info));
    info->fileType = "HP";
    info->fileDesc = fd1;
    info->lastBlock = 0;
    info->offset = 512 - sizeof(HP_block_info);
    info->firstBlock = NULL;
    memcpy(data, info, sizeof(HP_info));
    printf("Offset is : %d\n", info->offset);
    HP_block_info *info1 = malloc(sizeof(HP_block_info));
    info1->numOfRecords = 0;
    info1->maxRecords = (512 - sizeof(HP_block_info)) / sizeof(Record);
    info1->nextBlockIndex = 0;
    memcpy(data + info->offset, info1, sizeof(HP_block_info));

    /*Set dirty and unpin , close the file */
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    CALL_BF(BF_CloseFile(fd1));               //Κλείσιμο αρχείου και αποδέσμευση μνήμης
    BF_Block_Destroy(&block);
    free(info);
    free(info1);
    return 0;
}

HP_info *HP_OpenFile(char *fileName) {
    /*Re-opening the file*/
    int fd1;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF2(BF_OpenFile(fileName, &fd1));
    void *data;

    /*Pinning the first block update the HPInfo values*/
    CALL_BF2(BF_GetBlock(fd1, 0, block));
    data = BF_Block_GetData(block);
    HP_info *hpInfo = data;
    hpInfo->fileDesc = fd1;
    hpInfo->firstBlock = block;
    if (strcmp(hpInfo->fileType , "HP") != 0) {
        printf("This is not a HP file \n");
        return NULL;
    }
    return hpInfo;
}


int HP_CloseFile(HP_info *hp_info) {
    int fd1 = hp_info->fileDesc;
//    printEntries(hp_info); //In case we want to print the entries

    /*Setting dirty and unpinning the first block, Closing the file*/
    BF_Block_SetDirty(hp_info->firstBlock);
    CALL_BF(BF_UnpinBlock(hp_info->firstBlock));
    BF_Block_Destroy(&hp_info->firstBlock);

    CALL_BF(BF_CloseFile(fd1));
    return 0;
}

int HP_InsertEntry(HP_info *hp_info, Record record) {
    /*Return -1 if HPInfo is NULL(something went wrong)*/
    if (hp_info == NULL)
        return -1;

    BF_Block *lastBlock;
    void *data;
    HP_block_info *lastBlockInfo;

    /*If the last block is 0 that means we are on the first entry*/
    if (hp_info->lastBlock == 0) {
        data = hp_info->firstBlock;
        lastBlockInfo = data + hp_info->offset;
    } else { //else recover the last block data
        BF_Block_Init(&lastBlock);
        CALL_BF(BF_GetBlock(hp_info->fileDesc, hp_info->lastBlock, lastBlock));
        data = BF_Block_GetData(lastBlock);
        lastBlockInfo = data + hp_info->offset;
    }

    /*If we are on the first block or a block is full create a new block*/
    if (hp_info->lastBlock == 0 || lastBlockInfo->numOfRecords == lastBlockInfo->maxRecords) {
        BF_Block *newBlock;
        BF_Block_Init(&newBlock);
        CALL_BF(BF_AllocateBlock(hp_info->fileDesc, newBlock));  // Δημιουργία καινούριου block
        void *data2 = BF_Block_GetData(newBlock);
        HP_block_info *hpBlockInfo = malloc(sizeof(HP_block_info));
        hpBlockInfo->maxRecords = (512 - sizeof(HP_block_info)) / sizeof(Record);
        hpBlockInfo->numOfRecords = 1;
        hpBlockInfo->nextBlockIndex = -1;
        memcpy(data2 + hp_info->offset, hpBlockInfo, sizeof(HP_block_info));
        free(hpBlockInfo);

        /*Updating the hpInfo and lastBlockInfo values after the creation*/
        hp_info->lastBlock += 1;
        lastBlockInfo->nextBlockIndex = hp_info->lastBlock;

        /*Inserting the entry , Setting dirty unpinning the block*/
        Record *rec = data2;
        rec[0] = record;

        BF_Block_SetDirty(newBlock);
        CALL_BF(BF_UnpinBlock(newBlock));
        BF_Block_Destroy(&newBlock);
    } else { // else just insert the entry and update its info
        Record *rec = data;
        rec[lastBlockInfo->numOfRecords] = record;
        lastBlockInfo->numOfRecords += 1;
    }
    /*If the lastBlockInfo.numOfRecords is 0 that means we should not unpin the block,*/
    /*because the first block has to be always pinned*/
    if (lastBlockInfo->numOfRecords != 0) {
        BF_Block_SetDirty(lastBlock);
        CALL_BF(BF_UnpinBlock(lastBlock));
        BF_Block_Destroy(&lastBlock);
    }

    return hp_info->lastBlock;
}

/*Auxiliary function that prints all the entries for every block*/
int printEntries(HP_info *hp_info) {
    int fd1 = hp_info->fileDesc;
    printf("FD IS %d , last block is %d\n", fd1, hp_info->lastBlock);
    int numOfBlocks;
    CALL_BF(BF_GetBlockCounter(fd1, &numOfBlocks));
    printf("HP file has %d blocks \n", numOfBlocks);
    void *data;
    BF_Block *block;
    BF_Block_Init(&block);
    for (int i = 1; i < numOfBlocks; i++) {
        CALL_BF(BF_GetBlock(fd1, i, block));
        data = BF_Block_GetData(block);
        Record *rec = data;
        HP_block_info *hpBlockInfo = data + hp_info->offset;
        printf("Block %d with next block pointer %d has %d records : \n", i, hpBlockInfo->nextBlockIndex,
               hpBlockInfo->numOfRecords);
        for (int j = 0; j < hpBlockInfo->numOfRecords; j++) {
            printf("\tRecord %d is : ", j);
            printRecord(rec[j]);
        }
        CALL_BF(BF_UnpinBlock(block));

    }
    BF_Block_Destroy(&block);
    return 0;
}

int HP_GetAllEntries(HP_info *hp_info, int value) {

    /*Return -1 if the hpInfo value is null */
    if (hp_info == NULL)
        return -1;

    int numOfBuckets;
    CALL_BF(BF_GetBlockCounter(hp_info->fileDesc, &numOfBuckets));
    void *data;
    BF_Block *block;
    BF_Block_Init(&block);

    /*Iterate through the blocks to find the desired id*/
    for (int i = 1; i < numOfBuckets; i++) {
        CALL_BF(BF_GetBlock(hp_info->fileDesc, i, block));
        data = BF_Block_GetData(block);
        Record *rec = data;
        HP_block_info *hpBlockInfo = data + hp_info->offset;
        for (int j = 0; j < hpBlockInfo->numOfRecords; j++) {
            if (rec[j].id == value) {
                printf("\n");
                printRecord(rec[j]);
                CALL_BF(BF_UnpinBlock(block));
                BF_Block_Destroy(&block);
                return i; // return the number of blocks we iterated
            }
        }
        CALL_BF(BF_UnpinBlock(block));
    }
    BF_Block_Destroy(&block);

    /*We did not find the value return -1*/
    return -1;
}

