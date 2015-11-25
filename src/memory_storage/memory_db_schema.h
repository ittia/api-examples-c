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

#ifndef MEMORY_DB_SCHEMA_H_INCLUDED
#define MEMORY_DB_SCHEMA_H_INCLUDED

#define HOSTS_TABLE     "hosts"
#define CONNSTAT_TABLE  "connstat"
#define AGE_SEQUENCE    "age_seq"

// Hosts table fields
#define HOSTID_FNO      0
#define HOSTIP_FNO      1
#define HOSTNAME_FNO    2
#define HOSTIOSTAT_FNO  3
#define CONNCOUNT_FNO   4
#define HOSTAGE_FNO     5

#define MAX_HOSTNAME_LEN 50
#define MAX_IP_LEN 16

// Connstat table fields
//#define HOSTID_FNO    0
#define DPORT_FNO       1
#define SPORT_FNO       2
#define IOSTAT_FNO      3
#define CONNAGE_FNO     4

#define HOSTS_PKEY_INDEX_NAME       "hosts_pkey"
#define HOSTS_BY_IP_INDEX_NAME      "hosts_ip_idx"
#define HOSTS_BY_NAME_INDEX_NAME    "hosts_name_idx"
#define HOSTS_BY_AGE_INDEX_NAME     "hosts_age_idx"

#define CONNSTAT_PKEY_INDEX_NAME    "cs_pkey"
#define CONNSTAT_BY_AGE_INDEX_NAME  "cs_by_age"
#define HOSTID_FKEY_NAME            "cs_hostid_fkey"

extern dbs_schema_def_t db_schema;

//------- Declarations to use while populating table
typedef struct {
    int32_t   hostid;
    db_ansi_t hostip[ MAX_IP_LEN + 1 ];
    db_ansi_t hostname[ MAX_HOSTNAME_LEN + 1 ];
    int64_t   iostat;
    int32_t   conncount;
    int64_t   age;
} host_db_row_t;

typedef struct {
    int32_t   hostid;
    uint32_t  dport;
    uint32_t  sport;
    int64_t   iostat;
    int64_t   age;
} cs_db_row_t;

static const db_bind_t host_binds_def[] = {
    {
        HOSTID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( host_db_row_t, hostid ),
        DB_BIND_SIZE( host_db_row_t, hostid ), -1, DB_BIND_RELATIVE
    },
    {
        HOSTIP_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( host_db_row_t, hostip ),
        DB_BIND_SIZE( host_db_row_t, hostip ), -1, DB_BIND_RELATIVE
    },
    {
        HOSTNAME_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( host_db_row_t, hostname ),
        DB_BIND_SIZE( host_db_row_t, hostname ), -1, DB_BIND_RELATIVE
    },
    {
        HOSTIOSTAT_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( host_db_row_t, iostat ),
        DB_BIND_SIZE( host_db_row_t, iostat ), -1, DB_BIND_RELATIVE
    },
    {
        CONNCOUNT_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( host_db_row_t, conncount ),
        DB_BIND_SIZE( host_db_row_t, conncount ), -1, DB_BIND_RELATIVE
    },
    {
        HOSTAGE_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( host_db_row_t, age ),
        DB_BIND_SIZE( host_db_row_t, age ), -1, DB_BIND_RELATIVE
    },
};

static const db_bind_t cs_binds_def[] = {
    {
        HOSTID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( cs_db_row_t, hostid ),
        DB_BIND_SIZE( cs_db_row_t, hostid ), -1, DB_BIND_RELATIVE
    },
    {
        DPORT_FNO, DB_VARTYPE_UINT32,  DB_BIND_OFFSET( cs_db_row_t, dport ),
        DB_BIND_SIZE( cs_db_row_t, dport ), -1, DB_BIND_RELATIVE
    },
    {
        SPORT_FNO, DB_VARTYPE_UINT32,  DB_BIND_OFFSET( cs_db_row_t, sport ),
        DB_BIND_SIZE( cs_db_row_t, sport ), -1, DB_BIND_RELATIVE
    },
    {
        IOSTAT_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( cs_db_row_t, iostat ),
        DB_BIND_SIZE( cs_db_row_t, iostat ), -1, DB_BIND_RELATIVE
    },
    {
        CONNAGE_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( cs_db_row_t, age ),
        DB_BIND_SIZE( cs_db_row_t, age ), -1, DB_BIND_RELATIVE
    },
};


#endif // #ifndef MEMORY_DB_SCHEMA_H_INCLUDED
