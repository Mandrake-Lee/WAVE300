/*****************************************************************************
*   Source File:
*       mt_cnfg.h
*   Product Name:
*       Metalink VDSL Software
*   Description:                               
*       Customer/User settings and Metalink EVM PPC860 settings.
*   Copyright: 
*       (C) Metalink Ltd. 
*       All rights are strictly reserved. Reproduction or divulgence in any 
*       form whatsoever is not permitted without written authority from the 
*       copyright owner. Issued by Metalink Transmission Devices Ltd in 
*       Israel - 11/94.
*****************************************************************************/

#ifndef __MT_CNFG_H__                                 
#define __MT_CNFG_H__

/* Metalink's MT_EXTERN prefix should be used before variables definitions in this file */
#ifdef MT_GLOBAL
#define MT_EXTERN
#else
#define MT_EXTERN extern
#endif

/*****************************************************************************
*   Compiler dependant definitions - types.
*       The following type definitions should comply with the user host 
*   compiler requirements.
*****************************************************************************/
#ifndef _MS_SBR_CREATOR    

#define MT_CODE                         const              
#define MT_VOID                         void
#define MT_INLINE                       __inline__
#define MT_STATIC                       static
#define MT_INTERRUPT_PREFIX             void 
#define MT_INTERRUPT_SUFFIX 
#define MT_UBYTE                        unsigned char      /*  8 bit */
#define MT_BYTE                         signed char        /*  8 bit */
#define MT_UINT16                       unsigned short int /* 16 bit */
#define MT_INT16                        signed short int   /* 16 bit */
#define MT_UINT32                       unsigned long int  /* 32 bit */
#define MT_INT32                        signed long int    /* 32 bit */

#else /* support in MS browse info */

#define MT_CODE                         const              
#define MT_VOID                         void
#define MT_INLINE                       
#define MT_STATIC                       static
#define MT_INTERRUPT_PREFIX             void 
#define MT_INTERRUPT_SUFFIX 
#define MT_UBYTE                        unsigned char      /*  8 bit */
#define MT_BYTE                         signed char        /*  8 bit */
#define MT_UINT16                       unsigned short int /* 16 bit */
#define MT_INT16                        signed short int   /* 16 bit */
#define MT_UINT32                       unsigned long int  /* 32 bit */
#define MT_INT32                        signed long int    /* 32 bit */
#define __asm__(x)                      

#endif /* support in MS browse info */



/*****************************************************************************
*   OS dependant definition - terminal debug output.
*       The MT_TRACE macros are used in the Metalink driver for debug purposes.
*       Message are printed for errors, warnings and events.
*       We highly recommend to implement it for easy debug and better Metalink support.
*****************************************************************************/
#define MT_TRACE_ENABLE

#ifdef  MT_TRACE_ENABLE
#if !MT_EVM /* User:  */
     /* implement using your output function.
        for your own convenient, printouts are trunk oriented, so you can disable
        and enable printouts based on a trunk number.
        If a certain printout is not trunk oriented, the first argument is MT_DUMMY_ARG = 0xFF. */

/* for printouts with no arguments (just a string) */
#define MT_TRACE(MESSAGE)\
        mt_print(2,MESSAGE)    
/* for printouts with one argument  */
#define MT_TRACE_1(TRUNK,MESSAGE,ARG1)\
        mt_print(2,MESSAGE,ARG1)
/* for printouts with two arguments  */
#define MT_TRACE_2(TRUNK,MESSAGE,ARG1,ARG2)\
        mt_print(2,MESSAGE,ARG1,ARG2)    
/* for printouts with 3 arguments  */
#define MT_TRACE_3(TRUNK,MESSAGE,ARG1,ARG2,ARG3)\
        mt_print(2,MESSAGE,ARG1,ARG2,ARG3)    
/* for printouts with 4 arguments  */
#define MT_TRACE_4(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4)\
        mt_print(2,MESSAGE,ARG1,ARG2,ARG3,ARG4)   
/* for printouts with 5 arguments  */
#define MT_TRACE_5(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4,ARG5)\
        mt_print(2,MESSAGE,ARG1,ARG2,ARG3,ARG4,ARG5)    

#else /* Metalink PPC860 EVM.  Do not change: */
/* for printouts with no arguments (just a string) */
#define MT_TRACE(MESSAGE)\
        mt_mt_print(2,MESSAGE)    
/* for printouts with one argument  */
#define MT_TRACE_1(TRUNK,MESSAGE,ARG1)\
        mt_mt_print(2,MESSAGE,ARG1)
/* for printouts with two arguments  */
#define MT_TRACE_2(TRUNK,MESSAGE,ARG1,ARG2)\
        mt_mt_print(2,MESSAGE,ARG1,ARG2)    
/* for printouts with 3 arguments  */
#define MT_TRACE_3(TRUNK,MESSAGE,ARG1,ARG2,ARG3)\
        mt_mt_print(2,MESSAGE,ARG1,ARG2,ARG3)    
/* for printouts with 4 arguments  */
#define MT_TRACE_4(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4)\
        mt_mt_print(2,MESSAGE,ARG1,ARG2,ARG3,ARG4)    
/* for printouts with 5 arguments  */
#define MT_TRACE_5(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4,ARG5)\
        mt_mt_print(2,MESSAGE,ARG1,ARG2,ARG3,ARG4,ARG5)    

#endif

#else  /* MT_TRACE_ENABLE is undefined: */
       /* leave these following macros empty for both Metalink and user's code ! */
#define MT_TRACE(MESSAGE)          
#define MT_TRACE_1(TRUNK,MESSAGE,ARG1)          
#define MT_TRACE_2(TRUNK,MESSAGE,ARG1,ARG2)          
#define MT_TRACE_3(TRUNK,MESSAGE,ARG1,ARG2,ARG3)          
#define MT_TRACE_4(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4)  
#define MT_TRACE_5(TRUNK,MESSAGE,ARG1,ARG2,ARG3,ARG4,ARG5)

#endif



#undef MT_EXTERN
#endif /* __MT_CNFG_H__ */
