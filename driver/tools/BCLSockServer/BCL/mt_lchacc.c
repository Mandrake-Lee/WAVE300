// mt_lchacc.cpp: implementation of the wireless LINUX driver chip access
//
//////////////////////////////////////////////////////////////////////


#include "mtlkinc.h"
#include "mt_types.h"
#include "mt_bcl.h"
#include "mt_util.h"
#include "mt_chacc.h"
#include "mt_bcl_funcs.h"
#include "mt_wapi.h"




#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/if.h>
#include <linux/wireless.h>


#define MT_REQUEST_GET          0
#define MT_REQUEST_SET          1


typedef struct
{
    char name[IFNAMSIZ];
} NETWORK_INTERFACE;

typedef struct
{
    unsigned long unit;
    unsigned long address;
    unsigned long size;
    unsigned long data[64];
} OID_BCL_REQUEST, *POID_BCL_REQUEST;

// Do not change this structure (synchronized with driver)
typedef struct
{
    MT_UINT32 mode;
    MT_UINT32 category;
    MT_UINT32 index;
    MT_UINT32 datalen;
} OID_BCL_DRV_DATA_EX_REQUEST, *POID_BCL_DRV_DATA_EX_REQUEST;

// Do not change these values (synchronized with driver)
typedef enum
{
    BclDrvModeCatInit  = 1,
    BclDrvModeCatFree  = 2,
    BclDrvModeNameGet  = 3,
    BclDrvModeValGet   = 4,
    BclDrvModeValSet   = 5
} BCL_DRV_DATA_EX_MODE;

typedef enum
{
    MibTypeByte       = 1,
    MibTypeUShort     = 2,
    MibTypeULong      = 3,
    MibTypeULongLong  = 4,
    MibTypeMacAddress = 5,
    MibTypeByteStream = 6,
    MibTypeSetOfBytes = 7,
    MibTypeEssid      = 8,

    MibTypeUndef = -1
} MIB_OBJECT_TYPE;

typedef struct
{
    unsigned long   ObjectID;
    MIB_OBJECT_TYPE ObjectType;
    unsigned long   Size;
    unsigned short  RequestStatus;          /* Output parameter */
    unsigned char   Data[1];
} OID_MIB_REQUEST, *POID_MIB_REQUEST;


typedef struct
{
    unsigned long opcode;
    unsigned long size;
    unsigned long action;
    unsigned long res0;
    unsigned long res1;
    unsigned long res2;
    unsigned long retStatus;
    unsigned long data[64];
} OID_GENERIC_MAC_REQUEST, *POID_GENERIC_MAC_REQUEST;

typedef struct
{
    unsigned long   ID;
    unsigned long   action;
    unsigned long   value;
    unsigned long   size;
    unsigned long   offset;
    unsigned long  requestStatus;          /* Output parameter */
    unsigned char   data[256];
} OID_DEBUG_TABLE_REQUEST, *POID_DEBUG_TABLE_REQUEST;


int gsocket = -1;

int gifcount = 0;
NETWORK_INTERFACE gifs[32];

int SetStopLowerMac = 1;

#define FIELD_OFFSET(type, field) ((long)(long*)&(((type *)0)->field))

/*********************************************************************************
***                                                                            ***
***                            Local Definitions                               ***
***                                                                            ***
**********************************************************************************/


/*********************************************************************************
***                                                                            ***
***                            Local Functions Definition                      ***
***                                                                            ***
**********************************************************************************/
MT_UINT32 HypInit(void);

void add_interface( char* ifname )
{
  strncpy(gifs[gifcount++].name, ifname, IFNAMSIZ);
}

static int find_interfaces( void )
{
    FILE *f;
    char buffer[1024];
    char ifname[IFNAMSIZ];
    char *tmp, *tmp2;
    struct iwreq req;

    f = fopen( "/proc/net/dev", "r" );
    if (!f)
    {
        perror( "fopen()" );
        return -1;
    }

    // Skipping first two lines...
    if (!fgets( buffer, 1024, f ))
    {
        perror( "fgets()" );
        fclose( f );
        return -1;
    }

    if (!fgets( buffer, 1024, f ))
    {
        perror( "fgets()" );
        fclose( f );
        return -1;
    }

    // Reading interface descriptions...
    while (fgets( buffer, 1024, f ))
    {

        memset( ifname, 0, IFNAMSIZ );

        // Skipping through leading space characters...
        tmp = buffer;
        while (*tmp == ' ')
        {
            tmp++;
        }

        // Getting interface name...
        tmp2 = strstr( tmp, ":" );
        strncpy( ifname, 
                 tmp,
                 tmp2 - tmp > IFNAMSIZ ? IFNAMSIZ : tmp2 - tmp );

        mt_print(2, "Found network interface \"%s\"\n", ifname );

        // Checking if interface supports wireless extensions...
        strncpy( req.ifr_name, ifname, IFNAMSIZ );
        if (ioctl( gsocket, SIOCGIWNAME, &req ))
        {
            mt_print(2, "Interface \"%s\" has no wireless extensions...\n", ifname );
            continue;
        } 
        else
        {
            mt_print(2, "Interface \"%s\" has wireless extensions...\n", ifname );
        }

        // Adding interface into our database...
        add_interface(ifname);
    }

    fclose( f );

    return 0;
}



// Function name	: HypPciRead
// Description	    : perform reading from the hyperion through the PCI.
// Return type		: MT_UINT32 -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE unit - Hyperyon,Athena or Prometheus
// Argument         : MT_UINT32 address - start address to read from
// Argument         : MT_UBYTE size - amount of double words (32 bit) to read
// Argument         : MT_UINT32 * data - pointer to a buffer to read into
MT_UINT32 HypPciRead(MT_UBYTE unit, MT_UINT32 address, MT_UBYTE size, MT_UINT32 * data)
{
    struct iwreq req;
    OID_BCL_REQUEST *bclr;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    mt_print(2, "Reading unit = %d, addr 0x%lX, size=%d, ptr=0x%lx\n", unit, address, size, (MT_UINT32)data );

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    bclr = (OID_BCL_REQUEST *)malloc( sizeof( OID_BCL_REQUEST ) );
    if (bclr == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 1;
    }

    memset( bclr, 0, sizeof( OID_BCL_REQUEST ) );

    bclr->address = address;
    bclr->size = size;
    bclr->unit = unit;

    req.u.data.pointer = (caddr_t)bclr;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 20, &req ))
    {
        free( bclr );
        return 1;
    }

    memcpy( data, &bclr->data, size * sizeof( unsigned long ) );

    free( bclr );

    return 0;
}

// Function name	: HypPciWrite
// Description	    : perform writing to the hyperion through the PCI.
// Return type		: MT_UINT32 -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE unit - Hyperyon,Athena or Prometheus
// Argument         : MT_UINT32 address - start address to write  to
// Argument         : MT_UBYTE  size - amount of double words (32 bit) to write
// Argument         : MT_UINT32 * data - pointer to a buffer with the data to write
MT_UINT32 HypPciWrite(MT_UBYTE unit, MT_UINT32 address, MT_UBYTE size, MT_UINT32 * data)
{
    struct iwreq req;
    OID_BCL_REQUEST *bclr;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    mt_print(2, "Writing unit = %d, addr 0x%lX, size=%d, ptr=0x%lx, data[0]=0x%lx\n", unit, address, size, (MT_UINT32)data, data[0] );

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    bclr = (OID_BCL_REQUEST *)malloc( sizeof( OID_BCL_REQUEST ) );
    if (bclr == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 1;
    }

    memset( bclr, 0, sizeof( OID_BCL_REQUEST ) );
    bclr->address = address;
    bclr->size = size;
    bclr->unit = unit;
    memcpy( &bclr->data, data, size * sizeof( unsigned long ) );

    req.u.data.pointer = (caddr_t)bclr;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 21, &req ))
    {
        free( bclr );
        return 1;
    }

    free( bclr );

    return 0;
}




// Function name	: BCL_Init
// Description	    : called by the BCL engine when BCL is initialized
// Return type		: int - Non zero if init was successful
int BCL_Init(char* ifname)
{
    // first check for a new version
    int soc;

    soc = socket( AF_INET, SOCK_DGRAM, 0 );
    if (soc <= 0)
    {
        perror( "socket()" );
        return 0;
    }

    gsocket = soc;

    if(NULL != ifname)
    {
      add_interface(ifname);
    }

    if (find_interfaces() < 0)
    {
        close( soc );
        return 0;
    }

    return 1;
}



// Function name	: BCL_Exit
// Description	    : called by the BCL engine when BCL is exiting
// Return type		: none
void BCL_Exit()
{
    close( gsocket );
}


// Function name	: GetMibParams
// Description	    : perform reading MIB parameter through the PCI.
// Return type		: MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE ObjectID    - MIB parameter
// Argument         : MT_UBYTE ObjectType    - MIB type
// Argument         : MT_UBYTE size    - amount of double words (32 bit) to read
// Argument         : MT_UBYTE RequestStatus  - Return status
// Argument         : MT_UBYTE * data - pointer to a buffer to read into
// SCL command      : GETMIB ID Tyte Size Itemlist
MT_UINT32 GetMibParams(MT_UINT32 Id, MT_UBYTE type, MT_UINT32 size, MT_BYTE * data)
{
    struct iwreq req;
    OID_MIB_REQUEST *sMib;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 0;
    }

    mt_print(2, "Reading type = %d, size 0x%lX, ptr=0x%lx\n", type, size, (MT_UINT32)data );

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    sMib = (OID_MIB_REQUEST *)malloc( sizeof( OID_MIB_REQUEST ) + 255 /*64*4-1*/ );
    if (sMib == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 0;
    }

    memset( sMib, 0, sizeof( OID_MIB_REQUEST ) );

	sMib->ObjectID = Id;
    mt_print(2, "ObjectID = 0x%lx\n",sMib->ObjectID );
	sMib->ObjectType = type;
    sMib->Size = size;
    /* sMib->RequestStatus = RequestStatus; - only for return status */

    req.u.data.pointer = (caddr_t)sMib;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 24, &req ))
    {
        free( sMib );
        return 0;
    }

    if (sMib->RequestStatus == 0)
	    memcpy( data, &sMib->Data, sMib->Size/* * sizeof( unsigned long )*/ );
	else
        mt_print(2, "Error, Status = 0x%x\n",sMib->RequestStatus );

    if (type == 1)	
        mt_print(2, "data = 0x%x\n",*((MT_UBYTE*)data) );
    else if (type == 2)	
        mt_print(2, "data = 0x%x\n",*((MT_UINT16*)data) );
    else
        mt_print(2, "data = 0x%lx\n",*((MT_UINT32*)data) );
	
    mt_print(2, "Size = 0x%lx\n",sMib->Size );
    size = sMib->Size;

    free( sMib );

    return size;
}

// Function name	: SetMibParams
// Description	    : perform writing MIB parameter through the PCI.
// Return type		: MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE ObjectID    - MIB parameter
// Argument         : MT_UBYTE ObjectType    - MIB type
// Argument         : MT_UBYTE size    - amount of bytes to read
// Argument         : MT_UBYTE RequestStatus  - Return status
// Argument         : MT_UBYTE * data - pointer to a buffer to read from
// SCL command      : SETMIB ID Tyte Size Itemlist
MT_UINT32 SetMibParams(MT_UINT32 Id, MT_UBYTE type, MT_UINT32 size, MT_BYTE * data)
{
	struct iwreq req;
    OID_MIB_REQUEST *sMib;

	if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    mt_print(2, "Writing type = %d, size 0x%lX, ptr=0x%lx, data = 0x%x\n", type, size, (MT_UINT32)data, *data );

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    sMib = (OID_MIB_REQUEST *)malloc( (FIELD_OFFSET(OID_MIB_REQUEST,Data)+size) );
    if (sMib == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 1;
    }

    memset( sMib, 0, (FIELD_OFFSET(OID_MIB_REQUEST,Data)+size)/*sizeof( OID_MIB_REQUEST )*/ );
	sMib->ObjectID = Id;
	sMib->ObjectType = type;
    sMib->Size = size;

	/* sMib->RequestStatus = RequestStatus; - only for return status */
    memcpy( &sMib->Data, data, size );

    req.u.data.pointer = (caddr_t)sMib;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 25, &req ))
    {
        free( sMib );
        return 1;
    }
    if (sMib->RequestStatus != 0)
        mt_print(2, "Error, Status = 0x%x\n",sMib->RequestStatus );
    
    free( sMib );

    return 0;
}

// Function name	: SetMacCalibration
// Description	    : perform get ready to get MIB parameters. Followed "stop" lower MAC request.
// Return type		: MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// SCL command      : MACCALIBRATE - Enables MIB calibration on the fly
MT_UINT32 SetMacCalibration(void)
{
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    if (SetStopLowerMac == 1)
	{
        mt_print(2, "Set Stop Lower Mac\n" );
        SetStopLowerMac = 0;
	    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 26, &req ))
        {
			return 1;
        }
	}

    mt_print(2, "Set Mac Calibration\n" );
    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 27, &req ))
    {
        return 1;
    }

    return 0;
}


#define GENIOCTL_MAX_PARAMS 7

// Function name	: GenIoctl
// Description	    : perform reading MAC Variable through the PCI.
// Return type		: MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE opcode    - requested command
// Argument         : MT_UBYTE size    - amount of bytes to read
// Argument         : MT_UBYTE action  - Set/Get
// Argument         : MT_UBYTE res0/1/2  - for data transfer (instead of the data array)
// Argument         : MT_UBYTE retStatus  - status of MAC operation
// Argument         : MT_UINT32 * data - pointer to a buffer
// SCL command      : GENIOCTL opcode size action Itemlist
MT_UINT32 GenIoctl()
{
	char *params;
	int numConvErr;
    MT_UINT32 i, numOfParams, parameters[GENIOCTL_MAX_PARAMS];
    MT_UINT32 * pItem = (MT_UINT32 *)MT_BCL_item_list[0];
    
    struct iwreq req;
    OID_GENERIC_MAC_REQUEST *sIoctl;
	int retVal = 0;
	

    params = skipDelimiters(skipWord(MT_BCL_cmd_line,' '),' ');	/*params will now point to the first non-space  */
	            												/*character right after the DMEM_read command. */
	/* get the number of parameters */
    numOfParams = wordsCount(params,' ');
    if (numOfParams > GENIOCTL_MAX_PARAMS || numOfParams == 0)
    {
		error("Wrong number of arguments");
		return 1;
    }
	
	for (i=0;i<numOfParams;i++)
    {
	    parameters[i] = asciiToUnsigned(params,' ', &numConvErr);
        if(numConvErr)
        {
            strCpy(MT_BCL_cmd_line, "Invalid value entered for argument ");
            unsignedToAscii(i + 1, &MT_BCL_cmd_line[strLen(MT_BCL_cmd_line)]);
            return 1;
        }
		params = skipDelimiters( skipWord(params,' '),' ' );
    }

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
	
	sIoctl = (OID_GENERIC_MAC_REQUEST *)malloc( sizeof(OID_GENERIC_MAC_REQUEST) );
    if (sIoctl == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 1;
    }

    memset( sIoctl, 0, sizeof( OID_GENERIC_MAC_REQUEST ) );

    sIoctl->opcode = parameters[0];
    mt_print(2, "opcode = %ld\n",sIoctl->opcode );
	if (numOfParams > 3)
	{
        sIoctl->size   = parameters[1];
        sIoctl->action = parameters[2];
        mt_print(2, "size = %ld\n",sIoctl->size );
        mt_print(2, "action = %ld\n",sIoctl->action );
	    /* There is use of the optional fields */
		switch (numOfParams)
	    {
	        case 5:
                sIoctl->res0 = parameters[3];
                mt_print(2, "res0 = %ld\n",sIoctl->res0 );
		    break;
	        case 6:
                sIoctl->res0 = parameters[3];
                sIoctl->res1 = parameters[4];
                mt_print(2, "res0 = %ld, res1 = %ld\n",sIoctl->res0,sIoctl->res1 );
		    break;
	        case 7:
                sIoctl->res0 = parameters[3];
                sIoctl->res1 = parameters[4];
	            sIoctl->res2 = parameters[5];
                mt_print(2, "res0 = %ld, res1 = %ld, res2 = %ld\n",sIoctl->res0,sIoctl->res1, sIoctl->res2 );
		    break;
	    }
	}
	else if (numOfParams > 1)
	{
        sIoctl->size   = parameters[1];
        sIoctl->action = parameters[2];
        mt_print(2, "size = %ld, action = %ld\n",sIoctl->size,sIoctl->action );
	}
	if ((sIoctl->action < 2) && (parameters[numOfParams-1] < MT_BCL_ITEM_LISTS_NUM))
	{
	    pItem = (MT_UINT32 *)MT_BCL_item_list[parameters[numOfParams-1]];
        mt_print(2, "item = %d\n",parameters[numOfParams-1] );
	}

    if (sIoctl->action == MT_REQUEST_SET)
	{
	    memcpy( &sIoctl->data, pItem, 256  ); /*Set all*/
    }
    req.u.data.pointer = (caddr_t)sIoctl;
    /*Execute command*/
    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 28, &req ))
    {
        mt_print(2, "Error\n" );
        free( sIoctl );
		return 1;
    }

    if (sIoctl->retStatus == 0)
	    memcpy( pItem, &sIoctl->data, 256 ); /*Get all*/
	else
	{
        mt_print(2, "Error, retStatus = %lx\n",sIoctl->retStatus );
	    retVal = 1;
	}
		
    free( sIoctl );

    return retVal;
}

// Function name	: DebugTable
// Description	    : perform read/write in driver's variable table.
// Return type		: MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UBYTE ID    - index to variable
// Argument         : MT_UBYTE size    - amount of bytes to read/write
// Argument         : MT_UBYTE offset    - offset in variable
// Argument         : MT_UBYTE action    - Read or Write
// Argument         : MT_UBYTE value    - Read or Write
// Argument         : MT_UBYTE requestStatus  - Return status
// Argument         : MT_UBYTE * data - pointer to a buffer to read from
// SCL command      : DTABLE ID Tyte size Itemlist
MT_UINT32 DebugTable(MT_UINT32 Id, MT_UINT32 action, MT_UINT32 value, MT_UINT32 size, MT_UINT32 offset, MT_BYTE * data)
{
	struct iwreq req;
    OID_DEBUG_TABLE_REQUEST *sTable;

	if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 0xFFFFFFFF;
    }
    /********************************************************************
	* TODO - Change this function to "Special".
	* i.e., parse by itself, can reduce value, data, and size in some cases:
	* If data != 0, value is redundant.
	* If use value (in set, or when size <=long), data is redundant.
	* If use value from byte/short/long type - data, offset and size are redundant.
	*/

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );

    sTable = (OID_DEBUG_TABLE_REQUEST *)malloc( sizeof( OID_DEBUG_TABLE_REQUEST ) );
    if (sTable == NULL)
    {
        mt_print(2, "No memory when reading data...\n" );
        return 0xFFFFFFFF;
    }

    memset( sTable, 0, sizeof( OID_DEBUG_TABLE_REQUEST ) );
#if 0
	sTable->ID     = Id;
    sTable->action = action;
    sTable->value  = value; /*always copied*/
    sTable->size   = size;
    sTable->offset = offset;

    /************SET***************/
    if ( (action == 1) && (data != 0) )
	        memcpy( &sTable->data, data, 256 ); /*copy all*/
	/*else - if action == 1 -> the value already coppied*/
	/********************************/
    req.u.data.pointer = (caddr_t)sTable;
    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 29, &req ))
    {
        free( sTable );
        mt_print(2, "Error in execution\n" );
        return 0xFFFFFFFF;
    }
    /************GET***************/
    if (sTable->requestStatus == 0)
	    if (data != 0)
		    memcpy( data, &sTable->data, sTable->size/* * sizeof( unsigned long )*/ );
	else
        mt_print(2, "Error, Status = 0x%x\n",sTable->requestStatus );

    /********************************/

    free( sTable );

    mt_print(2, "return, sTable->value = %lx \n",sTable->value );
#else    
    mt_print(2, "Not supported\n" );
#endif    
	return sTable->value;
}

// Function name    : DrvCatInit
// Description	    : Initializes driver's data category for reading. Use
//                    DrvCatFree to release resources later. DrvCatInit can
//                    be called multiple times. In this case, if category
//                    is the same, resources from the previous call will be
//                    freed automatically.
// Return type      : MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UINT32 category    - requested category
// Argument         : MT_UINT32 * cnt - number of items in a category
// SCL command      : DRV_CAT_INIT Category Count
MT_UINT32 DrvCatInit(MT_UINT32 category, MT_UINT32 * cnt)
{
    OID_BCL_DRV_DATA_EX_REQUEST *sReq;
    int datalen = sizeof(MT_UINT32);
    int reqlen = sizeof(*sReq) + datalen;
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    sReq = (OID_BCL_DRV_DATA_EX_REQUEST *)malloc( reqlen );
    if (sReq == NULL)
    {
        mt_print(2, "Not enough memory when reading data from the driver\n" );
        return 1;
    }

    memset( sReq, 0, reqlen );

    sReq->mode = BclDrvModeCatInit;
    sReq->category = category;
    sReq->datalen = datalen;

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
    req.u.data.pointer = (caddr_t)sReq;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 22, &req ))
    {
        mt_print(2, "DrvCatInit: ioctl error\n" );
        free( sReq );
        return 1;
    }

    *cnt = * (MT_UINT32 *) (sReq + 1);

    free(sReq);
    return 0;
}

// Function name    : DrvCatFree
// Description	    : Frees resources allocated by DrvCatInit.
// Return type      : MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UINT32 category    - requested category
// SCL command      : DRV_CAT_FREE Category
MT_UINT32 DrvCatFree(MT_UINT32 category)
{
    OID_BCL_DRV_DATA_EX_REQUEST *sReq;
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    sReq = (OID_BCL_DRV_DATA_EX_REQUEST *)malloc( sizeof(*sReq) );
    if (sReq == NULL)
    {
        mt_print(2, "Not enough memory when reading data from the driver\n" );
        return 1;
    }

    memset( sReq, 0, sizeof( *sReq ) );

    sReq->mode = BclDrvModeCatFree;
    sReq->category = category;

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
    req.u.data.pointer = (caddr_t)sReq;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 22, &req ))
    {
        mt_print(2, "DrvCatFree: ioctl error\n" );
        free( sReq );
        return 1;
    }

    free(sReq);
    return 0;
}

// Function name    : DrvNameGet
// Description	    : Returns the name of a category item specified.
//                    Note that indexes may not neccessarily correspond to
//                    number of items returned by DrvCatInit because of
//                    possible presence of header/footer fields. Consult
//                    documentation on corresponding category before
//                    requesting data using this command.
// Return type      : MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UINT32 category    - requested category
// Argument         : MT_UINT32 index       - requested item index
// Argument         : MT_UBYTE * name       - item name
// SCL command      : DRV_CAT_INIT Category Index Name
MT_UINT32 DrvNameGet(MT_UINT32 category, MT_UINT32 index, MT_UBYTE * name)
{
    OID_BCL_DRV_DATA_EX_REQUEST *sReq;
    int datalen = 128;
    int reqlen = sizeof(*sReq) + datalen;
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    sReq = (OID_BCL_DRV_DATA_EX_REQUEST *)malloc( reqlen );
    if (sReq == NULL)
    {
        mt_print(2, "Not enough memory when reading data from the driver\n" );
        return 1;
    }

    memset( sReq, 0, sizeof(*sReq) );
    *(char *) (sReq + 1) = '\0';

    sReq->mode = BclDrvModeNameGet;
    sReq->category = category;
    sReq->index = index;
    sReq->datalen = datalen;

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
    req.u.data.pointer = (caddr_t)sReq;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 22, &req ))
    {
        mt_print(2, "DrvNameGet: ioctl error\n" );
        free( sReq );
        return 1;
    }

    strcpy( (char*) name, (char *) (sReq + 1) );

    free(sReq);
    return 0;
}

// Function name    : DrvValGet
// Description	    : Returns the numeric value of a category item specified.
//                    Note that indexes may not neccessarily correspond to
//                    number of items returned by DrvCatInit because of
//                    possible presence of multi-value tables. Consult
//                    documentation on corresponding category before
//                    requesting data using this command.
// Return type      : MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UINT32 category    - requested category
// Argument         : MT_UINT32 index       - requested item index
// Argument         : MT_UBYTE * val        - item value
// SCL command      : DRV_CAT_INIT Category Index Value
MT_UINT32 DrvValGet(MT_UINT32 category, MT_UINT32 index, MT_UINT32 * val)
{
    OID_BCL_DRV_DATA_EX_REQUEST *sReq;
    int datalen = sizeof(MT_UINT32);
    int reqlen = sizeof(*sReq) + datalen;
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    sReq = (OID_BCL_DRV_DATA_EX_REQUEST *)malloc( reqlen );
    if (sReq == NULL)
    {
        mt_print(2, "Not enough memory when reading data from the driver\n" );
        return 1;
    }

    memset( sReq, 0, reqlen );

    sReq->mode = BclDrvModeValGet;
    sReq->category = category;
    sReq->index = index;
    sReq->datalen = datalen;

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
    req.u.data.pointer = (caddr_t)sReq;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 22, &req ))
    {
        mt_print(2, "DrvValGet: ioctl error\n" );
        free( sReq );
        return 1;
    }

    *val = * (MT_UINT32 *) (sReq + 1);

    free(sReq);
    return 0;
}

// Function name    : DrvValSet
// Description	    : Triggers some action or sets a value in
//                    a specified category
// Return type      : MT_UINT32        -  MT_RET_OK / MT_RET_FAIL
// Argument         : MT_UINT32 category    - category
// Argument         : MT_UINT32 index       - item index
// Argument         : MT_UBYTE * val        - value to set
// SCL command      : DRV_VAL_SET Category Index Value
MT_UINT32 DrvValSet(MT_UINT32 category, MT_UINT32 index, MT_UINT32 val)
{
    OID_BCL_DRV_DATA_EX_REQUEST *sReq;
    int datalen = sizeof(MT_UINT32);
    int reqlen = sizeof(*sReq) + datalen;
    struct iwreq req;

    if (gifcount == 0)
    {
        mt_print(2, "No wireless interfaces to write the data\n" );
        return 1;
    }

    sReq = (OID_BCL_DRV_DATA_EX_REQUEST *)malloc( reqlen );
    if (sReq == NULL)
    {
        mt_print(2, "Not enough memory when writing data to the driver\n" );
        return 1;
    }

    memset( sReq, 0, sizeof(*sReq) );
    * (MT_UINT32 *) (sReq + 1) = val;

    sReq->mode = BclDrvModeValSet;
    sReq->category = category;
    sReq->index = index;
    sReq->datalen = datalen;

    memcpy( req.ifr_ifrn.ifrn_name, gifs[0].name, IFNAMSIZ );
    req.u.data.pointer = (caddr_t)sReq;

    if (ioctl( gsocket, SIOCIWFIRSTPRIV + 22, &req ))
    {
        mt_print(2, "DrvValSet: ioctl error\n" );
        free( sReq );
        return 1;
    }

    free(sReq);
    return 0;
}

