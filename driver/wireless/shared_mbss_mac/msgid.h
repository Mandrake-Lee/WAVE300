/***************************************************************************
****************************************************************************
**
** COMPONENT:        Common
**
** MODULE:           $File: //bwp/enet/demo153_sw/develop/src/common/msgid.h $
**
** VERSION:          $Revision: #1 $
**
** DATED:            $Date: 2003/10/27 $
**
** AUTHOR:           LDE
**
** DESCRIPTION:      Message ID Defines
**
** CHANGE HISTORY:
**
**   $Log: msgid.h $
**   Revision 1.4  2003/02/20 16:44:06Z  prh
**   Tidy up MACRO for host usage.
**   Revision 1.3  2003/02/12 15:58:29Z  jh3
**   Increased message type space from 4 to 6 bit to accomodate new
**   RSN messages
**   Revision 1.2  2003/02/11 17:30:21Z  prh
**   Bring Host applications into main Software build evnironment.
**   Revision 1.7  2001/08/14 10:13:23Z  ifo
**   Added class for driver messages.
**   Revision 1.6  2001/07/19 09:03:24Z  ifo
**   Added component for test messages.
**   Revision 1.5  2001/07/18 16:40:22Z  lde
**   a few more useful macros.
**   Revision 1.4  2001/07/18 15:03:02Z  lde
**   Added macros to check whether message needs reply
**   Revision 1.3  2001/04/20 16:18:37Z  lde
**   Added MAC Client component.
**   Revision 1.2  2001/03/07 16:55:51Z  lde
**   Revision 1.1  2001/03/07 16:29:00Z  lde
**   Initial revision
**
** LAST MODIFIED BY:   $Author: njk $
**                     $Modtime:$
**
****************************************************************************
*
* Copyright (c) TTPCom Limited, 2001
*
* Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef _MSGID_H
#define _MSGID_H

#define  MTLK_PACK_ON
#include "mtlkpack.h"

/***************************************************************************/
/***                         Types and Defines                           ***/
/***************************************************************************/

/*
 * Message IDs in the system will be structured as follows:
 *
 * Bit 13..12   Indicates whether the message is a Request, Confirm,
 *              Indication or Response.
 *
 * Bit 11..8    Component that message belongs to e.g.
 *              MSG_UMI_COMP, MSG_LMI_COMP, MSG_UPPER_MAC_COMP
 *
 * Bit 7..0     Message number, unique to all messages in a component.
 *
 */
#define MSG_DIR_OFFSET                  (12)
#define MSG_DIR_MASK                    (0x3 << MSG_DIR_OFFSET)
#define MSG_GET_DIR(x)                  (((x) >> MSG_DIR_OFFSET) & 3)
#define MSG_REQ                         (0)
#define MSG_CFM                         (1)
#define MSG_IND                         (3)
#define MSG_RES                         (2)
#define MSG_DIR_NUM                     (4)

#define MSG_MAKE_REPLY(x)               ((x) ^= (1 << 12))

#define MSG_COMP_OFFSET                 (8)
#define MSG_COMP_MASK                   (0xF << MSG_COMP_OFFSET)
#define MSG_GET_COMP(x)                 ((x) & MSG_COMP_MASK)
#define MSG_UMI_COMP                    (0x0 << MSG_COMP_OFFSET)
#define MSG_LMI_COMP                    (0x1 << MSG_COMP_OFFSET)
#define MSG_UPPER_MAC_COMP              (0x2 << MSG_COMP_OFFSET)
#define MSG_MAC_CLIENT_COMP             (0x3 << MSG_COMP_OFFSET)
#define MSG_HOST_COMP					(0x4 << MSG_COMP_OFFSET)
#define MSG_TEST_COMP                   (0xF << MSG_COMP_OFFSET)

#define MSG_COMP_DIR_OFFSET				(8)
#define MSG_COMP_DIR_MASK				(0xff << MSG_COMP_DIR_OFFSET)
#define MSG_HOST_REQ_MASK				(MSG_HOST_COMP | (MSG_REQ << MSG_DIR_OFFSET))
#define MSG_HOST_CFM_MASK				(MSG_HOST_COMP | (MSG_CFM << MSG_DIR_OFFSET))


#define MSG_TYPE_OFFSET                 (6)
#define MSG_GET_TYPE(x)                 (((x) >> MSG_TYPE_OFFSET) & 3)
#define MSG_TYPE_MASK                   (3 << MSG_TYPE_OFFSET)
#define MSG_MAN                         (0)
#define MSG_DAT                         (1)
#define MSG_MEM                         (2)
#define MSG_DBG                         (3)
#define MSG_TYPES_NUM                   (4)

#define MSG_DRIVER_COMP                 (0x8 << MSG_COMP_OFFSET)
#define MSG_TEST_COMP                   (0xF << MSG_COMP_OFFSET)

#define MSG_NUM_MASK                    (0x3F)
#define MSG_GET_NUM(x)                  ((x) & MSG_NUM_MASK)

#define  MTLK_PACK_OFF
#include "mtlkpack.h"

#endif /* !_MSGID_H */
