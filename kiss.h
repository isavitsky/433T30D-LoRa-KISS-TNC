// kiss.h
#ifndef KISS_H
#define KISS_H

#define FEND	0xC0
#define FESC	0xDB
#define TFEND	0xDC
#define TFESC	0xDD

#define CMD_DATA_FRAME          0x00
#define CMD_TX_DELAY            0x01
#define CMD_P                   0x02
#define CMD_SLOT_TIME           0x03
#define CMD_SET_HARDWARE        0x06
#define CMD_SET_FEC             0x08

#define CMD_SF_MASK             0xA0
#define CMD_BW_MASK             0xB0
#define CMD_PREAMBLE_MASK       0xC0

#define FRAME_FIRST             2       /* First frame */
#define FRAME_INTER             1       /* Intermediate frame */
#define FRAME_LAST              0       /* last frame */

void kiss(void);

#endif