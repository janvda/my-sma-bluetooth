/* tool to read power production data for SMA solar power convertors 
   Copyright Wim Hofman 2010 
   Copyright Stephen Collier 2010,2011 
   Copyright Edwin Zuidema 2020, 2021, 2022

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

// v2.0 Changed to UTC timezone
// v2.1 Fixed the return code upon successful retrieving inverter data
// v2.2 Fixed hanging on waiting for more BT data (Status 0e and no more data), removed fprintf to stderr, fixed sunset/rise

#define PROGRAM "smatool"
#define VERSION "2.2 (2022-03-06)"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/xpath.h>
#include "almanac.h"
#include "sb_commands.h"
#include "sma_mysql.h"

// u16 represents an unsigned 16-bit number.  Adjust the typedef for your hardware.
typedef u_int16_t u16;

#define PPPINITFCS16 0xffff /* Initial FCS value    */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */
#define ASSERT(x) assert(x)
#define SCHEMA "4"  /* Current database schema */


char *accepted_strings[] = {
"$END",
"$ADDR",
"$TIME",
"$SERIAL",
"$CRC",
"$POW",
"$DTOT",
"$ADD2",
"$CHAN",
"$ITIME",
"$TMMI",
"$TMPL",
"$TIMESTRING",
"$TIMEFROM1",
"$TIMETO1",
"$TIMEFROM2",
"$TIMETO2",
"$TESTDATA",
"$ARCHIVEDATA1",
"$PASSWORD",
"$SIGNAL",
"$SUSYID",
"$INVCODE",
"$ARCHCODE",
"$INVERTERDATA",
"$CNT",         /* Counter of sent packets*/
"$TIMEZONE",    /* Timezone seconds +1 from GMT*/
"$TIMESET",     /*Unknown string involved in time setting*/
"$DATA",        /*Data string */
"$MYSUSYID",
"$MYSERIAL",
"$LOGIN"
};

int cc;
unsigned char fl[1024] = { 0 };


static u16 fcstab[256] = {
   0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
   0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
   0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
   0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
   0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
   0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
   0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
   0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
   0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
   0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
   0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
   0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
   0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
   0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
   0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
   0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
   0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
   0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
   0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
   0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
   0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
   0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
   0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
   0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
   0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
   0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
   0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
   0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
   0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
   0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
   0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
   0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
 * Calculate a new fcs given the current fcs and the new data.
 */
u16 pppfcs16(u16 fcs, void *_cp, int len)
{
    register unsigned char *cp = (unsigned char *)_cp;
    /* don't worry about the efficiency of these asserts here.  gcc will
     * recognise that the asserted expressions are constant and remove them.
     * Whether they are usefull is another question. 
     */

    ASSERT(sizeof (u16) == 2);
    ASSERT(((u16) -1) > 0);
    while (len--)
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    return (fcs);
}

/*
 * Strip escapes (7D) as they aren't includes in fcs
 */
void
strip_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=0; i<(*len); i++ ) {
      if( cp[i] == 0x7d ) { /*Found escape character. Need to convert*/
        cp[i] = cp[i+1]^0x20;
        for( j=i+1; j<(*len)-1; j++ ) cp[j] = cp[j+1];
        (*len)--;
      }
   }
}

int quick_pow10(int n)
{
    static int pow10[10] = {
        1, 10, 100, 1000, 10000, 
        100000, 1000000, 10000000, 100000000, 1000000000
    };

    return pow10[n]; 
}

/*
 * Add escapes (7D) as they are required
 */
void add_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=19; i<(*len); i++ ) {
      switch( cp[i] ) {
        case 0x7d :
        case 0x7e :
        case 0x11 :
        case 0x12 :
        case 0x13 :
          for( j=(*len); j>i; j-- ) cp[j] = cp[j-1];
          cp[i+1] = cp[i]^0x20;
          cp[i]=0x7d;
          (*len)++;
      }
   }
}

/*
 * Recalculate and update length to correct for escapes
 */
void fix_length_send( FlagType * flag, unsigned char *cp, int *len)
{
  if( flag->debug == 1 ) printf( "sum=%x\n", cp[1]+cp[3] );
  if(( cp[1] != (*len)+1 )) {
    if( flag->debug == 1 ) printf( "length change from %x to %x diff=%x \n", cp[1],(*len)+1,cp[1]+cp[3] );
    cp[3] = (cp[1]+cp[3])-((*len)+1);
    cp[1] =(*len)+1;
    cp[3]=cp[0]^cp[1]^cp[2];
    if( flag->debug == 1 ) printf( "new sum=%x\n", cp[1]+cp[3] );
  }
}

/*
 * Recalculate and update length to correct for escapes
 */
void fix_length_received(FlagType * flag, unsigned char *received, int *len)
{
  int sum;

  if( received[1] != (*len) )
  {
    sum = received[1]+received[3];
    if (flag->debug == 1) {
      printf( "sum=%x\n", sum );
      printf( "length change from %x to %x\n", received[1], (*len) );
    }
    if(( received[3] != 0x13 )&&( received[3] != 0x14 )) { 
      received[1] = (*len);
      switch( received[1] ) {
        case 0x52: received[3]=0x2c; break;
        case 0x5a: received[3]=0x24; break;
        case 0x66: received[3]=0x1a; break;
        case 0x6a: received[3]=0x14; break;
        default:  received[3]=sum-received[1]; break;
      }
    }
  }
}

/*
 * How to use the fcs
 */
void tryfcs16(FlagType * flag, unsigned char *cp, int len, unsigned char *fl, int * cc)
{
  u16 trialfcs;
  unsigned
  int i;
  unsigned char stripped[1024] = { 0 };

  memcpy( stripped, cp, len );
  /* add on output */
  if (flag->debug == 2) {
    printf("String to calculate FCS\n");	 
    for (i=0;i<len;i++) printf("%02x ",cp[i]);
    printf("\n\n");
  }	
  trialfcs = pppfcs16( PPPINITFCS16, stripped, len );
  trialfcs ^= 0xffff;               /* complement */
  fl[(*cc)] = (trialfcs & 0x00ff);  /* least significant byte first */
  fl[(*cc)+1] = ((trialfcs >> 8) & 0x00ff);
  (*cc)+=2;
  if (flag->debug == 2 ) { 
    printf("FCS = %x%x %x\n",(trialfcs & 0x00ff),((trialfcs >> 8) & 0x00ff), trialfcs); 
  }
}


unsigned char conv(char *nn)
{
	unsigned char tt=0,res=0;
	int i;   
	
	for(i=0;i<2;i++){
		switch(nn[i]){

		case 65: /*A*/
		case 97: /*a*/
		tt = 10;
		break;

		case 66: /*B*/
		case 98: /*b*/
		tt = 11;
		break;

		case 67: /*C*/
		case 99: /*c*/
		tt = 12;
		break;

		case 68: /*D*/
		case 100: /*d*/
		tt = 13;
		break;

		case 69: /*E*/
		case 101: /*e*/
		tt = 14;
		break;

		case 70: /*F*/
		case 102: /*f*/
		tt = 15;
		break;


		default:
		tt = nn[i] - 48;
		}
		res = res + (tt * pow(16,1-i));
		}
		return res;
}

int empty_read_bluetooth(  ConfType * conf, FlagType * flag, ReadRecordType * readRecord, int *s, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated )
{
  int bytes_read,i,j, last_decoded;
  unsigned char buf[1024]; /*read buffer*/
  unsigned char header[4]; /*read buffer*/
  struct timeval tv;
  fd_set readfds;

  tv.tv_sec = 0; // just cleaning BT before sending, so no timeout 
  tv.tv_usec = 0;
  memset(buf,0,1024);

  FD_ZERO(&readfds);
  FD_SET((*s), &readfds);

  if( select((*s)+1, &readfds, NULL, NULL, &tv) <  0) {
    printf("ERROR: select error has occurred\n");
  }
  (*terminated) = 0; // Tag to tell if string has 7e termination
  // first read the header to get the record length
  if (FD_ISSET((*s), &readfds)) { // did we receive anything in the mean time?
    bytes_read = recv((*s), header, sizeof(header), 0); //Get length of string
    (*rr) = 0;
    for( i=0; i<sizeof(header); i++ ) {
      received[(*rr)] = header[i];
      if (flag->debug == 1) printf("%02x ", received[i]);
      (*rr)++;
    }
  } else {
    // No problem if no data waiting, just clearing BT anyway
    if (flag->debug == 1) printf("Timeout reading bluetooth socket (empty_read_bluetooth, header)\n");
    memset(received,0,1024);
    (*rr)=0;
    return -1;
  }
  if (FD_ISSET((*s), &readfds)) { // did we receive anything within 5 seconds
    bytes_read = recv((*s), buf, header[1]-3, 0); //Read the length specified by header
  } else {
    printf("ERROR: Timeout reading bluetooth socket (empty_read_bluetooth, body)\n");
    memset(received,0,1024);
    (*rr)=0;
    return -1;
  }
  readRecord->Status[0]=0;
  readRecord->Status[1]=0;
  if ( bytes_read > 0) {
    if (flag->debug == 1){ 
      printf( "\n-----------------------------------------------------------" );
      printf( "\nREAD:");
      //Start byte
      printf("\n7e "); j++;
      //Size and checkbit
      printf("%02x ",header[1]);
      printf("                      size:              %d", header[1] );
      printf("\n   " );
      printf("%02x ",header[2]);
      printf("\n   " );
      printf("%02x ",header[3]);
      printf("                      checkbit:          %d", header[3] );
      printf("\n   " );
      //Source Address
      for( i=0; i<bytes_read; i++ ) {
        if( i > 5 ) break;
        printf("%02x ",buf[i]);
      }
      printf("       source:            %02x:%02x:%02x:%02x:%02x:%02x", buf[5], buf[4], buf[3], buf[2], buf[1], buf[0] );
      printf("\n   " );
      //Destination Address
      for( i=6; i<bytes_read; i++ ) {
        if( i > 11 ) break;
        printf("%02x ",buf[i]);
      }
      printf("       destination:       %02x:%02x:%02x:%02x:%02x:%02x", buf[11], buf[10], buf[9], buf[8], buf[7], buf[6] );
      printf("\n   " );
      //Destination Address
      for( i=12; i<bytes_read; i++ ) {
        if( i > 13 ) break;
        printf("%02x ",buf[i]);
      }
      printf("                   control:           %02x%02x", buf[13], buf[12] );
      readRecord->Control[0]=buf[12];
      readRecord->Control[1]=buf[13];
           
      last_decoded=14;
      if( memcmp( buf+14, "\x7e\xff\x03\x60\x65", 5 ) == 0 ){
        printf("\n");
        for( i=14; i<bytes_read; i++ ) {
          if( i > 18 ) break;
          printf("%02x ",buf[i]);
        }
        printf("             SMA Data2+ header: %02x:%02x:%02x:%02x:%02x", buf[18], buf[17], buf[16], buf[15], buf[14] );
        printf("\n   " );
        for( i=19; i<bytes_read; i++ ) {
          if( i > 19 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                      data packet size:  %02d", buf[19] );
        printf("\n   " );
        for( i=20; i<bytes_read; i++ ) {
          if( i > 20 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                      control:           %02x", buf[20] );
        printf("\n   " );
        for( i=21; i<bytes_read; i++ ) {
          if( i > 26 ) break;
          printf("%02x ",buf[i]);
        }
        printf("       source:            %02x %02x:%02x:%02x:%02x:%02x", buf[21], buf[26], buf[25], buf[24], buf[23], buf[22] );
        printf("\n   " );
        for( i=27; i<bytes_read; i++ ) {
          if( i > 28 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                   read status:       %02x %02x", buf[28], buf[27] );
        printf("\n   " );
        for( i=29; i<bytes_read; i++ ) {
          if( i > 30 ) break;
          printf("%02x ",buf[i]);
        }
        readRecord->Status[0]=buf[28];
        readRecord->Status[1]=buf[27];
        printf("                   count up:          %02d %02x:%02x", buf[29]+buf[30]*256, buf[30], buf[29] );
        printf("\n   " );
        for( i=31; i<bytes_read; i++ ) {
          if( i > 32 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                   count down:        %02d %02x:%02x", buf[31]+buf[32]*256, buf[32], buf[31] );
        printf("\n   " );
        last_decoded=33;
      }
      printf("\n   " );
      j=0;
      for (i=last_decoded;i<bytes_read;i++) {
        if( j%16== 0 ) printf( "\n   %08x: ",j);
        printf("%02x ",buf[i]);
        j++;
      }
      printf(" rr=%d",(bytes_read+3));
      printf("\n\n");
    } // if debug
  } // if bytes read
  (*rr)=0;
  memset(received,0,1024);
  return 0;
}

int read_bluetooth( ConfType * conf, FlagType * flag, ReadRecordType * readRecord, int *s, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated )
{
  int bytes_read,i,j, last_decoded;
  unsigned char buf[1024]; /*read buffer*/
  unsigned char header[4]; /*read buffer*/
  unsigned char checkbit;
  struct timeval tv;
  fd_set readfds;

  tv.tv_sec = conf->bt_timeout; // set timeout of reading
  tv.tv_usec = 0;
  memset(buf,0,1024);

  FD_ZERO(&readfds);
  FD_SET((*s), &readfds);
      
  if( select((*s)+1, &readfds, NULL, NULL, &tv) <  0) {
    printf("ERROR: select error has occurred\n");
  }
      
  if(flag->debug == 1) printf("Reading bluetooth packet (socket=%d)\n", (*s));
  (*terminated) = 0; // Tag to tell if string has 7e termination
  // first read the header to get the record length
  if (FD_ISSET((*s), &readfds)) { // did we receive anything within 5 seconds
    bytes_read = recv((*s), header, sizeof(header), 0); //Get length of string
    (*rr) = 0;
    for( i=0; i<sizeof(header); i++ ) {
      received[(*rr)] = header[i];
      if (flag->debug == 2) printf("%02x ", received[i]);
      (*rr)++;
    }
  } else {
    printf("ERROR: Timeout reading bluetooth socket (read_bluetooth, header)\n");
    (*rr) = 0;
    memset(received,0,1024);
    return -1;
  }
  if (FD_ISSET((*s), &readfds)){ // did we receive anything within 5 seconds
    bytes_read = recv((*s), buf, header[1]-3, 0); //Read the length specified by header
  } else {
    printf("ERROR: Timeout reading bluetooth socket (read_bluetooth, body)\n");
    (*rr) = 0;
    memset(received,0,1024);
    return -1;
  }
  readRecord->Status[0]=0;
  readRecord->Status[1]=0;
  if ( bytes_read > 0) {
    if (flag->debug == 1) { 
      printf( "\n-----------------------------------------------------------" );
      printf( "\nREAD:");
      //Start byte
      printf("\n7e "); j++;
      //Size and checkbit
      printf("%02x ",header[1]);
      printf("                      size:              %d", header[1] );
      printf("\n   " );
      printf("%02x ",header[2]);
      printf("\n   " );
      printf("%02x ",header[3]);
      printf("                      checkbit:          %d", header[3] );
      printf("\n   " );
      //Source Address
      for( i=0; i<bytes_read; i++ ) {
        if( i > 5 ) break;
        printf("%02x ",buf[i]);
      }
      printf("       source:            %02x:%02x:%02x:%02x:%02x:%02x", buf[5], buf[4], buf[3], buf[2], buf[1], buf[0] );
      printf("\n   " );
      //Destination Address
      for( i=6; i<bytes_read; i++ ) {
        if( i > 11 ) break;
        printf("%02x ",buf[i]);
      }
      printf("       destination:       %02x:%02x:%02x:%02x:%02x:%02x", buf[11], buf[10], buf[9], buf[8], buf[7], buf[6] );
      printf("\n   " );
      //Destination Address
      for( i=12; i<bytes_read; i++ ) {
        if( i > 13 ) break;
        printf("%02x ",buf[i]);
      }
      printf("                   control:           %02x%02x", buf[13], buf[12] );
      readRecord->Control[0]=buf[12];
      readRecord->Control[1]=buf[13];
         
      last_decoded=14;
      if( memcmp( buf+14, "\x7e\xff\x03\x60\x65", 5 ) == 0 ) {
        printf("\n");
        for( i=14; i<bytes_read; i++ ) {
          if( i > 18 ) break;
          printf("%02x ",buf[i]);
        }
        printf("             SMA Data2+ header: %02x:%02x:%02x:%02x:%02x", buf[18], buf[17], buf[16], buf[15], buf[14] );
        printf("\n   " );
        for( i=19; i<bytes_read; i++ ) {
          if( i > 19 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                      data packet size:  %02d", buf[19] );
        printf("\n   " );
        for( i=20; i<bytes_read; i++ ) {
          if( i > 20 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                      control:           %02x", buf[20] );
        printf("\n   " );
        for( i=21; i<bytes_read; i++ ) {
          if( i > 26 ) break;
          printf("%02x ",buf[i]);
        }
        printf("       source:            %02x %02x:%02x:%02x:%02x:%02x", buf[21], buf[26], buf[25], buf[24], buf[23], buf[22] );
        printf("\n   " );
        for( i=27; i<bytes_read; i++ ) {
          if( i > 28 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                   read status:       %02x %02x", buf[28], buf[27] );
        printf("\n   " );
        for( i=29; i<bytes_read; i++ ) {
          if( i > 30 ) break;
          printf("%02x ",buf[i]);
        }
        readRecord->Status[0]=buf[28];
        readRecord->Status[1]=buf[27];
        printf("                   count up:          %02d %02x:%02x", buf[29]+buf[30]*256, buf[30], buf[29] );
        printf("\n   " );
        for( i=31; i<bytes_read; i++ ) {
          if( i > 32 ) break;
          printf("%02x ",buf[i]);
        }
        printf("                   count down:        %02d %02x:%02x", buf[31]+buf[32]*256, buf[32], buf[31] );
        printf("\n   " );
        last_decoded=33;
      } // if memcmp
      printf("\n   " );
      j=0;
      for (i=last_decoded;i<bytes_read;i++) {
        if( j%16== 0 ) printf( "\n   %08x: ",j);
        printf("%02x ",buf[i]);
        j++;
      }
      printf(" rr=%d",(bytes_read+3));
      printf("\n");
    } // if debug
    if ((cc==bytes_read)&&(memcmp(received,last_sent,cc) == 0)){
      printf("ERROR: received what we sent!\n");
      //Need to do something
      // EZ added:
      return -1;
    }
    // Check check bit
    checkbit=header[0]^header[1]^header[2];
    if( checkbit != header[3] ) {
      printf("ERROR: Checkbit Error! %02x!=%02x\n",  header[0]^header[1]^header[2], header[3]);
      (*rr) = 0;
      memset(received,0,1024);
      return -1;
    }
    if( buf[ bytes_read-1 ] == 0x7e )
      (*terminated) = 1;
    else
      (*terminated) = 0;
    for (i=0;i<bytes_read;i++) { //start copy the rec buffer in to received
      if (buf[i] == 0x7d) { //did we receive the escape char
        switch (buf[i+1]) {   // act depending on the char after the escape char
          case 0x5e :
            received[(*rr)] = 0x7e;
            break;
          case 0x5d :
            received[(*rr)] = 0x7d;
            break;
          default :
            received[(*rr)] = buf[i+1] ^ 0x20;
            break;
        }
        i++;
      } else { 
        received[(*rr)] = buf[i];
      }
      if (flag->debug == 2) printf("%02x ", received[(*rr)]);
      (*rr)++;
    }
    fix_length_received( flag, received, rr );
    if (flag->debug == 2) {
      printf("\n");
      for( i=0;i<(*rr); i++ ) printf("%02x ", received[(i)]);
    }
    if (flag->debug == 1) printf("\n\n");
  } // if bytes read
  return 0;
}

int select_str(FlagType * flag, char *s)
{
  int i;
  for (i=0; i < sizeof(accepted_strings)/sizeof(*accepted_strings);i++) {
     if (flag->debug == 2) printf( "\ni=%d accepted=%s string=%s", i, accepted_strings[i], s );
     if (!strcmp(s, accepted_strings[i])) {
     if (flag->debug == 1) printf( "Accepted %s, returning code %d\n", accepted_strings[i], i);
       return i;
     }
  }
  return -1;
}

unsigned char *  get_timezone_in_seconds( FlagType * flag, unsigned char *tzhex )
{
  time_t curtime;
  struct tm *loctime;
  struct tm *utctime;
  int day,month,year,hour,minute,isdst;
  float localOffset;
  int	 tzsecs;

  curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
  loctime = localtime(&curtime);
  day = loctime->tm_mday;
  month = loctime->tm_mon +1;
  year = loctime->tm_year + 1900;
  hour = loctime->tm_hour;
  minute = loctime->tm_min; 
  isdst  = loctime->tm_isdst;
  utctime = gmtime(&curtime);

  if( flag->debug == 1 ) printf( "utc=%04d-%02d-%02d %02d:%02d this machine local=%04d-%02d-%02d %02d:%02d diff %d hours\n", utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, year, month, day, hour, minute, hour-utctime->tm_hour );
  localOffset=(hour-utctime->tm_hour)+(float)(minute-utctime->tm_min)/60;
  if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
    localOffset+=24;
  if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
    localOffset-=24;
  if( isdst > 0 ) 
    localOffset=localOffset-1;
  if( flag->debug == 1 ) printf( "localOffset=%0.2f (hours), includes dst=%d\n", localOffset, isdst );
  tzsecs = (localOffset) * 3600 + 1;
  if( tzsecs < 0 )
    tzsecs=65536+tzsecs;
  if( flag->debug == 1 ) printf( "tzsecs=%d (dec) %x (hex)\n", tzsecs, tzsecs );
  tzhex[1] = tzsecs/256;
  tzhex[0] = tzsecs -(tzsecs/256)*256;
  if( flag->debug == 1 ) printf( "tzsecs=%02x %02x (2 bytes hex)\n", tzhex[1], tzhex[0] );

  return tzhex;
}

int auto_set_dates( ConfType * conf, FlagType * flag )
/*  If there are no dates set - get last updated date and go from there to NOW (UTC) */
{
  MYSQL_ROW row;
  char SQLQUERY[200];
  time_t curtime;
  int day,month,year,hour,minute;
  struct tm *utctime;

  if( flag->mysql == 1 ) {
    OpenMySqlDatabase( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
    //Get last updated value
    sprintf(SQLQUERY,"SELECT DATE_FORMAT( DateTime, \"%%Y-%%m-%%d %%H:%%i:%%S\" ) FROM DayData WHERE 1 ORDER BY DateTime DESC LIMIT 1" );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
    if ((row = mysql_fetch_row(res)))  //if there is a result, update the row
      strcpy( conf->datefrom, row[0] );
    mysql_free_result( res );
    mysql_close(conn);
  }
  if( strlen( conf->datefrom ) == 0 ) {
    strcpy( conf->datefrom, "2000-01-01 00:00:00" );
    if( flag->debug == 1 ) printf( "datefrom %s\n", conf->datefrom);
  }
  curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
  // EZ fixed to UTC
  utctime = gmtime(&curtime);
  day = utctime->tm_mday;
  month = utctime->tm_mon +1;
  year = utctime->tm_year + 1900;
  hour = utctime->tm_hour;
  minute = utctime->tm_min;
  if( flag->debug == 1 ) printf( "utc=%04d-%02d-%02d %02d:%02d\n", year, month, day, hour, minute);
  sprintf( conf->dateto, "%04d-%02d-%02d %02d:%02d:00", year, month, day, hour, minute );
  flag->daterange=1;
  if( flag->verbose == 1 ) printf( "Auto set dates: from %s to %s (UTC)\n", conf->datefrom, conf->dateto );
  return 1;
}

int is_light( ConfType * conf, FlagType * flag )
/*  Check if all data done and past sunset or before sunrise */
{
  int light=1;
  MYSQL_ROW row;
  char SQLQUERY[200];

  if( flag->mysql == 1 ) {
    OpenMySqlDatabase( conf->MySqlHost, conf->MySqlUser, conf->MySqlPwd, conf->MySqlDatabase);
    //Get Start of day value, all in local time
    sprintf(SQLQUERY,"SELECT if(sunrise < NOW(),1,0) FROM Almanac WHERE date= DATE_FORMAT( NOW(), \"%%Y-%%m-%%d\" ) " );
    if (flag->debug == 1) printf("%s\n",SQLQUERY);
    DoQuery(SQLQUERY);
    if ((row = mysql_fetch_row(res)))
      if( atoi( (char *)row[0] ) == 0 ) {
        if (flag->debug == 1) printf("Before sunrise\n");
        light=0;
      }
    // light if we are after sunrise. Now check if before sunset
    if( light ) {
// EZ too complex???
//      sprintf(SQLQUERY,"SELECT if( dd.datetime > al.sunset,1,0) FROM DayData as dd left join Almanac as al on al.date=DATE(dd.datetime) and al.date=DATE(NOW()) WHERE 1 ORDER BY dd.datetime DESC LIMIT 1" );
      sprintf(SQLQUERY,"SELECT if(sunset > NOW(),1,0) FROM Almanac WHERE date= DATE_FORMAT( NOW(), \"%%Y-%%m-%%d\" ) " );
      if (flag->debug == 1) printf("%s\n",SQLQUERY);
      DoQuery(SQLQUERY);
      if ((row = mysql_fetch_row(res)))
        if( atoi( (char *)row[0] ) == 0 ) {
          if (flag->debug == 1) printf("After sunset\n");
          light=0;
        }
    }
    mysql_close(conn);
  }
  if( flag->debug == 1 ) printf( "light = %d\n", light);
  return light;
}

//Set a value depending on inverter
void  SetInverterType( ConfType * conf, UnitType ** unit )  
{
  srand(time(NULL));
  unit[0]->SUSyID[0] = 0xFF;
  unit[0]->SUSyID[1] = 0xFF;
  conf->MySUSyID[0] = rand()%254;
  conf->MySUSyID[1] = rand()%254;
  conf->MySerial[0] = rand()%254;
  conf->MySerial[1] = rand()%254;
  conf->MySerial[2] = rand()%254;
  conf->MySerial[3] = rand()%254;
}

//Convert a received string to a value
long ConvertStreamtoLong( unsigned char * stream, int length, unsigned long long  * value )
{
   int	i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//Convert a recieved string to a value
float ConvertStreamtoFloat( unsigned char * stream, int length, float * value )
{
   int	i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//Convert a recieved string to a value
char * ConvertStreamtoString( unsigned char * stream, int length )
{
  int i, j=0, nullvalue=1;
  char * value;

  value = malloc( sizeof(char)*length+1 );
  for( i=0; i < length; i++ ) {
    if( stream[i] != 0xff ) //check if all ffs which is a null value 
      nullvalue = 0;
    if( stream[i] != 0 )
      value[i] = stream[i];
    else
      break;
  }
  if( nullvalue == 1 )
    (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
  else {
    value[i]= 0; //string null termination
  }
  return (char *)value;
}

//read return value data from init file
ReturnType * InitReturnKeys( ConfType * conf )
{
  FILE *fp;
  char line[400];
  ReturnType tmp;
  ReturnType *returnkeylist;
  int num_return_keys=0;
  int data_follows=0;

  fp=fopen(conf->File,"r");
  if( fp == NULL ) {
    printf("ERROR: Couldn't open file %s, error = %s\n", conf->File, strerror( errno ));
    exit(1);
  } else {
    returnkeylist=(ReturnType *)malloc(sizeof(ReturnType));
    while (!feof(fp)) {
      if (fgets(line,400,fp) != NULL){ //read line from smatool.conf
        if( line[0] != '#' ) {
          if( strncmp( line, ":unit conversions", 17 ) == 0 )
            data_follows = 1;
          if( strncmp( line, ":end unit conversions", 21 ) == 0 )
            data_follows = 0;
          if( data_follows == 1 ) {
            tmp.key1=0x0;
            tmp.key2=0x0;
            strcpy( tmp.description, "" ); //Null out value
            strcpy( tmp.units, "" ); //Null out value
            tmp.divisor=0;
            tmp.decimal=0;
            tmp.datalength=0;
            tmp.recordgap=0;
            tmp.persistent=1;
            if( sscanf( line, "%x %x \"%[^\"]\" \"%[^\"]\" %d %d %d %d", &tmp.key1, &tmp.key2, tmp.description, tmp.units, &tmp.decimal, &tmp.recordgap, &tmp.datalength, &tmp.persistent ) == 8 ) {
              if( (num_return_keys) != 0 )
                returnkeylist=(ReturnType *)realloc(returnkeylist,sizeof(ReturnType)*((num_return_keys)+1));
              (returnkeylist+(num_return_keys))->key1=tmp.key1;
              (returnkeylist+(num_return_keys))->key2=tmp.key2;
              strcpy( (returnkeylist+(num_return_keys))->description, tmp.description );
              strcpy( (returnkeylist+(num_return_keys))->units, tmp.units );
              (returnkeylist+(num_return_keys))->decimal = tmp.decimal;
              (returnkeylist+(num_return_keys))->divisor = (float)pow( 10, tmp.decimal );
              (returnkeylist+(num_return_keys))->datalength = tmp.datalength;
              (returnkeylist+(num_return_keys))->recordgap = tmp.recordgap;
              (returnkeylist+(num_return_keys))->persistent = tmp.persistent;
              (num_return_keys)++;
            } else {
              if( line[0] != ':' )
                printf( "\nWARNING: Data Scan Failure\n %s\n", line );
            }
          } // if data followa
        } // if no comment #
      } // if new line
    } // while not EOF
    fclose(fp);
  } // fp <> 0
  conf->num_return_keys=num_return_keys;
  conf->returnkeylist=returnkeylist;
}

//Convert a received string to a value
int ConvertStreamtoInt( unsigned char * stream, int length, int * value )
{
   int	i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//Convert a received string to a value
time_t ConvertStreamtoTime( unsigned char * stream, int length, time_t * value, int *day, int *month, int *year, int *hour, int *minute, int *second )
{
   int	i, nullvalue;
   struct tm *utctime;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   else
   {
      //Get human readable dates
      utctime = gmtime(value);
      (*day) = utctime->tm_mday;
      (*month) = utctime->tm_mon +1;
      (*year) = utctime->tm_year + 1900;
      (*hour) = utctime->tm_hour;
      (*minute) = utctime->tm_min; 
      (*second) = utctime->tm_sec; 
   }
   return (*value);
}

// Set switches in flag variable to save lots of strcmps
void  SetSwitches( ConfType *conf, FlagType *flag )  
{
    //Check if all location variables are set
    if(( conf->latitude_f <= 180 )&&( conf->longitude_f <= 180 ))
        flag->location=1;
    else
        flag->location=0;
    //Check if all Mysql variables are set
    if(( strlen(conf->MySqlUser) > 0 )
	 &&( strlen(conf->MySqlPwd) > 0 )
	 &&( strlen(conf->MySqlHost) > 0 )
	 &&( strlen(conf->MySqlDatabase) > 0 )
	 &&( flag->test==0 ))
        flag->mysql=1;
    else
        flag->mysql=0;
    //Check if all File variables are set
    if( strlen(conf->File) > 0 )
        flag->file=1;
    else
        flag->file=0;
    if(( strlen(conf->datefrom) > 0 )&&( strlen(conf->dateto) > 0 ))
        flag->daterange=1;
    else
        flag->daterange=0;
}

unsigned char *ReadStream( ConfType * conf, FlagType * flag, ReadRecordType * readRecord, int * s, unsigned char * stream, int * streamlen, unsigned char * datalist, int * datalen, unsigned char * last_sent, int cc, int * terminated, int * togo )
{
  int finished;
  int finished_record;
  int i, j=0;

  (*togo)=ConvertStreamtoInt( stream+43, 2, togo );
  if(flag->debug == 2) printf( "togo=%d\n", (*togo) );
  i=59; //Initial position of data stream
  (*datalen)=0;
  datalist=(unsigned char *)malloc(sizeof(char));
  finished=0;
  finished_record=0;
  while( finished != 1 ) {
    datalist=(unsigned char *)realloc(datalist,sizeof(char)*((*datalen)+(*streamlen)-i));
    while( finished_record != 1 ) {
      if( i> 500 ) break; //Somthing has gone wrong
      if(( i < (*streamlen) )&&(( (*terminated) != 1)||(i+3 < (*streamlen) ))) {
        datalist[j]=stream[i];
        j++;
        (*datalen)=j;
        i++;
      } else
        finished_record = 1;
    }
    finished_record = 0;
    if( (*terminated) == 0 ) {
      if( read_bluetooth( conf, flag, readRecord, s, streamlen, stream, cc, last_sent, terminated ) != 0 ) {
		if( flag->debug== 1 ) printf("ReadStream error reading BT, freeing datalist");
        free( datalist );
        datalist = NULL;
      }
      if( j> 0 ) i=18;
    } else
      finished = 1;
  } // while not finished
  if( flag->debug== 1 ) {
    printf( "len=%d data=", (*datalen) );
    for( i=0; i< (*datalen); i++ )
      printf( "%02x ", datalist[i] );
    printf( "\n" );
  }
  return datalist;
}

/* Init Config to default values */
void InitConfig( ConfType *conf )
{
    strcpy( conf->Config,"/etc/smatool.conf");
    strcpy( conf->BTAddress, "" );  
    conf->bt_timeout = 5;
    strcpy( conf->Password, "0000" );  
    strcpy( conf->File, "/etc/sma.in" );  
    strcpy( conf->Xml, "/etc/smatool.xml" );  
    conf->latitude_f = 999 ;  
    conf->longitude_f = 999 ;  
    strcpy( conf->MySqlHost, "localhost" );  
    strcpy( conf->MySqlDatabase, "smatool" );  
    strcpy( conf->MySqlUser, "" );  
    strcpy( conf->MySqlPwd, "" );  
    strcpy( conf->datefrom, "" );  
    strcpy( conf->dateto, "" );  
}

/* Init Flags to default values */
void InitFlag( FlagType *flag )
{
    flag->debug=0;         /* debug flag */
    flag->verbose=0;       /* verbose flag */
    flag->daterange=0;     /* is system using a daterange */
    flag->location=0;     /* is system using a location */
    flag->test=0;     /* is system using a test */
    flag->mysql=0;     /* is system using MySQL */
    flag->file=0;     /* is system using a file for sma.in */
}

/* read Config from file */
int GetConfig( ConfType *conf, FlagType * flag )
{
    FILE 	*fp;
    char	line[400];
    char	variable[400];
    char	value[400];

    if (strlen(conf->Config) > 0 )
    {
        if(( fp=fopen(conf->Config,"r")) == (FILE *)NULL )
        {
           printf("ERROR: Could not open file %s\n", conf->Config );
           return( -1 ); //Could not open file
        }
    }
    else
    {
        if(( fp=fopen("./smatool.conf","r")) == (FILE *)NULL )
        {
           printf("ERROR: Could not open file ./smatool.conf\n" );
           return( -1 ); //Could not open file
        }
    }
    while (!feof(fp)){	
	if (fgets(line,400,fp) != NULL){				//read line from smatool.conf
            if( line[0] != '#' ) 
            {
                strcpy( value, "" ); //Null out value
                sscanf( line, "%s %s", variable, value );
                if( flag->debug == 1 ) printf( "variable=%s value=%s\n", variable, value );
                if( value[0] != '\0' )
                {
                    if( strcmp( variable, "BTAddress" ) == 0 )
                       strcpy( conf->BTAddress, value );  
                    if( strcmp( variable, "BTTimeout" ) == 0 )
                       conf->bt_timeout =  atoi(value);  
                    if( strcmp( variable, "Password" ) == 0 )
                       strcpy( conf->Password, value );  
                    if( strcmp( variable, "File" ) == 0 )
                       strcpy( conf->File, value );  
                    if( strcmp( variable, "Latitude" ) == 0 )
                       conf->latitude_f = atof(value) ;  
                    if( strcmp( variable, "Longitude" ) == 0 )
                       conf->longitude_f = atof(value) ;  
                    if( strcmp( variable, "MySqlHost" ) == 0 )
                       strcpy( conf->MySqlHost, value );  
                    if( strcmp( variable, "MySqlDatabase" ) == 0 )
                       strcpy( conf->MySqlDatabase, value );  
                    if( strcmp( variable, "MySqlUser" ) == 0 )
                       strcpy( conf->MySqlUser, value );  
                    if( strcmp( variable, "MySqlPwd" ) == 0 )
                       strcpy( conf->MySqlPwd, value );  
                }
            }
        }
    }
    fclose( fp );
    return( 0 );
}

xmlDocPtr getdoc (char *docname)
{
  xmlDocPtr doc;
  doc = xmlParseFile(docname);

  if (doc == NULL ) {
    printf("ERROR: XML Document not parsed successfully.\n");
    return NULL;
  }
  return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{
  xmlXPathContextPtr context;
  xmlXPathObjectPtr result;

  context = xmlXPathNewContext(doc);
  if (context == NULL) {
    printf("ERROR: No context in xmlXPathNewContext\n");
    return NULL;
  }
  result = xmlXPathEvalExpression(xpath, context);
  xmlXPathFreeContext(context);
  if (result == NULL) {
    printf("ERROR: No path in xmlXPathEvalExpression\n");
    return NULL;
  }
  if(xmlXPathNodeSetIsEmpty(result->nodesetval)) {
    xmlXPathFreeObject(result);
    printf("ERROR: Empty path nodes\n");
    return NULL;
  }
  return result;
}

int setup_xml_xpath( ConfType *conf, xmlChar * xpath, char * docname, int index )
{
  sprintf( xpath, "//Datamap/Map[@index='%d']", index );
  sprintf( docname, "%s", conf->Xml );
  return (1);
}

char * return_xml_data( ConfType *conf, int index )
{
  xmlDocPtr doc;
  xmlNodeSetPtr nodeset;
  xmlNodePtr cur;
  xmlXPathObjectPtr result;
  xmlChar xpath[30];
  char docname[400];
  char *return_string = (char *)NULL;
  int i;
  xmlChar *keyword;
  
  setup_xml_xpath( conf, xpath, docname, index );
  doc = getdoc(docname);
  result = getnodeset (doc, xpath);
  if (result) {
    nodeset = result->nodesetval;
    for (i=0; i < nodeset->nodeNr; i++) {
      cur = nodeset->nodeTab[i]->xmlChildrenNode;
      while (cur != NULL ) {
        if( xmlStrEqual(cur->name, (const xmlChar *)"Value")) {
          keyword = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
          return_string=malloc(sizeof(char)*strlen(keyword)+1);
          strcpy( return_string, keyword );
          xmlFree(keyword);
        }
        cur = cur->next;
      }
    }
    xmlXPathFreeObject (result);
  } else
    printf( "\nfailed to getnodeset" );
  xmlFreeDoc(doc);
  xmlCleanupParser();

  return (char *)return_string ;
}

/* Print a help message */
void PrintHelp()
{
    printf( "Usage: smatool [OPTION]\n" );
    printf( "  -v,  --verbose                           Give more verbose output\n" );
    printf( "  -d,  --debug                             Show debug\n" );
    printf( "  -c,  --config CONFIGFILE                 Set config file default smatool.conf\n" );
    printf( "       --test                              Run in test mode - don't update data\n" );
    printf( "\n" );
    printf( "Dates are no longer required - defaults to last update if using mysql\n" );
    printf( "or 2000 to now if not using mysql\n" );
    printf( "  -from  --datefrom YYYY-MM-DD HH:MM:00    Date range from date\n" );
    printf( "  -to  --dateto YYYY-MM-DD HH:MM:00        Date range to date\n" );
    printf( "\n" );
    printf( "The following options are in config file but may be overridden\n" );
    printf( "  -i,  --inverter INVERTER_MODEL           inverter model\n" );
    printf( "  -a,  --address INVERTER_ADDRESS          inverter BT address\n" );
    printf( "  -t,  --timeout TIMEOUT                   bluetooth timeout (secs) default 5\n" );
    printf( "  -p,  --password PASSWORD                 inverter user password default 0000\n" );
    printf( "  -f,  --file FILENAME                     command file default sma.in.new\n" );
    printf( "Location Information to calculate sunset and sunrise so inverter is not\n" );
    printf( "queried in the dark\n" );
    printf( "  -n,  --nodark                            force querying inverter\n" );
    printf( "  -lat,  --latitude LATITUDE               location latitude -180 to 180 deg\n" );
    printf( "  -lon,  --longitude LONGITUDE             location longitude -90 to 90 deg\n" );
    printf( "Mysql database information\n" );
    printf( "  -H,  --mysqlhost MYSQLHOST               mysql host default localhost\n");
    printf( "  -D,  --mysqldb MYSQLDATBASE              mysql database default smatool\n");
    printf( "  -U,  --mysqluser MYSQLUSER               mysql user\n");
    printf( "  -P,  --mysqlpwd MYSQLPASSWORD            mysql password\n");
    printf( "Mysql tables can be installed using INSTALL you may have to use a higher \n" );
    printf( "privelege user to allow the creation of databases and tables, use command line \n" );
    printf( "       --INSTALL                           install mysql data tables\n");
    printf( "       --UPDATE                            update mysql data tables\n");
    printf( "\n\n" );
}

/* Init Config to default values */
int ReadCommandConfig( ConfType *conf, FlagType *flag, int argc, char **argv, int * no_dark, int * install, int * update )
{
  int i;

  // these need validation checking at some stage TODO
  for (i=1;i<argc;i++) { //Read through passed arguments
    if ((strcmp(argv[i],"-v")==0)||(strcmp(argv[i],"--verbose")==0)) flag->verbose = 1;
    else if ((strcmp(argv[i],"-d")==0)||(strcmp(argv[i],"--debug")==0)) {
      flag->debug = 1;
      flag->verbose = 1;
    }
    else if ((strcmp(argv[i],"-c")==0)||(strcmp(argv[i],"--config")==0)) {
      i++;
      if(i<argc) {
        strcpy(conf->Config,argv[i]);
      }
    }
    else if (strcmp(argv[i],"--test")==0) flag->test=1;
    else if ((strcmp(argv[i],"-from")==0)||(strcmp(argv[i],"--datefrom")==0)) {
      i++;
      if(i<argc) {
        strcpy(conf->datefrom,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-to")==0)||(strcmp(argv[i],"--dateto")==0)) {
      i++;
      if(i<argc){
        strcpy(conf->dateto,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-a")==0)||(strcmp(argv[i],"--address")==0)){
      i++;
      if (i<argc){
        strcpy(conf->BTAddress,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-t")==0)||(strcmp(argv[i],"--timeout")==0)){
      i++;
      if (i<argc){
        conf->bt_timeout = atoi(argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-p")==0)||(strcmp(argv[i],"--password")==0)){
      i++;
      if (i<argc){
        strcpy(conf->Password,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-f")==0)||(strcmp(argv[i],"--file")==0)){
      i++;
      if (i<argc){
        strcpy(conf->File,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-n")==0)||(strcmp(argv[i],"--nodark")==0)) (*no_dark)=1;
    else if ((strcmp(argv[i],"-lat")==0)||(strcmp(argv[i],"--latitude")==0)){
      i++;
	    if(i<argc){
        conf->latitude_f=atof(argv[i]);
	    }
    }
    else if ((strcmp(argv[i],"-long")==0)||(strcmp(argv[i],"--longitude")==0)){
	    i++;
	    if(i<argc){
        conf->longitude_f=atof(argv[i]);
	    }
    }
    else if ((strcmp(argv[i],"-H")==0)||(strcmp(argv[i],"--mysqlhost")==0)){
      i++;
      if (i<argc){
        strcpy(conf->MySqlHost,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-D")==0)||(strcmp(argv[i],"--mysqlcwdb")==0)){
      i++;
      if (i<argc){
        strcpy(conf->MySqlDatabase,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-U")==0)||(strcmp(argv[i],"--mysqluser")==0)){
      i++;
      if (i<argc){
        strcpy(conf->MySqlUser,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-P")==0)||(strcmp(argv[i],"--mysqlpwd")==0)){
      i++;
      if (i<argc){
        strcpy(conf->MySqlPwd,argv[i]);
      }
    }
    else if ((strcmp(argv[i],"-h")==0) || (strcmp(argv[i],"--help") == 0 )) {
      PrintHelp();
      return( -1 );
    }
    else if (strcmp(argv[i],"--INSTALL")==0) (*install)=1;
    else if (strcmp(argv[i],"--UPDATE")==0) (*update)=1;
    else {
      printf("Bad Syntax\n\n" );
      for( i=0; i< argc; i++ )
        printf( "%s ", argv[i] );
      printf( "\n\n" );
      PrintHelp();
      return( -1 );
    }
  }
  return( 0 );
}

char * debugdate()
{
  time_t curtime;
  struct tm *tm;
  static char result[80];

  curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)	
  tm = localtime(&curtime);
  sprintf( result, "%4d-%02d-%02d %02d:%02d:%02d",
  1900+tm->tm_year,
  1+tm->tm_mon,
  tm->tm_mday,
  tm->tm_hour,
  tm->tm_min,
  tm->tm_sec );
  return result;
}

int main(int argc, char **argv)
{
  FILE *fp;
  ConfType conf;
  FlagType flag;
  int maximumUnits=1;
  UnitType *unit;
  unsigned char received[1024];
  int i,s;
  int install=0, update=0, no_dark=0;
  unsigned char tzhex[2] = { 0 };
  int result;
  char SQLQUERY[1024];
  struct tm *utctime;
  char datetime[40];
  int day,month,year,hour,minute,second;
  int archdatalen=0, livedatalen=0;
  ArchDataType *archdatalist=NULL;
  LiveDataType *livedatalist=NULL;

  char sunrise_time[6], sunset_time[6];

  printf("%s version %s\n", PROGRAM, VERSION);

  unit=(UnitType *)malloc( sizeof(UnitType) * maximumUnits);
  if( unit == NULL ) {
    printf("ERROR: Unable to allocate memory\n");
    exit(1);
  }
  memset(received,0,1024);

  // set config to defaults
  InitConfig( &conf );
  InitFlag( &flag );
  // read command arguments needed so can get config
  if( ReadCommandConfig( &conf, &flag, argc, argv, &no_dark, &install, &update ) < 0 ) {
    printf("ERROR: Unable to command line arguments\n");
    exit(1);
  }
  // read Config file
  if( GetConfig( &conf, &flag ) < 0 ) {
    printf("ERROR: Unable to read config file\n");
    exit(1);
  }
  // read command arguments  again - they overide config
  if( ReadCommandConfig( &conf, &flag, argc, argv, &no_dark ,&install, &update ) < 0 ) {
    printf("ERROR: Unable to command line arguments\n");
    exit(1);
  }
  // set switches used through the program
  SetSwitches( &conf, &flag );
  // Print all Conf settings after init
  if( flag.debug == 1) {
    printf("Configuration parameters:\n");
    printf("BTAddress = %s\n", conf.BTAddress);
    printf("bt_timeout = %d\n", conf.bt_timeout);
    printf("Password = %s\n", conf.Password);
    printf("Config = %s\n", conf.Config);
    printf("File = %s\n", conf.File);
    printf("Xml = %s\n", conf.Xml);
    printf("latitude = %f\n", conf.latitude_f);
    printf("longitude = %f\n", conf.longitude_f);
    printf("MySqlHost = %s\n", conf.MySqlHost);
    printf("MySqlDatabase = %s\n", conf.MySqlDatabase);
    printf("MySqlUser = %s\n", conf.MySqlUser);
    printf("MySqlPwd = %s\n", conf.MySqlPwd);
    printf("MySUSyID = %d %d\n", conf.MySUSyID[0], conf.MySUSyID[1]);
    printf("MySerial = %d %d %d %d\n", conf.MySerial[0], conf.MySerial[1], conf.MySerial[2], conf.MySerial[3]);
    printf("MyBTAddress = %d %d %d %d %d %d\n", conf.MyBTAddress[0], conf.MyBTAddress[1], conf.MyBTAddress[2], conf.MyBTAddress[3], conf.MyBTAddress[4], conf.MyBTAddress[5]);
    printf("NetID = %d\n", conf.NetID);
    printf("datefrom = %s\n", conf.datefrom);
    printf("dateto = %s\n", conf.dateto);
    printf("Switches:\n");
    printf("debug = %d\n", flag.debug);
    printf("verbose = %d\n", flag.verbose);
    printf("daterange = %d\n", flag.daterange);
    printf("location = %d\n", flag.location);
    printf("test = %d\n", flag.test);
    printf("mysql = %d\n", flag.mysql);
    printf("file = %d\n", flag.file);
  }
  // If asked for installing MySQL database structure
  if(( install==1 )&&( flag.mysql==1 )) {
    install_mysql_tables( &conf, &flag, SCHEMA );
    exit(0);
  }
  // If asked to update MySQL database structure
  if(( update==1 )&&( flag.mysql==1 )) {
    update_mysql_tables( &conf, &flag );
    exit(0);
  }
  // Get Return Value lookup from file
  InitReturnKeys( &conf );
  // Set value for inverter type
  SetInverterType( &conf, &unit );
  // Get Local Timezone offset in seconds
  get_timezone_in_seconds( &flag, tzhex );
  // Location based information to avoid quering Inverter in the dark
  if((flag.location==1)&&(flag.mysql==1)) {
    if( flag.debug == 1 ) printf( "Before todays Almanac\n" ); 
    if( ! todays_almanac( &conf, flag.debug ) ) {
      sprintf( sunrise_time, "%s", sunrise(&conf, flag.debug ));
      sprintf( sunset_time, "%s", sunset(&conf, flag.debug ));
      if( flag.verbose == 1) printf( "sunrise=%s sunset=%s (local time)\n", sunrise_time, sunset_time );
      update_almanac(  &conf, sunrise_time, sunset_time, flag.debug );
    }
  }
  if( flag.mysql==1 ) { 
    if( flag.debug == 1 ) printf( "Before Check Schema\n" ); 
    if( check_schema( &conf, &flag,  SCHEMA ) != 1 ) {
      printf("ERROR: Schema not correct\n");
      exit(1);
    }
  }
  if(flag.daterange==0 ) {
    //auto set the dates
    if( flag.debug == 1) printf( "Before auto_set_dates\n" ); 
    auto_set_dates( &conf, &flag);
  }
  if( flag.verbose == 1 ) printf( "QUERY RANGE from %s to %s (daterange = %d)\n", conf.datefrom, conf.dateto, flag.daterange );

  // Collect data from inverter
  if(flag.location==0||no_dark==1||is_light( &conf, &flag )) {
    if (flag.debug == 1) printf("Collecting data from inverter address %s\n",conf.BTAddress);
    //Connect to Inverter
    if ((s = ConnectSocket( &conf )) < 0 ) {
      printf("ERROR: Cannot connect to socket\n");
      exit(1);
    }
    // Read inverter codes
    if (flag.file == 1)
      fp=fopen(conf.File,"r");
    else {
      printf("ERROR: Cannot connect open inverter code file %s\n", conf.File);
      exit(1);
    }
    static const char * commands[] = {
        "init", "login", "typelabel", "startuptime",
        "getacvoltage", "getenergyproduction", "getspotdcpower", "getspotdcvoltage", "getspotacpower", "getgridfreq",
        "maxACPower", "maxACPowerTotal", "ACPowerTotal", "DeviceStatus", "getrangedata", "logoff",
        0
    };
    while(commands[i] && result >= 0) {
        if( flag.debug == 1) printf("Executing command %s\n", commands[i]);
        result = InverterCommand( commands[i], &conf, &flag, &unit, &s, fp, &archdatalist, &archdatalen, &livedatalist, &livedatalen );
        if (result < 0) printf("ERROR executing command %s\n", commands[i]);
        i++;
    }
  } else
    if( flag.verbose == 1) printf("Not waking up inverter\n");

  // Store in database
  if (result>=0 && flag.mysql==1) {
    if( flag.debug == 1) printf( "Before store in database\n" ); 
    // Connect to database
    OpenMySqlDatabase( conf.MySqlHost, conf.MySqlUser, conf.MySqlPwd, conf.MySqlDatabase );
    if(archdatalen > 0) printf( "Storing archive data (%d records)\n",  archdatalen);
    for( i=1; i<archdatalen; i++ ) { //Start at 1 as the first record is a dummy 
	  // Storing in Inverter timezone (mostly set to UTC)
      utctime = gmtime(&((archdatalist+i)->date));
      day = utctime->tm_mday;
      month = utctime->tm_mon +1;
      year = utctime->tm_year + 1900;
      hour = utctime->tm_hour;
      minute = utctime->tm_min;
      second = utctime->tm_sec;
      sprintf( datetime, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
      if( flag.debug == 1 ) printf( "utc datetime = %s\n", datetime);    
      sprintf(SQLQUERY,"INSERT INTO DayData ( DateTime, Inverter, Serial, CurrentPower, EtotalToday ) VALUES (\'%s\',\'%s\',%ld,%0.f, %.3f ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), CurrentPower=VALUES(CurrentPower), EtotalToday=VALUES(EtotalToday)",datetime, (archdatalist+i)->inverter, (archdatalist+i)->serial, (archdatalist+i)->current_value, (archdatalist+i)->accum_value );
      if (flag.debug == 1) printf("SQL Query: %s\n",SQLQUERY);
      DoQuery(SQLQUERY);
    }
    mysql_close(conn);

    // Update Mysql with live data
    if(livedatalen > 0) printf( "Storing live data (%d records)\n",  livedatalen); 
    live_mysql( &conf, &flag, livedatalist, livedatalen );
  }

  // Clean up data
  if( archdatalen > 0 )
    free( archdatalist );
  archdatalen=0;
  if( livedatalen > 0 )
    free( livedatalist );
  livedatalen=0;
  close(s);
  if( flag.verbose == 1) printf("Done (resultcode = %d).\n", result);
  return(result);
}
