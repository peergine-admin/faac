/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: bitstream.c,v 1.2 2001/01/23 13:46:18 menno Exp $
 */

#include <stdlib.h>
#include <memory.h>

#include "coder.h"
#include "channels.h"
#include "huffman.h"
#include "bitstream.h"


int WriteBitstream(faacEncHandle hEncoder,
				   CoderInfo *coderInfo,
				   ChannelInfo *channelInfo,
				   BitStream *bitStream,
				   int numChannel)
{
	int channel;
	int bits = 0;
	int bitsLeftAfterFill, numFillBits;

	CountBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannel);

	bits += WriteADTSHeader(hEncoder, bitStream, 1);

	for (channel = 0; channel < numChannel; channel++) {

		if (channelInfo[channel].present) {

			/* Write out a single_channel_element */
			if (!channelInfo[channel].cpe) {

				if (channelInfo[channel].lfe) {
					/* Write out lfe */ 
					bits += WriteLFE(&coderInfo[channel],
						&channelInfo[channel],
						bitStream,
						1);
				} else {
					/* Write out sce */
					bits += WriteSCE(&coderInfo[channel],
						&channelInfo[channel],
						bitStream,
						1);
				}

			} else {

				if (channelInfo[channel].ch_is_left) {
					/* Write out cpe */
					bits += WriteCPE(&coderInfo[channel],
						&coderInfo[channelInfo[channel].paired_ch],
						&channelInfo[channel],
						bitStream,
						1);
				}
			}
		}
	}

	/* Compute how many fill bits are needed to avoid overflowing bit reservoir */
	/* Save room for ID_END terminator */
	if (bits < (8 - LEN_SE_ID) ) {
		numFillBits = 8 - LEN_SE_ID - bits;
	} else {
		numFillBits = 0;
	}

	/* Write AAC fill_elements, smallest fill element is 7 bits. */
	/* Function may leave up to 6 bits left after fill, so tell it to fill a few extra */
	numFillBits += 6;
	bitsLeftAfterFill = WriteAACFillBits(bitStream, numFillBits, 1);
	bits += (numFillBits - bitsLeftAfterFill);

	/* Write ID_END terminator */
	bits += LEN_SE_ID;
    PutBit(bitStream, ID_END, LEN_SE_ID);

	/* Now byte align the bitstream */
	bits += ByteAlign(bitStream, 1);

	return bits;
}

static int CountBitstream(faacEncHandle hEncoder,
						  CoderInfo *coderInfo,
						  ChannelInfo *channelInfo,
						  BitStream *bitStream,
						  int numChannel)
{
	int channel;
	int bits = 0;
	int bitsLeftAfterFill, numFillBits;

	bits += WriteADTSHeader(hEncoder, bitStream, 0);

	for (channel = 0; channel < numChannel; channel++) {

		if (channelInfo[channel].present) {

			/* Write out a single_channel_element */
			if (!channelInfo[channel].cpe) {

				if (channelInfo[channel].lfe) {
					/* Write out lfe */ 
					bits += WriteLFE(&coderInfo[channel],
						&channelInfo[channel],
						bitStream,
						0);
				} else {
					/* Write out sce */
					bits += WriteSCE(&coderInfo[channel],
						&channelInfo[channel],
						bitStream,
						0);
				}

			} else {

				if (channelInfo[channel].ch_is_left) {
					/* Write out cpe */
					bits += WriteCPE(&coderInfo[channel],
						&coderInfo[channelInfo[channel].paired_ch],
						&channelInfo[channel],
						bitStream,
						0);
				}
			}
		}
	}

	/* Compute how many fill bits are needed to avoid overflowing bit reservoir */
	/* Save room for ID_END terminator */
	if (bits < (8 - LEN_SE_ID) ) {
		numFillBits = 8 - LEN_SE_ID - bits;
	} else {
		numFillBits = 0;
	}

	/* Write AAC fill_elements, smallest fill element is 7 bits. */
	/* Function may leave up to 6 bits left after fill, so tell it to fill a few extra */
	numFillBits += 6;
	bitsLeftAfterFill = WriteAACFillBits(bitStream, numFillBits, 0);
	bits += (numFillBits - bitsLeftAfterFill);

	/* Write ID_END terminator */
	bits += LEN_SE_ID;

	/* Now byte align the bitstream */
	bits += ByteAlign(bitStream, 0);

	hEncoder->usedBytes = bit2byte(bits);

	return bits;
}

static int WriteADTSHeader(faacEncHandle hEncoder,
						   BitStream *bitStream,
						   int writeFlag)
{
	if (writeFlag) {
		/* Fixed ADTS header */
		PutBit(bitStream, 0xFFFF, 12); // 12 bit Syncword
		PutBit(bitStream, 1, 1); // ID
		PutBit(bitStream, 0, 2); // layer
		PutBit(bitStream, 1, 1); // protection absent
		PutBit(bitStream, hEncoder->aacProfile, 2); // profile
		PutBit(bitStream, hEncoder->sampleRateIdx, 4); // sampling rate
		PutBit(bitStream, 0, 1); // private bit
		PutBit(bitStream, 1, 3); // ch. config (must be > 0)
		PutBit(bitStream, 0, 1); // original/copy
		PutBit(bitStream, 0, 1); // home
		PutBit(bitStream, 0, 2); // emphasis

		/* Variable ADTS header */
		PutBit(bitStream, 0, 1); // copyr. id. bit
		PutBit(bitStream, 0, 1); // copyr. id. start
		PutBit(bitStream, hEncoder->usedBytes, 13);
		PutBit(bitStream, 0x7FF, 11); // buffer fullness (0x7FF for VBR)
		PutBit(bitStream, 0, 2); // raw data blocks (0+1=1)
	}
	return 58;
}

static int WriteCPE(CoderInfo *coderInfoL,
					CoderInfo *coderInfoR,
					ChannelInfo *channelInfo,
					BitStream* bitStream,
					int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		/* write ID_CPE, single_element_channel() identifier */
		PutBit(bitStream, ID_CPE, LEN_SE_ID);

		/* write the element_identifier_tag */
		PutBit(bitStream, channelInfo->tag, LEN_TAG);

		/* common_window? */
		PutBit(bitStream, channelInfo->common_window, LEN_COM_WIN);
	}

	bits += LEN_SE_ID;
	bits += LEN_TAG;
	bits += LEN_COM_WIN;

	/* if common_window, write ics_info */
	if (channelInfo->common_window) {
		int numWindows, maxSfb;

		bits += WriteICSInfo(coderInfoL, bitStream, writeFlag);
		numWindows = coderInfoL->num_window_groups;
		maxSfb = coderInfoL->max_sfb;

		if (writeFlag) {
			PutBit(bitStream, channelInfo->msInfo.is_present, LEN_MASK_PRES);
			if (channelInfo->msInfo.is_present == 1) {
				int g;
				int b;
				for (g=0;g<numWindows;g++) {
					for (b=0;b<maxSfb;b++) {
						PutBit(bitStream, channelInfo->msInfo.ms_used[g*maxSfb+b], LEN_MASK);
					}
				}
			}
		}
		bits += LEN_MASK_PRES;
		if (channelInfo->msInfo.is_present == 1)
			bits += (numWindows*maxSfb*LEN_MASK);
	}

	/* Write individual_channel_stream elements */
	bits += WriteICS(coderInfoL, bitStream, channelInfo->common_window, writeFlag);
	bits += WriteICS(coderInfoR, bitStream, channelInfo->common_window, writeFlag);

	return bits;
}

static int WriteSCE(CoderInfo *coderInfo,
					ChannelInfo *channelInfo,
					BitStream *bitStream,
					int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		/* write Single Element Channel (SCE) identifier */
		PutBit(bitStream, ID_SCE, LEN_SE_ID);

		/* write the element identifier tag */
		PutBit(bitStream, channelInfo->tag, LEN_TAG);
	}

	bits += LEN_SE_ID;
	bits += LEN_TAG;

	/* Write an Individual Channel Stream element */
	bits += WriteICS(coderInfo, bitStream, 0, writeFlag);

	return bits;
}

static int WriteLFE(CoderInfo *coderInfo,
					ChannelInfo *channelInfo,
					BitStream *bitStream,
					int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		/* write ID_LFE, lfe_element_channel() identifier */
		PutBit(bitStream, ID_LFE, LEN_SE_ID);

		/* write the element_identifier_tag */
		PutBit(bitStream, channelInfo->tag, LEN_TAG);
	}

	bits += LEN_SE_ID;
	bits += LEN_TAG;

	/* Write an individual_channel_stream element */
	bits += WriteICS(coderInfo, bitStream, 0, writeFlag);

	return bits;
}

static int WriteICSInfo(CoderInfo *coderInfo,
						BitStream *bitStream,
						int writeFlag)
{
	int grouping_bits;
	int bits = 0;

	if (writeFlag) {
		/* write out ics_info() information */
		PutBit(bitStream, 0, LEN_ICS_RESERV);  /* reserved Bit*/

		/* Write out window sequence */
		PutBit(bitStream, coderInfo->block_type, LEN_WIN_SEQ);  /* block type */

		/* Write out window shape */
		PutBit(bitStream, coderInfo->window_shape, LEN_WIN_SH);  /* window shape */
	}

	bits += LEN_ICS_RESERV;
	bits += LEN_WIN_SEQ;
	bits += LEN_WIN_SH;

	/* For short windows, write out max_sfb and scale_factor_grouping */
	if (coderInfo->block_type == ONLY_SHORT_WINDOW){
		if (writeFlag) {
			PutBit(bitStream, coderInfo->max_sfb, LEN_MAX_SFBS);
			grouping_bits = FindGroupingBits(coderInfo);
			PutBit(bitStream, grouping_bits, MAX_SHORT_WINDOWS - 1);  /* the grouping bits */
		}
		bits += LEN_MAX_SFBS;
		bits += MAX_SHORT_WINDOWS - 1;
	} else { /* Otherwise, write out max_sfb and predictor data */
		if (writeFlag) {
			PutBit(bitStream, coderInfo->max_sfb, LEN_MAX_SFBL);
		}
		bits += LEN_MAX_SFBL;
		bits += WriteLTPPredictorData(coderInfo, bitStream, writeFlag);
		/* bits += WritePredictorData(coderInfo, bitStream, writeFlag); */
	}

	return bits;
}

static int WriteICS(CoderInfo *coderInfo,
					BitStream *bitStream,
					int commonWindow,
					int writeFlag)
{
	/* this function writes out an individual_channel_stream to the bitstream and */
	/* returns the number of bits written to the bitstream */
	int bits = 0;

	/* Write the 8-bit global_gain */
	if (writeFlag)
		PutBit(bitStream, coderInfo->global_gain, LEN_GLOB_GAIN);
	bits += LEN_GLOB_GAIN;

	/* Write ics information */
	if (!commonWindow) {
		bits += WriteICSInfo(coderInfo, bitStream, writeFlag);
	}

	bits += SortBookNumbers(coderInfo, bitStream, writeFlag);
	bits += WriteScalefactors(coderInfo, bitStream, writeFlag);
	bits += WritePulseData(coderInfo, bitStream, writeFlag);
	bits += WriteTNSData(coderInfo, bitStream, writeFlag);
	bits += WriteGainControlData(coderInfo, bitStream, writeFlag);
	bits += WriteSpectralData(coderInfo, bitStream, writeFlag);

	/* Return number of bits */
	return bits;
}

static int WriteLTPPredictorData(CoderInfo *coderInfo,
								 BitStream *bitStream,
								 int writeFlag)
{
	int bits = 0;

	if(writeFlag)
		PutBit (bitStream, 0, 1);  /* LTP not used */

	bits += 1;

	return bits;
}

static int WritePulseData(CoderInfo *coderInfo,
						  BitStream *bitStream,
						  int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		PutBit(bitStream, 0, LEN_PULSE_PRES);  /* no pulse_data_present */
	}

	bits += LEN_PULSE_PRES;

	return bits;
}

static int WriteTNSData(CoderInfo *coderInfo,
						BitStream *bitStream,
						int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		PutBit(bitStream, 0, LEN_TNS_PRES);
	}

	bits += LEN_TNS_PRES;

	return bits;
}

static int WriteGainControlData(CoderInfo *coderInfo,
								BitStream *bitStream,
								int writeFlag)
{
	int bits = 0;

	if (writeFlag) {
		PutBit(bitStream, 0, LEN_GAIN_PRES);
	}

	bits += LEN_GAIN_PRES;

	return bits;
}

static int WriteSpectralData(CoderInfo *coderInfo,
							 BitStream *bitStream,
							 int writeFlag)
{
	int i, bits = 0;

	/* set up local pointers to data and len */
	/* data array contains data to be written */
	/* len array contains lengths of data words */
	int* data = coderInfo->data;
	int* len  = coderInfo->len;

	if (writeFlag) {
		for(i = 0; i < coderInfo->spectral_count; i++) {
			if (len[i] > 0) {  /* only send out non-zero codebook data */
				PutBit(bitStream, data[i], len[i]); /* write data */
				bits += len[i];
			}
		}
	} else {
		for(i = 0; i < coderInfo->spectral_count; i++) {
			bits += len[i];
		}
	}

	return bits;
}

static int WriteAACFillBits(BitStream* bitStream,
							int numBits,
							int writeFlag)
{
	int numberOfBitsLeft = numBits;

	/* Need at least (LEN_SE_ID + LEN_F_CNT) bits for a fill_element */
	int minNumberOfBits = LEN_SE_ID + LEN_F_CNT;

	while (numberOfBitsLeft >= minNumberOfBits)
	{
		int numberOfBytes;
		int maxCount;

		if (writeFlag) {
			PutBit(bitStream, ID_FIL, LEN_SE_ID);	/* Write fill_element ID */
		}
		numberOfBitsLeft -= minNumberOfBits;	/* Subtract for ID,count */

		numberOfBytes = (int)(numberOfBitsLeft/LEN_BYTE);
		maxCount = (1<<LEN_F_CNT) - 1;  /* Max count without escaping */

		/* if we have less than maxCount bytes, write them now */
		if (numberOfBytes < maxCount) {
			int i;
			if (writeFlag) {
				PutBit(bitStream, numberOfBytes, LEN_F_CNT);
				for (i = 0; i < numberOfBytes; i++) {
					PutBit(bitStream, 0, LEN_BYTE);
				}
			}
			/* otherwise, we need to write an escape count */
		}
		else {
			int maxEscapeCount, maxNumberOfBytes, escCount;
			int i;
			if (writeFlag) {
				PutBit(bitStream, maxCount, LEN_F_CNT);
			}
			maxEscapeCount = (1<<LEN_BYTE) - 1;  /* Max escape count */
			maxNumberOfBytes = maxCount + maxEscapeCount;
			numberOfBytes = (numberOfBytes > maxNumberOfBytes ) ? (maxNumberOfBytes) : (numberOfBytes);
			escCount = numberOfBytes - maxCount;
			if (writeFlag) {
				PutBit(bitStream, escCount, LEN_BYTE);
				for (i = 0; i < numberOfBytes-1; i++) {
					PutBit(bitStream, 0, LEN_BYTE);
				}
			}
		}
		numberOfBitsLeft -= LEN_BYTE*numberOfBytes;
	}

	return numberOfBitsLeft;
}

static int FindGroupingBits(CoderInfo *coderInfo)
{
	/* This function inputs the grouping information and outputs the seven bit 
	'grouping_bits' field that the AAC decoder expects.  */

	int grouping_bits = 0;
	int tmp[8];
	int i, j;
	int index = 0;

	for(i = 0; i < coderInfo->num_window_groups; i++){
		for (j = 0; j < coderInfo->window_group_length[i]; j++){
			tmp[index++] = i;
		}
	}

	for(i = 1; i < 8; i++){
		grouping_bits = grouping_bits << 1;
		if(tmp[i] == tmp[i-1]) {
			grouping_bits++;
		}
	}
	
	return grouping_bits;
}

/* size in bytes! */
BitStream *OpenBitStream(int size, unsigned char *buffer)
{
	BitStream *bitStream;

	bitStream = malloc(sizeof(BitStream));
	bitStream->size = size;
	bitStream->numBit = 0;
	bitStream->currentBit = 0;
	bitStream->data = buffer;
	memset(bitStream->data, 0, size);

	return bitStream;
}

int CloseBitStream(BitStream *bitStream)
{
	int bytes = bit2byte(bitStream->numBit);

	free(bitStream);

	return bytes;
}

static long BufferNumBit(BitStream *bitStream)
{
	return bitStream->numBit;
}

static int WriteByte(BitStream *bitStream,
					 unsigned long data,
					 int numBit)
{
	long numUsed,idx;

	idx = (bitStream->currentBit / BYTE_NUMBIT) % bitStream->size;
	numUsed = bitStream->currentBit % BYTE_NUMBIT;
	if (numUsed == 0)
		bitStream->data[idx] = 0;
	bitStream->data[idx] |= (data & ((1<<numBit)-1)) <<
		(BYTE_NUMBIT-numUsed-numBit);
	bitStream->currentBit += numBit;
	bitStream->numBit = bitStream->currentBit;

	return 0;
}

int PutBit(BitStream *bitStream,
		   unsigned long data,
		   int numBit)
{
	int num,maxNum,curNum;
	unsigned long bits;

	if (numBit == 0)
		return 0;

	/* write bits in packets according to buffer byte boundaries */
	num = 0;
	maxNum = BYTE_NUMBIT - bitStream->currentBit % BYTE_NUMBIT;
	while (num < numBit) {
		curNum = min(numBit-num,maxNum);
		bits = data>>(numBit-num-curNum);
		if (WriteByte(bitStream, bits, curNum)) {
			return 1;
		}
		num += curNum;
		maxNum = BYTE_NUMBIT;
	}

	return 0;
}

static int ByteAlign(BitStream *bitStream, int writeFlag)
{
	int len, i,j;

	len = BufferNumBit(bitStream);
	   
	j = (8 - (len%8))%8;

	if ((len % 8) == 0) j = 0;
	if (writeFlag) {
		for( i=0; i<j; i++ ) {
			PutBit(bitStream, 0, 1);
		}
	}
	return j;
}
