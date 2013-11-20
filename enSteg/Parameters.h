/*
 *  Parameters.h
 *  APVD Embed
 *
 *  Created by Dasa Anand on 08/10/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#define MAX_CLUSTERS 4

char windowName[] = "enSteg"; 
char DESkey[]="AGio03!";

int noOfLSB = 2, nChannels;

uint16_t lfsr1 = 0xE1u;
uint16_t lfsr2 = 0xABu;
