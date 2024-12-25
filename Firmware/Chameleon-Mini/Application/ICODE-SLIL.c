/*
 * ICODE-SLIL.c
 * 
 *  Created on: 27.12.2019
 *      Author: ceres-c & fptrs
 * 
 * TODO:
 *  - EAS handling
 *  - All commands marked as TODO in ICODE-SLIL.h
 *  - Long range commands (if possible)
 *  - Compile time switch for nonrandom get-random response
 *  - Compile time switch for Tonies (privacy mode enabled by default)?
 * 
 * NOTES:
 *  - To emulate Tonies, set State as STATE_PRIV in ICODEAppInit and ICODEAppReset
 */

#include "../Random.h"
#include "ISO15693-A.h"
#include "ICODE-SLIL.h"

static enum {
    STATE_READY,
    STATE_SELECTED,
    STATE_QUIET,
    STATE_PRIV
} State;

bool SLILLoggedIn;

void ICODEAppInit(void) {
    State = STATE_READY;

    FrameInfo.Flags         = NULL;
    FrameInfo.Command       = NULL;
    FrameInfo.Parameters    = NULL;
    FrameInfo.ParamLen      = 0;
    FrameInfo.Addressed     = false;
    FrameInfo.Selected      = false;
    SLILLoggedIn = false;
    MemoryReadBlock(&MyAFI, ICODE_MEM_AFI_ADDRESS, 1);

}

void ICODEAppReset(void) {
    State = STATE_READY;

    FrameInfo.Flags         = NULL;
    FrameInfo.Command       = NULL;
    FrameInfo.Parameters    = NULL;
    FrameInfo.ParamLen      = 0;
    FrameInfo.Addressed     = false;
    FrameInfo.Selected      = false;
    SLILLoggedIn = false;
}

void ICODEAppTask(void) {

}

void ICODEAppTick(void) {

}

uint16_t ICODE_GetRandom(uint8_t * FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint16_t random = 0; // super random
    FrameBuf[0] = ISO15693_RES_FLAG_NO_ERROR;
    memcpy(FrameBuf + 0x1, &random, 2);
    ResponseByteCount +=3;
    return ResponseByteCount;
}

uint16_t ICODE_SetPassword(uint8_t * FrameBuf, uint16_t FrameBytes) {
    /* accept any password */
    FrameBuf[0] = ISO15693_RES_FLAG_NO_ERROR;
    State = STATE_READY;
    return 1;
}

uint16_t ICODE_Lock_Block(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t BlockAddress = *FrameInfo.Parameters;
    uint8_t LockStatus = 0;

    MemoryReadBlock(&LockStatus, (ICODE_MEM_LSM_ADDRESS + BlockAddress), 1);

    if (FrameInfo.ParamLen != 1)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    if (BlockAddress > ICODE_NUMBER_OF_BLCKS) {
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_OPT_NOT_SUPP;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount; /* malformed: trying to lock a non-existing block */
    }


    if (LockStatus > ISO15693_MASK_UNLOCKED) { /* LockStatus 0x00 represent unlocked block, greater values are different kind of locks */
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    } else {
        LockStatus |= ISO15693_MASK_USER_LOCK;
        MemoryWriteBlock(&LockStatus, (ICODE_MEM_LSM_ADDRESS + BlockAddress), 1); /* write user lock in memory */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    }

    return ResponseByteCount;
}

uint16_t ICODE_Write_Single(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t BlockAddress = *FrameInfo.Parameters;
    uint8_t *Dataptr = FrameInfo.Parameters + 0x01; /* Data to write begins on 2nd byte of the frame received by the reader */
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 5)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    if (BlockAddress > ICODE_NUMBER_OF_BLCKS) {
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_OPT_NOT_SUPP;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount; /* malformed: trying to write in a non-existing block */
    }

    MemoryReadBlock(&LockStatus, (ICODE_MEM_LSM_ADDRESS + BlockAddress), 1);

    if (LockStatus & ISO15693_MASK_FACTORY_LOCK) {
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_OPT_NOT_SUPP;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway - probably: no factory lock exists? */
    } else if (LockStatus & ISO15693_MASK_USER_LOCK) {
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_BLK_CHG_LKD;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    } else {
        MemoryWriteBlock(Dataptr, BlockAddress * ICODE_BYTES_PER_BLCK, ICODE_BYTES_PER_BLCK);
        FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR;
        ResponseByteCount += 1;
    }

    return ResponseByteCount;
}

uint16_t ICODE_Read_Single(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t FramePtr; /* holds the address where block's data will be put */
    uint8_t BlockAddress = FrameInfo.Parameters[0];
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 1)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    if (BlockAddress >= ICODE_NUMBER_OF_BLCKS) { /* check if the reader is requesting a sector out of bound */
        if (FrameInfo.Addressed) { /* If the request is addressed */
            FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
            FrameBuf[ISO15693_RES_ADDR_PARAM] = 0x0F; /* Magic number from real tag */
            ResponseByteCount += 2; /* Copied this behaviour from real tag, not specified in ISO documents */
        }
        return ResponseByteCount; /* If not addressed real tag does not respond */
    }

    FramePtr = 1;

    if (FrameBuf[ISO15693_ADDR_FLAGS] & ISO15693_REQ_FLAG_OPTION) { /* request with option flag set */
        MemoryReadBlock(&LockStatus, (ICODE_MEM_LSM_ADDRESS + BlockAddress), 1);
        if (LockStatus & ISO15693_MASK_FACTORY_LOCK)  { /* tests if the n-th bit of the factory bitmask if set to 1 */
            FrameBuf[FramePtr] = ISO15693_MASK_FACTORY_LOCK; /* return bit 1 set as 1 (factory locked) */
        } else if (LockStatus & ISO15693_MASK_USER_LOCK) { /* tests if the n-th bit of the user bitmask if set to 1 */
            FrameBuf[FramePtr] = ISO15693_MASK_USER_LOCK; /* return bit 0 set as 1 (user locked) */
        } else
            FrameBuf[FramePtr] = ISO15693_MASK_UNLOCKED; /* return lock status 00 (unlocked) */
        FramePtr += 1; /* block's data from byte 2 */
        ResponseByteCount += 1;
    }

    MemoryReadBlock(&FrameBuf[FramePtr], BlockAddress * ICODE_BYTES_PER_BLCK, ICODE_BYTES_PER_BLCK);
    ResponseByteCount += 4;

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    ResponseByteCount += 1;

    return ResponseByteCount;
}

uint16_t ICODE_Read_Multiple(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t FramePtr; /* holds the address where block's data will be put */
    uint8_t BlockAddress = FrameInfo.Parameters[0];
    uint8_t BlocksNumber = FrameInfo.Parameters[1] + 0x01; /* according to ISO standard, we have to read 0x08 blocks if we get 0x07 in request */

    if (FrameInfo.ParamLen != 2)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    if (BlockAddress >= ICODE_NUMBER_OF_BLCKS) { /* the reader is requesting a block out of bound */
        if (FrameInfo.Addressed) { /* If the request is addressed */
            FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
            FrameBuf[ISO15693_RES_ADDR_PARAM] = 0x0F; /* Magic number from real tag */
            ResponseByteCount += 2; /* Copied this behaviour from real tag, not specified in ISO documents */
        }
        return ResponseByteCount; /* If not addressed real tag does not respond */
    } else if ((BlockAddress + BlocksNumber) >= ICODE_NUMBER_OF_BLCKS) { /* last block is out of bound */
        BlocksNumber = ICODE_NUMBER_OF_BLCKS - BlockAddress; /* we read up to latest block, as real tag does */
    }

    FramePtr = 1; /* start of response data  */

    if ((FrameBuf[ISO15693_ADDR_FLAGS] & ISO15693_REQ_FLAG_OPTION) == 0) {   /* blocks' lock status is not requested */
        /* read data straight into frame */
        MemoryReadBlock(&FrameBuf[FramePtr], BlockAddress * ICODE_BYTES_PER_BLCK, BlocksNumber * ICODE_BYTES_PER_BLCK);
        ResponseByteCount += BlocksNumber * ICODE_BYTES_PER_BLCK;

    } else { /* we have to slice blocks' data with lock statuses */
        uint8_t DataBuffer[ BlocksNumber * ICODE_BYTES_PER_BLCK ]; /* a temporary vector with blocks' content */
        uint8_t LockStatusBuffer[ BlocksNumber ]; /* a temporary vector with blocks' lock status */

        /* read all at once to reduce timing issues */
        MemoryReadBlock(&DataBuffer, BlockAddress * ICODE_BYTES_PER_BLCK, BlocksNumber * ICODE_BYTES_PER_BLCK);
        MemoryReadBlock(&LockStatusBuffer, ICODE_MEM_LSM_ADDRESS + BlockAddress, BlocksNumber);

        for (uint8_t block = 0; block < BlocksNumber; block++) { /* we cycle through the blocks */

            /* add lock status */
            FrameBuf[FramePtr++] = LockStatusBuffer[block]; /* Byte in dump equals to the byte that has to be sent */
            /* I.E. We store 0x01 to identify user lock, which is the same as what ISO15693 enforce */
            ResponseByteCount += 1;

            /* then copy block's data */
            FrameBuf[FramePtr++] = DataBuffer[block * ICODE_BYTES_PER_BLCK + 0];
            FrameBuf[FramePtr++] = DataBuffer[block * ICODE_BYTES_PER_BLCK + 1];
            FrameBuf[FramePtr++] = DataBuffer[block * ICODE_BYTES_PER_BLCK + 2];
            FrameBuf[FramePtr++] = DataBuffer[block * ICODE_BYTES_PER_BLCK + 3];
            ResponseByteCount += ICODE_BYTES_PER_BLCK;
        }
    }

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    ResponseByteCount += 1;
    return ResponseByteCount;
}

uint16_t ICODE_Write_AFI(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t AFI = FrameInfo.Parameters[0];
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 1)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    MemoryReadBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1);

    if (LockStatus & ICODE_MASK_AFI_STATUS) {  /* The AFI is locked */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        // ResponseByteCount += 2;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount;
    }

    MemoryWriteBlock(&AFI, ICODE_MEM_AFI_ADDRESS, 1); /* Actually write new AFI */
    MyAFI = AFI; /* And update global variable */

    // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    // ResponseByteCount += 1;
    ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    return ResponseByteCount;
}

uint16_t ICODE_Lock_AFI(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 0)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    MemoryReadBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1);

    if (LockStatus & ICODE_MASK_AFI_STATUS) {  /* The AFI is already locked */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        // ResponseByteCount += 2;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount;
    }

    LockStatus |= ICODE_MASK_AFI_STATUS;

    MemoryWriteBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1); /* Write in info bits AFI lockdown */

    // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    // ResponseByteCount += 1;
    ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    return ResponseByteCount;
}

uint16_t ICODE_Write_DSFID(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t DSFID = FrameInfo.Parameters[0];
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 1)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    MemoryReadBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1);

    if (LockStatus & ICODE_MASK_DSFID_STATUS) {  /* The DSFID is locked */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        // ResponseByteCount += 2;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount;
    }

    MemoryWriteBlock(&DSFID, ICODE_MEM_DSFID_ADDRESS, 1); /* Actually write new DSFID */

    // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    // ResponseByteCount += 1;
    ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    return ResponseByteCount;
}

uint16_t ICODE_Lock_DSFID(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t LockStatus = 0;

    if (FrameInfo.ParamLen != 0)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    MemoryReadBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1);

    if (LockStatus & ICODE_MASK_DSFID_STATUS) {  /* The DSFID is already locked */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        // ResponseByteCount += 2;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount;
    }

    LockStatus |= ICODE_MASK_DSFID_STATUS;

    MemoryWriteBlock(&LockStatus, ICODE_MEM_INF_ADDRESS, 1); /* Write in info bits DSFID lockdown */

    // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    // ResponseByteCount += 1;
    ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
    return ResponseByteCount;
}

uint8_t ICODE_Get_SysInfo(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t FramePtr; /* holds the address where block's data will be put */

    if (FrameInfo.ParamLen != 0)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    FramePtr = 1;

    /* I've no idea how this request could generate errors ._.
    if ( ) {
        FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        ResponseByteCount += 2;
        return ResponseByteCount;
    }
    */

    /* System info flags */
    FrameBuf[FramePtr] = EM4233_SYSINFO_BYTE; /* check EM4233SLIC datasheet for this */
    FramePtr += 1;             /* Move forward the buffer data pointer */
    ResponseByteCount += 1;    /* Increment the response count */

    /* Then append UID */
    uint8_t Uid[ActiveConfiguration.UidSize];
    ICODEGetUid(Uid);
    ISO15693CopyUid(&FrameBuf[FramePtr], Uid);
    FramePtr += ISO15693_GENERIC_UID_SIZE;             /* Move forward the buffer data pointer */
    ResponseByteCount += ISO15693_GENERIC_UID_SIZE;    /* Increment the response count */

    /* Append DSFID */
    if (EM4233_SYSINFO_BYTE & (1 << 0)) {
        MemoryReadBlock(&FrameBuf[FramePtr], ICODE_MEM_DSFID_ADDRESS, 1);
        FramePtr += 1;             /* Move forward the buffer data pointer */
        ResponseByteCount += 1;    /* Increment the response count */
    }

    /* Append AFI */
    if (EM4233_SYSINFO_BYTE & (1 << 1)) {
        MemoryReadBlock(&FrameBuf[FramePtr], ICODE_MEM_AFI_ADDRESS, 1);
        FramePtr += 1;             /* Move forward the buffer data pointer */
        ResponseByteCount += 1;    /* Increment the response count */
    }

    /* Append VICC memory size */
    if (EM4233_SYSINFO_BYTE & (1 << 2)) {
        FrameBuf[FramePtr] = ICODE_NUMBER_OF_BLCKS - 0x01;
        FramePtr += 1;             /* Move forward the buffer data pointer */
        ResponseByteCount += 1;    /* Increment the response count */

        FrameBuf[FramePtr] = ICODE_BYTES_PER_BLCK - 0x01;
        FramePtr += 1;             /* Move forward the buffer data pointer */
        ResponseByteCount += 1;    /* Increment the response count */
    }

    /* Append IC reference */
    if (EM4233_SYSINFO_BYTE & (1 << 3)) {
        FrameBuf[FramePtr] = ICODE_IC_REFERENCE;
        FramePtr += 1;             /* Move forward the buffer data pointer */
        ResponseByteCount += 1;    /* Increment the response count */
    }

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    ResponseByteCount += 1;
    return ResponseByteCount;
}

uint16_t ICODE_Get_Multi_Block_Sec_Stat(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t FramePtr; /* holds the address where block's data will be put */
    uint8_t BlockAddress = FrameInfo.Parameters[0];
    uint8_t BlocksNumber = FrameInfo.Parameters[1] + 0x01;

    if (FrameInfo.ParamLen != 2)
        return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

    if (BlockAddress > ICODE_NUMBER_OF_BLCKS) { /* the reader is requesting a starting block out of bound */
        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_BLK_NOT_AVL;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE; /* real tag does not respond anyway */
        return ResponseByteCount;
    } else if ((BlockAddress + BlocksNumber) >= ICODE_NUMBER_OF_BLCKS) { /* last block is out of bound */
        BlocksNumber = ICODE_NUMBER_OF_BLCKS - BlockAddress; /* we read up to latest block, as real tag does */
    }

    FramePtr = 1; /* start of response data  */

    /* read all at once to reduce timing issues */
    MemoryReadBlock(&FrameBuf[FramePtr], ICODE_MEM_LSM_ADDRESS + BlockAddress, BlocksNumber);
    ResponseByteCount += BlocksNumber;

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    ResponseByteCount += 1;
    return ResponseByteCount;
}

uint16_t ICODE_Select(uint8_t *FrameBuf, uint16_t FrameBytes, uint8_t *Uid) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    /* I've no idea how this request could generate errors ._.
    if ( ) {
        FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        ResponseByteCount += 2;
        return ResponseByteCount;
    }
    */

    bool UidEquals = ISO15693CompareUid(&FrameBuf[ISO15693_REQ_ADDR_PARAM], Uid);

    if (!(FrameBuf[ISO15693_ADDR_FLAGS] & ISO15693_REQ_FLAG_ADDRESS) ||
            (FrameBuf[ISO15693_ADDR_FLAGS] & ISO15693_REQ_FLAG_SELECT)
       ) {
        /* tag should remain silent if Select is performed without address flag or with select flag */
        return ISO15693_APP_NO_RESPONSE;
    } else if (!UidEquals) {
        /* tag should remain silent and reset if Select is performed against another UID,
         * whether our the tag is selected or not
         */
        State = STATE_READY;
        return ISO15693_APP_NO_RESPONSE;
    } else if (State != STATE_SELECTED && UidEquals) {
        State = STATE_SELECTED;
        FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR;
        ResponseByteCount += 1;
        return ResponseByteCount;
    }

    /* This should never happen (TM), I've added it to shut the compiler up */
    State = STATE_READY;
    return ISO15693_APP_NO_RESPONSE;
}

uint16_t ICODE_Reset_To_Ready(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    /* I've no idea how this request could generate errors ._.
    if ( ) {
        FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        ResponseByteCount += 2;
        return ResponseByteCount;
    }
    */
    FrameInfo.Flags         = NULL;
    FrameInfo.Command       = NULL;
    FrameInfo.Parameters    = NULL;
    FrameInfo.ParamLen      = 0;
    FrameInfo.Addressed     = false;
    FrameInfo.Selected      = false;

    State = STATE_READY;

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR;
    ResponseByteCount += 1;
    return ResponseByteCount;
}

uint16_t ICODE_Login(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t Password[4] = { 0 };

    if (FrameInfo.ParamLen != 4 || !FrameInfo.Addressed || !(FrameInfo.Selected && State == STATE_SELECTED))
        /* Malformed: not enough or too much data. Also this command only works in addressed mode */
        return ISO15693_APP_NO_RESPONSE;

    MemoryReadBlock(&Password, EM4233_MEM_PSW_ADDRESS, 4);

#ifdef ICODE_LOGIN_YES_CARD
    /* Accept any password from reader as correct one */
    SLILLoggedIn = true;

    MemoryWriteBlock(FrameInfo.Parameters, EM4233_MEM_PSW_ADDRESS, 4); /* Store password in memory for retrival */

#else
    /* Check if the password is actually the right one */
    if (memcmp(Password, FrameInfo.Parameters, 4) != 0) { /* Incorrect password */
        SLILLoggedIn = false;

        // FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
        // FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_GENERIC;
        ResponseByteCount = ISO15693_APP_NO_RESPONSE;
        return ResponseByteCount;
    }

#endif

    SLILLoggedIn = true;

    FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR; /* flags */
    ResponseByteCount += 1;
    return ResponseByteCount;
}

uint16_t ICODEAppProcess(uint8_t *FrameBuf, uint16_t FrameBytes) {
    uint16_t ResponseByteCount = ISO15693_APP_NO_RESPONSE;
    uint8_t Uid[ActiveConfiguration.UidSize];
    ICODEGetUid(Uid);

    if ((FrameBytes < ISO15693_MIN_FRAME_SIZE) || !ISO15693CheckCRC(FrameBuf, FrameBytes - ISO15693_CRC16_SIZE))
        /* malformed frame */
        return ResponseByteCount;

    if (FrameBuf[ISO15693_REQ_ADDR_CMD] == ISO15693_CMD_SELECT) {
        /* Select has its own path before PrepareFrame because we have to change the variable State
         * from Select to Ready if "Select" cmd is addressed to another tag.
         * It felt weird to add this kind of check in ISO15693PrepareFrame, which should not
         * interfere with tag specific variables, such as State in this case.
         */
        ResponseByteCount = ICODE_Select(FrameBuf, FrameBytes, Uid);
    } else if (!ISO15693PrepareFrame(FrameBuf, FrameBytes, &FrameInfo, State == STATE_SELECTED, Uid, MyAFI))
        return ISO15693_APP_NO_RESPONSE;

    if (State == STATE_READY || State == STATE_SELECTED) {

        if (*FrameInfo.Command == ISO15693_CMD_INVENTORY) {
            if (FrameInfo.ParamLen == 0)
                return ISO15693_APP_NO_RESPONSE; /* malformed: not enough or too much data */

            if (ISO15693AntiColl(FrameBuf, FrameBytes, &FrameInfo, Uid)) {
                FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_NO_ERROR;
                MemoryReadBlock(&FrameBuf[ISO15693_RES_ADDR_PARAM], ICODE_MEM_DSFID_ADDRESS, 1);
                ISO15693CopyUid(&FrameBuf[ISO15693_RES_ADDR_PARAM + 0x01], Uid);
                ResponseByteCount += 10;
            }

        } else if ((*FrameInfo.Command == ISO15693_CMD_STAY_QUIET) && FrameInfo.Addressed) {
            State = STATE_QUIET;

        } else if (*FrameInfo.Command == ISO15693_CMD_READ_SINGLE) {
            ResponseByteCount = ICODE_Read_Single(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_WRITE_SINGLE) {
            ResponseByteCount = ICODE_Write_Single(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_LOCK_BLOCK) {
            ResponseByteCount = ICODE_Lock_Block(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_READ_MULTIPLE) {
            ResponseByteCount = ICODE_Read_Multiple(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_WRITE_AFI) {
            ResponseByteCount = ICODE_Write_AFI(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_LOCK_AFI) {
            ResponseByteCount = ICODE_Lock_AFI(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_WRITE_DSFID) {
            ResponseByteCount = ICODE_Write_DSFID(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_LOCK_DSFID) {
            ResponseByteCount = ICODE_Lock_DSFID(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_GET_SYS_INFO) {
            ResponseByteCount = ICODE_Get_SysInfo(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_GET_BLOCK_SEC) {
            ResponseByteCount = ICODE_Get_Multi_Block_Sec_Stat(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == ISO15693_CMD_RESET_TO_READY) {
            ResponseByteCount = ICODE_Reset_To_Ready(FrameBuf, FrameBytes);

        } else if (*FrameInfo.Command == EM4233_CMD_LOGIN) {
            ResponseByteCount = ICODE_Login(FrameBuf, FrameBytes);

        } else {
            if (FrameInfo.Addressed) {
                FrameBuf[ISO15693_ADDR_FLAGS] = ISO15693_RES_FLAG_ERROR;
                FrameBuf[ISO15693_RES_ADDR_PARAM] = ISO15693_RES_ERR_NOT_SUPP;
                ResponseByteCount = 2;
            } /* EM4233 respond with error flag only to addressed commands */
        }

    } else if (State == STATE_QUIET) {
        if (*FrameInfo.Command == ISO15693_CMD_RESET_TO_READY) {
            ResponseByteCount = ICODE_Reset_To_Ready(FrameBuf, FrameBytes);
        }
    } else if (State == STATE_PRIV) {
        if (*FrameInfo.Command == ICODE_CMD_SET_PSW) {
           ResponseByteCount = ICODE_SetPassword(FrameBuf, FrameBytes);
        } else if (*FrameInfo.Command == ICODE_CMD_GET_RND) {
           ResponseByteCount = ICODE_GetRandom(FrameBuf, FrameBytes);
        }
    }

    if (ResponseByteCount > 0) {
        /* There is data to send. Append CRC */
        ISO15693AppendCRC(FrameBuf, ResponseByteCount);
        ResponseByteCount += ISO15693_CRC16_SIZE;
    }

    return ResponseByteCount;
}

void ICODEGetUid(ConfigurationUidType Uid) {
    MemoryReadBlock(&Uid[0], ICODE_MEM_UID_ADDRESS, ActiveConfiguration.UidSize);
}

void ICODESetUid(ConfigurationUidType Uid) {
    MemoryWriteBlock(Uid, ICODE_MEM_UID_ADDRESS, ActiveConfiguration.UidSize);
}
