

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Sun Nov 06 16:51:37 2011
 */
/* Compiler settings for UOAI.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, LIBID_UOAI,0x61937D8B,0xBBCE,0x48b0,0x8A,0xDF,0xF0,0x3C,0x94,0x43,0xC7,0xF2);


MIDL_DEFINE_GUID(CLSID, CLSID_Client,0xBFC0EE0A,0x2B29,0x41d9,0x83,0xDA,0x4B,0x6B,0x61,0xEF,0x54,0xF9);


MIDL_DEFINE_GUID(CLSID, CLSID_ClientList,0xBE02C37F,0x111E,0x402e,0x9D,0x05,0x62,0x1C,0x3D,0x32,0x87,0x24);


MIDL_DEFINE_GUID(CLSID, CLSID_UOAI,0x8A11E500,0x2101,0x4a27,0xBB,0x16,0x54,0x98,0xC2,0x8F,0x91,0xDA);


MIDL_DEFINE_GUID(CLSID, CLSID_COMInjector,0xB25FAAF7,0xC0AA,0x4007,0x8C,0x85,0x76,0x93,0xE0,0x2D,0x59,0xF0);


MIDL_DEFINE_GUID(IID, IID_UOAIInterface,0xCD10E6BE,0xFDEE,0x4d59,0xA8,0x12,0x6F,0xD0,0x8C,0x02,0xE8,0xCE);


MIDL_DEFINE_GUID(IID, IID_ClientListInterface,0x39FB3C1B,0xEF1F,0x4e30,0xAF,0x63,0x83,0xCB,0x91,0xB9,0xFF,0xA8);


MIDL_DEFINE_GUID(IID, IID_ClientListEventInterface,0xB327CDFD,0x2742,0x42c7,0xA7,0x81,0x50,0x84,0xEE,0x06,0xDC,0xDC);


MIDL_DEFINE_GUID(IID, IID_ClientInterface,0x45A784D5,0x0E63,0x49c9,0xAC,0xBA,0x46,0xE4,0xED,0xBD,0x7D,0x29);


MIDL_DEFINE_GUID(IID, IID_ClientEventInterface,0xEC60659C,0x611F,0x467c,0xB1,0xF8,0x2C,0xDB,0x15,0xBD,0x25,0x7E);


MIDL_DEFINE_GUID(IID, IID_COMInjectorInterface,0x486EC9D9,0x5FC9,0x469a,0xB5,0x0A,0x4E,0xE7,0xF7,0x58,0xE0,0xD3);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



