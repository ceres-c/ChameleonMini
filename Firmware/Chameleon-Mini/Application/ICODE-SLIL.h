/*
 * ICODE-SLIL.h
 *
 *  Created on: 27.12.2019
 *      Author: ceres-c & fptrs
 */

#ifndef ICODE_H_
#define ICODE_H_

#include "Application.h"

/*****************************
 * TODO remove this
 * E004035010963A52 - clown
 * E00403500fc94a29 - gorilla
 * E004035011b9b7a8 - tales
 *****************************/


#define ICODE_STD_UID_SIZE              ISO15693_GENERIC_UID_SIZE
#define ICODE_STD_MEM_SIZE              0x20        // Bytes
#define ICODE_BYTES_PER_BLCK            0x04
#define ICODE_BLCKS_PER_PAGE            0x04
#define ICODE_NUMBER_OF_BLCKS           ( ICODE_STD_MEM_SIZE / ICODE_BYTES_PER_BLCK )
#define ICODE_NUMBER_OF_BLCKS_DATASHEET 0x30        // See note below
#define ICODE_NUMBER_OF_PAGES           ( ICODE_STD_MEM_SIZE / (ICODE_BYTES_PER_BLCK * ICODE_BLCKS_PER_PAGE) )
/*
 * According to ICODE SLI-L datasheet Rev. 3.0 — 14 March 2007 (136430), part 8.1.2.10:
 * The "Get system information" command will report having 48 blocks, even if the tag actually has only 16.
 */

#define ICODE_IC_REFERENCE              0x03        // From ICODE SLI-L datasheet

#define ICODE_MEM_UID_ADDRESS           0x40        // From 0x40 to 0x47 - UID
#define ICODE_MEM_AFI_ADDRESS           0x48        // AFI byte address
#define ICODE_MEM_DSFID_ADDRESS         0x49        // DSFID byte adress
#define ICODE_MEM_EAS_ADDRESS           0x4A        // From 0x4A to 0x4B - EAS
#define ICODE_MEM_INF_ADDRESS           0x4C        // Some status bits

#define ICODE_MEM_LSM_ADDRESS           0x50        // From 0x50 to 0x8F - Lock status masks
#define ICODE_MEM_PRV_PSW_ADDRESS       0x90        // From 0x90 to 0x93 - 32 bit Privacy Password
#define ICODE_MEM_DSTRY_PSW_ADDRESS     0x94        // From 0x94 to 0x97 - 32 bit Destroy SLI-L Password
#define ICODE_MEM_EAS_PSW_ADDRESS       0x98        // From 0x98 to 0x9B - 32 bit EAS Password

/* Bit masks */
#define ICODE_MASK_AFI_STATUS           ( 1 << 0 )  // Used with status bits
#define ICODE_MASK_DSFID_STATUS         ( 1 << 1 )
#define ICODE_MASK_EAS_STATUS           ( 1 << 2 )

/* Custom command code */
#define ICODE_CMD_SET_EAS               0xA2 // TODO
#define ICODE_CMD_RST_EAS               0xA3 // TODO
#define ICODE_CMD_LCK_EAS               0xA4 // TODO
#define ICODE_CMD_EAS_ALRM              0xA5 // TODO
#define ICODE_CMD_PRT_EAS               0xA6 // TODO
#define ICODE_CMD_WRT_EAS_ID            0xA7 // TODO

#define ICODE_CMD_GET_RND               0xB2
#define ICODE_CMD_SET_PSW               0xB3
#define ICODE_CMD_WRITE_PSW             0xB4 // TODO
#define ICODE_CMD_LOCK_PSW              0xB5 // TODO
#define ICODE_CMD_DESTROY               0xB9 // TODO
#define ICODE_CMD_ENABLE_PRCY           0xBA // TODO
#define ICODE_CMD_INV_PAGE_READ         0xB0 // TODO
#define ICODE_CMD_FAST_INV_PAGE_READ    0xB1 // TODO

/* Compile time switch */
/* ICODE_LOGIN_YES_CARD has to be uncommented if you want your emulated card
 * to accept any given password from the reader when a Login request (E4) is issued.
 * It is expecially useful when analyzing an unknown system and you want to fool a reader
 * into thiking you are using the original tag without actually knowing the password.
 */
// #define ICODE_LOGIN_YES_CARD

void ICODEAppInit(void);
void ICODEAppReset(void);
void ICODEAppTask(void);
void ICODEAppTick(void);
uint16_t ICODEAppProcess(uint8_t *FrameBuf, uint16_t FrameBytes);
void ICODEGetUid(ConfigurationUidType Uid);
void ICODESetUid(ConfigurationUidType Uid);

#endif /* ICODE_H_ */
