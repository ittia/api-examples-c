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

/** @file memory_db_schema.c
 *
 *  Memory DB scema to use in 'full_memory_storage' & 'memory_disk_hybrid'  examples.
 *
 *  Both examples supposed to be by part of some software which collects network information
 *  about TCPIPv4 connections of some client host.
 *
 *  DB contais two tables:
 *    - 'hosts' table holds summary information about connected hosts:
 *       - ipv4 address;
 *       - hostname;
 *       - input/output statistics;
 *       - connections count;
 *    - 'connstat' table holds detailed information about connections:
 *       - source port;
 *       - detination port;
 *       - input/output statistics;
 *
 *  Hosts in 'hosts' table related by foreign key from 'connstat' table.
*/

#include "dbs_schema.h"

#include "memory_db_schema.h"

static db_fielddef_t hosts_fields[] =
{
    { HOSTID_FNO,       "hostid",          DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { HOSTIP_FNO,       "hostip",          DB_COLTYPE_ANSISTR,     MAX_IP_LEN,       0, DB_NOT_NULL, 0 },
    { HOSTNAME_FNO,     "hostname",        DB_COLTYPE_ANSISTR,     MAX_HOSTNAME_LEN, 0, DB_NULLABLE, 0 },
    { HOSTIOSTAT_FNO,   "iostat",          DB_COLTYPE_SINT64,      0,                0, DB_NULLABLE, 0 },
    { CONNCOUNT_FNO,    "conncount",       DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { HOSTAGE_FNO,      "age",             DB_COLTYPE_SINT64,      0,                0, DB_NULLABLE, 0 },
};
static db_indexfield_t hosts_pkey_fields[] = { { HOSTID_FNO }, };
static db_indexfield_t hosts_by_ip_idx_fields[] = { { HOSTIP_FNO }, };
static db_indexfield_t hosts_by_name_idx_fields[] = { { HOSTNAME_FNO }, };
static db_indexfield_t hosts_by_age_idx_fields[] = { { HOSTAGE_FNO }, };

static db_indexdef_t hosts_indexes[] =
{
    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      HOSTS_PKEY_INDEX_NAME,      /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(hosts_pkey_fields),  /* nfields */
      hosts_pkey_fields },              /* fields */
    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      HOSTS_BY_IP_INDEX_NAME,     /* index_name */
      DB_UNIQUE_INDEX,            /* index_mode */
      DB_ARRAY_DIM(hosts_by_ip_idx_fields),  /* nfields */
      hosts_by_ip_idx_fields },              /* fields */

//  Attention. Evaluation verions of ittiadb does not permits creation more then 5 indexes. So comment this.
//    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
//      DB_INDEXTYPE_DEFAULT,       /* index_type */
//      HOSTS_BY_NAME_INDEX_NAME,   /* index_name */
//      DB_UNIQUE_INDEX,            /* index_mode */
//      DB_ARRAY_DIM(hosts_by_name_idx_fields),  /* nfields */
//      hosts_by_name_idx_fields },              /* fields */

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      HOSTS_BY_AGE_INDEX_NAME,    /* index_name */
      DB_MULTISET_INDEX,          /* index_mode */
      DB_ARRAY_DIM(hosts_by_age_idx_fields),  /* nfields */
      hosts_by_age_idx_fields },              /* fields */
};

static db_fielddef_t connstat_fields[] =
{
    { HOSTID_FNO,       "hostid",          DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { DPORT_FNO,        "dport",           DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { SPORT_FNO,        "sport",           DB_COLTYPE_UINT32,      0,                0, DB_NOT_NULL, 0 },
    { IOSTAT_FNO,       "iostat",          DB_COLTYPE_UINT64,      0,                0, DB_NULLABLE, 0 },
    { CONNAGE_FNO,      "age",             DB_COLTYPE_SINT32,      0,                0, DB_NULLABLE, 0 },
};

// PKey fields
static db_indexfield_t connstat_pkey_fields[] = { { HOSTID_FNO }, { DPORT_FNO }, { SPORT_FNO }, };
static db_indexfield_t connstat_by_age_idx_fields[] = { { CONNAGE_FNO }, };

static db_indexdef_t connstat_indexes[] =
{

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      CONNSTAT_PKEY_INDEX_NAME,   /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(connstat_pkey_fields),  /* nfields */
      connstat_pkey_fields },              /* fields  */

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      CONNSTAT_BY_AGE_INDEX_NAME,    /* index_name */
      DB_MULTISET_INDEX,          /* index_mode */
      DB_ARRAY_DIM(connstat_by_age_idx_fields),  /* nfields */
      connstat_by_age_idx_fields },              /* fields */
};

static db_foreign_key_def_t connstat_fkeys[] = 
{
    { HOSTID_FKEY_NAME,                         /* fk_name   */
      HOSTS_TABLE,                              /* ref_table */
      DB_FK_MATCH_SIMPLE,                       /* match_option */
      DB_FK_ACTION_RESTRICT,                    /* update_rule  */
      DB_FK_ACTION_CASCADE,                     /* delete_rule  */
      DB_FK_NOT_DEFERRABLE,                     /* deferrable   */
      DB_FK_CHECK_IMMEDIATE,                    /* check_time   */

      1,                                        /* nfields      */
      {                                         /* fields[ DB_MAX_FOREIGNKEY_FIELD_COUNT ] */
          {
              HOSTID_FNO,                       /* org_field    */
              HOSTID_FNO,                       /* ref_field - Field number in the referenced table */
          }
      }
    },
};

static db_tabledef_t tables[] = 
{
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_MEMORY,   
        HOSTS_TABLE,
        DB_ARRAY_DIM(hosts_fields),
        hosts_fields,
        DB_ARRAY_DIM(hosts_indexes),   // Indexes array size
                     hosts_indexes,    // Indexes array
        0, NULL,
    },
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_MEMORY,   
        CONNSTAT_TABLE,
        DB_ARRAY_DIM(connstat_fields),
        connstat_fields,
        DB_ARRAY_DIM(connstat_indexes),   // Indexes array size
                     connstat_indexes,    // Indexes array
        DB_ARRAY_DIM(connstat_fkeys),     // FKey array size
                     connstat_fkeys       // FKeys array
    },
};

static db_seqdef_t sequences[] = 
{
    { AGE_SEQUENCE, {{ 1, 0}} },
};

dbs_schema_def_t db_schema = 
{
    DB_ARRAY_DIM(tables),
    tables,
    DB_ARRAY_DIM(sequences),
    sequences
};

