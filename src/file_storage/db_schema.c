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

#include "db_schema.h"

static db_fielddef_t storage_fields[] =
{
    { 0, "ansi_field",      DB_COLTYPE_ANSISTR,     MAX_STRING_FIELD, 0, DB_NULLABLE, 0 },
    { 1, "int64_field",     DB_COLTYPE_SINT64,      0,                0, DB_NOT_NULL, 0 },
    { 2, "float64_field",   DB_COLTYPE_FLOAT64,     0,                0, DB_NULLABLE, 0 },
    { 3, "utf8_field",      DB_COLTYPE_UTF8STR,     2*MAX_STRING_FIELD, 0, DB_NOT_NULL, 0 },
};

// PKey fields
static db_indexfield_t pkey_fields[] =
{
    { 1 }
};

static db_indexdef_t indexes[] =
{
    { DB_ALLOC_INITIALIZER(),       /* db_alloc */
      DB_INDEXTYPE_DEFAULT,         /* index_type */
      PKEY_INDEX_NAME,              /* index_name */
      DB_PRIMARY_INDEX,             /* index_mode */
      DB_ARRAY_DIM(pkey_fields),    /* nfields */
      pkey_fields },                /* fields */
};

/* Database schemas. */
static db_tabledef_t tables[] =
{
    {
        DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        STORAGE_TABLE,
        DB_ARRAY_DIM(storage_fields),
        storage_fields,
        DB_ARRAY_DIM(indexes),   // Indexes array size
        indexes,                 // Indexes array
        0, NULL,
    },
};

dbs_schema_def_t db_schema =
{
    DB_ARRAY_DIM(tables),
    tables
};
