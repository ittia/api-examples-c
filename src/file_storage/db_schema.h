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

#ifndef DB_SCHEMA_H_INCLUDED
#define DB_SCHEMA_H_INCLUDED

#include <ittia/db.h>

#define STORAGE_TABLE "table"
#define MAX_STRING_FIELD 10
#define PKEY_INDEX_NAME "pkey"
#define PKEY_FIELD_NO 1

//------- Declarations to use while populating table

/* Populate table using relative bounds fields. So declare appropriate structure. See 
   ittiadb/manuals/users-guide/api-c-database-access.html#relative-bound-fields
*/
typedef struct {
    db_ansi_t   f0[MAX_STRING_FIELD + 1];   /* Extra char for null termination. */
    uint64_t    f1;
    double      f2;
    char        f3[MAX_STRING_FIELD*2 + 1]; 
} storage_t;

static const db_bind_t binds_def[] = {
    { 0, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( storage_t, f0 ),  DB_BIND_SIZE( storage_t, f0 ), -1, DB_BIND_RELATIVE },
    { PKEY_FIELD_NO, DB_VARTYPE_SINT64,   DB_BIND_OFFSET( storage_t, f1 ),  DB_BIND_SIZE( storage_t, f1 ), -1, DB_BIND_RELATIVE }, 
    { 2, DB_VARTYPE_FLOAT64,  DB_BIND_OFFSET( storage_t, f2 ),  DB_BIND_SIZE( storage_t, f2 ), -1, DB_BIND_RELATIVE }, 
    { 3, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, f3 ),  DB_BIND_SIZE( storage_t, f3 ), -1, DB_BIND_RELATIVE }, 
};

extern dbs_schema_def_t db_schema;

#endif
