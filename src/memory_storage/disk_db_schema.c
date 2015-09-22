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

#include "dbs_schema.h"

#include "disk_db_schema.h"

static db_fielddef_t disk_hosts_fields[] =
{
    { HOSTNAME_FNO,     "hostname",        DB_COLTYPE_ANSISTR,     MAX_HOSTNAME_LEN, 0, DB_NOT_NULL, 0 },
    { HOSTIP_FNO,       "hostip",          DB_COLTYPE_ANSISTR,     MAX_IP_LEN,       0, DB_NOT_NULL, 0 },
    { REQCOUNT_FNO,     "requestcount",    DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { AGE_FNO,          "age",             DB_COLTYPE_SINT64,      0,                0, DB_NULLABLE, 0 },
};

static db_indexfield_t disk_hosts_pkey_fields[] = { { HOSTNAME_FNO }, };
static db_indexfield_t disk_hosts_by_age_idx_fields[] = { { AGE_FNO }, };
static db_indexdef_t disk_hosts_indexes[] =
{
    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      HOSTS_PKEY_INDEX_NAME,      /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(disk_hosts_pkey_fields),  /* nfields */
      disk_hosts_pkey_fields },              /* fields */

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      HOSTS_BY_AGE_INDEX_NAME,    /* index_name */
      DB_MULTISET_INDEX,          /* index_mode */
      DB_ARRAY_DIM(disk_hosts_by_age_idx_fields),  /* nfields */
      disk_hosts_by_age_idx_fields },              /* fields */
};

static db_tabledef_t tables[] = 
{
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_KHEAP,   
        DISK_HOSTS_TABLE,
        DB_ARRAY_DIM(disk_hosts_fields),
        disk_hosts_fields,
        DB_ARRAY_DIM(disk_hosts_indexes),   // Indexes array size
                     disk_hosts_indexes,    // Indexes array
        0, NULL,
    },
    /// Just the same table but in memory
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_MEMORY,   
        MEM_HOSTS_TABLE,
        DB_ARRAY_DIM(disk_hosts_fields),
        disk_hosts_fields,
        DB_ARRAY_DIM(disk_hosts_indexes),   // Indexes array size
                     disk_hosts_indexes,    // Indexes array
        0, NULL,
    },
};

static db_seqdef_t sequences[] = 
{
    { AGE_SEQUENCE, {{ 1, 0}} },
};

dbs_schema_def_t disk_db_schema = 
{
    DB_ARRAY_DIM(tables),
    tables,
    DB_ARRAY_DIM(sequences),
    sequences
};


