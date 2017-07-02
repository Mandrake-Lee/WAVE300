/*****************************************************************************
*
* MT_UTIL.H
*
* BCL's utility functions for string and numbers manipulation
*
*****************************************************************************/
#include "mt_types.h"
#include "mt_bcl_defs.h"

/*****************************************************************************
*
* (C) Metalink Ltd - All rights are strictly reserved.
* Reproduction or divulgence in any form whatsoever is not permitted
* without written authority from the copyright owner.
* Issued by Metalink Transmission Devices Ltd in Israel.
*
*****************************************************************************/

/*****************************************************************************
*
*        Global Definitions, Data and Function Prototypes (of this module)
*
*****************************************************************************/


/*****************************************************************************
*
*       Local Function Prototypes
*
*****************************************************************************/
char *getLine(char *buff, int buff_len);
unsigned int asciiToUnsigned(char * pAscii, char delim, int *error_occured);
signed int asciiToSigned(char * pAscii, int *numConvErr);
char *skipDelimiters(char *str, char delim);
char *skipWord(char *str, char delim);
int wordsCount(char *str, char delim);
int compareFirstWordOnly(char *word1, char* word2, char delimiter);
int unsignedToAsciiHex(unsigned int number, char *dest);
int signedToAscii(signed int number, char *dest);
int unsignedToAscii(unsigned int number, char *dest);
char *strUpr(char *str, char delimiter);
int strCpy(char *dest, const char *source);
unsigned int strLen(const char *str);

MT_UINT32 showHelp(void);
void error(char *msg);
void mt_print(char level,char *fmt, ...);
MT_UINT32 PrintLevel(char level);

