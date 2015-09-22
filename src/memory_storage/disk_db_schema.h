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

#ifndef DISK_DB_SCHEMA_H_INCLUDED
#define DISK_DB_SCHEMA_H_INCLUDED

#define MAX_HOSTNAME_LEN 50
#define MAX_IP_LEN 16

#define DISK_HOSTS_TABLE    "hosts_dsk"
#define MEM_HOSTS_TABLE     "hosts_mem"
#define AGE_SEQUENCE        "age_seq"

// Hosts table fields
#define HOSTNAME_FNO    0
#define HOSTIP_FNO      1    
#define REQCOUNT_FNO    2
#define AGE_FNO         3   

#define HOSTS_PKEY_INDEX_NAME       "hosts_pkey"
#define HOSTS_BY_AGE_INDEX_NAME     "hosts_age_idx"

extern dbs_schema_def_t disk_db_schema;

/// Cache record data
typedef struct {
    db_ansi_t hostname[ MAX_HOSTNAME_LEN + 1 ];
    db_ansi_t hostip[ MAX_IP_LEN + 1 ];
    int32_t   reqcount;
    int64_t   age;
} cache_db_row_t;

static const db_bind_t host_binds_def[] = {
    { 
        HOSTNAME_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( cache_db_row_t, hostname ),
        DB_BIND_SIZE( cache_db_row_t, hostname ), -1, DB_BIND_RELATIVE
    },
    { 
        HOSTIP_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( cache_db_row_t, hostip ),
        DB_BIND_SIZE( cache_db_row_t, hostip ), -1, DB_BIND_RELATIVE
    },
    { 
        REQCOUNT_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( cache_db_row_t, reqcount ),
        DB_BIND_SIZE( cache_db_row_t, reqcount ), -1, DB_BIND_RELATIVE
    },
    { 
        AGE_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( cache_db_row_t, age ),
        DB_BIND_SIZE( cache_db_row_t, age ), -1, DB_BIND_RELATIVE
    },
};

#endif // DISK_DB_SCHEMA_H_INCLUDED
