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


#include <ittia/db.h>
#include "schema_upgrade_schema.h"
#include "dbs_schema.h"

/* TABLE CONTACT */
// Target table schema (v2)
//
//   In v2 we decided to:
//   - Int-type column "sex" replace with string-type column "title"
//
db_fielddef_t contact_fields[] =
{
    { CONTACT_ID,           "id",           DB_COLTYPE_UINT64,   0,                0, DB_NOT_NULL, 0 },
    { CONTACT_NAME,         "name",         DB_COLTYPE_WCHARSTR, MAX_CONTACT_NAME, 0, DB_NOT_NULL, 0 },
    { CONTACT_RING_ID,      "ring_id",      DB_COLTYPE_UINT64,   0,                0, DB_NOT_NULL, 0 },
    //+++ In v2 schema we decided to replace "sex_title" (Mrs, Mr values) column with int-type "sex" field
    { CONTACT_SEX,          "sex",          DB_COLTYPE_UINT8,    0,                0, DB_NOT_NULL, 0 }
};

// v1 PKey fields
static db_indexfield_t contact_by_id_fields[] = 
{
    { CONTACT_ID }
};

static db_indexfield_t contact_by_name_fields[] =
{
    { CONTACT_NAME }
};

// v2 PKey fields
//   In v2 wi decided to change primary key from (id) to (id, ring_id). I.e add "ring_id" to v1 primary key
static db_indexfield_t contact_by_id_and_ring_fields[] = 
{
    { CONTACT_ID }, { CONTACT_RING_ID }
};

db_indexdef_t contact_indexes[] =
{
    { DB_ALLOC_INITIALIZER(),       /* db_alloc */
        DB_INDEXTYPE_DEFAULT,       /* index_type */
        CONTACT_BY_ID_RING_ID,      /* index_name */
        DB_PRIMARY_INDEX,           /* index_mode */
        DB_ARRAY_DIM(contact_by_id_and_ring_fields),  /* nfields */
        contact_by_id_and_ring_fields },     /* fields */

    { DB_ALLOC_INITIALIZER(),        /* db_alloc */
        DB_INDEXTYPE_DEFAULT,        /* index_type */
        CONTACT_BY_NAME,             /* index_name */
        DB_MULTISET_INDEX,           /* index_mode */
        DB_ARRAY_DIM(contact_by_name_fields), /* nfields */
        contact_by_name_fields },    /* fields */
};

/* TABLE VERSION */
static db_fielddef_t schema_version_fields[] =
{
    { SCHEMA_VERSION_VERSION,       "VERSION",       DB_COLTYPE_UINT64,   0,                0, DB_NOT_NULL, 0 },
};

static db_tabledef_t tables[] = 
{
    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        CONTACT_TABLE,
        DB_ARRAY_DIM(contact_fields),
                     contact_fields,
        DB_ARRAY_DIM(contact_indexes),
                     contact_indexes,
        0, NULL,
    },

    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        SCHEMA_VERSION_TABLE,
        DB_ARRAY_DIM(schema_version_fields),
                     schema_version_fields,
        0, NULL,
        0, NULL,
    },

};

dbs_versioned_schema_def_t v2_schema = 
{
    {
        DB_ARRAY_DIM(tables),
        tables,
    },
    2,
};
