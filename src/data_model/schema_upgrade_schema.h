/**************************************************************************/
/*                                                                        */
/*      Copyright (c) 2005-2015 by ITTIA L.L.C. All rights reserved.      */
/*                                                                        */
/*  This software is copyrighted by and is the sole property of ITTIA     */
/*  L.L.C.  All rights, title, ownership, or other interests in the       */
/*  software remain the property of ITTIA L.L.C.  This software may only  */
/*  be used in accordance with the corresponding license agreement.  Any  */
/*  unauthorized use, duplication, transmission, distribution, or         */
/*  disclosure of this software is expressly forbidden.                   */
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */
/*  written consent of ITTIA L.L.C.                                       */
/*                                                                        */
/*  ITTIA L.L.C. reserves the right to modify this software without       */
/*  notice.                                                               */
/*                                                                        */
/*  info@ittia.com                                                        */
/*  http://www.ittia.com                                                  */
/*                                                                        */
/*                                                                        */
/**************************************************************************/

/*-------------------------------------------------------------------------*
* @file schema_upgrade_db_schema.h                                        *
*                                                                         *
* schema_upgrade example DB schemas definitions                           *
*-------------------------------------------------------------------------*/

#ifndef SCHEMA_UPGRADE_SCHEMA_H
#define SCHEMA_UPGRADE_SCHEMA_H 1

#include "dbs_schema.h"

#define CONTACT_TABLE           "contact"
#define CONTACT_BY_ID           "by_id"                     ///< v1 pkey index
#define CONTACT_BY_ID_RING_ID   "by_id_and_ring_id"     ///< v2 pkey index
#define CONTACT_BY_NAME         "by_name"
#define CONTACT_ID_SEQUENCE     "contact_id"

#define MAX_CONTACT_NAME        50   /* Unicode characters */
#define DATA_SIZE               1024 /* BLOB chunk size */
#define MAX_SEX_TITLE           5    /* ANSI characters */

/*-------------------------------------------------------------------------*
* CONTACT field IDs                                                       *
*-------------------------------------------------------------------------*/
#define CONTACT_ID              0
#define CONTACT_NAME            1
#define CONTACT_RING_ID         2
#define CONTACT_SEX             3
#define CONTACT_NFIELDS         4


typedef enum {
    SEX_MAN = 0,
    SEX_WOMAN = 1,
    SEX_UNDEFINED = 2,
} SexEnum;


#define SCHEMA_VERSION_TABLE           "SCHEMA_VERSION"
/*-------------------------------------------------------------------------*
* VERSION field IDs                                                       *
*-------------------------------------------------------------------------*/
#define SCHEMA_VERSION_VERSION  0

typedef struct {
    dbs_schema_def_t schema;
    int version;
} dbs_versioned_schema_def_t;

#endif // SCHEMA_UPGRADE_DB_SCHEMAS_H
