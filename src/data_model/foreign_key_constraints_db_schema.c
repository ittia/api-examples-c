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

#include "foreign_key_constraints_db_schema.h"

static db_fielddef_t storage_fields[] =
{
    { UNITID_FNO,       "unitid",          DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { BARCODEID_FNO,    "barcodeid",       DB_COLTYPE_SINT32,      0,                0, DB_NULLABLE, 0 },
    { ARTICLEID_FNO,    "articleid",       DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { ARTICLENAME_FNO,  "aritclename",     DB_COLTYPE_ANSISTR,     MAX_NAME_LEN,     0, DB_NOT_NULL, 0 },
};
static db_indexfield_t storage_pkey_fields[] = { { ARTICLEID_FNO },  };

static db_indexdef_t storage_indexes[] =
{

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      SORAGE_PKEY_INDEX_NAME,   /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(storage_pkey_fields),  /* nfields */
      storage_pkey_fields },              /* fields  */
};

static db_foreign_key_def_t storage_fkeys[] = 
{
    { UNIT_FKEY_NAME,                           /* fk_name   */
      UNITS_TABLE,                              /* ref_table */
      DB_FK_MATCH_SIMPLE,                       /* match_option */
      DB_FK_ACTION_RESTRICT,                    /* update_rule  */
      DB_FK_ACTION_CASCADE,                     /* delete_rule  */
      DB_FK_NOT_DEFERRABLE,                     /* deferrable   */
      DB_FK_CHECK_IMMEDIATE,                    /* check_time   */

      1,                                        /* nfields      */
      {                                         /* fields[ DB_MAX_FOREIGNKEY_FIELD_COUNT ] */
          {
              UNITID_FNO,                       /* org_field    */
              UNITID_FNO,                       /* ref_field - Field number in the referenced table */
          }
      }
    },
    { BARCODE_FKEY_NAME,                        /* fk_name   */
      BARCODES_TABLE,                           /* ref_table */
      DB_FK_MATCH_SIMPLE,                       /* match_option */
      DB_FK_ACTION_CASCADE,                     /* update_rule  */
      DB_FK_ACTION_RESTRICT,                    /* delete_rule  */
      DB_FK_NOT_DEFERRABLE,                     /* deferrable   */
      DB_FK_CHECK_IMMEDIATE,                    /* check_time   */

      1,                                        /* nfields      */
      {                                         /* fields[ DB_MAX_FOREIGNKEY_FIELD_COUNT ] */
          {
              BARCODEID_FNO,                    /* org_field    */
              BARCODEID_FNO,                    /* ref_field - Field number in the referenced table */
          }
      }
    },
};

static db_fielddef_t units_fields[] =
{
    { UNITID_FNO,    "unitid",             DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { UNITNAME_FNO,  "unitname",           DB_COLTYPE_ANSISTR,     MAX_UNAME_LEN,    0, DB_NOT_NULL, 0 },
};
static db_indexfield_t units_pkey_fields[] = { { UNITID_FNO },  };

static db_indexdef_t units_indexes[] =
{

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      UNITS_PKEY_INDEX_NAME,      /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(units_pkey_fields),  /* nfields */
      units_pkey_fields },              /* fields  */
};

static db_fielddef_t barcodes_fields[] =
{
    { BARCODE_FNO,   "barcode",            DB_COLTYPE_ANSISTR,     MAX_BCODE_LEN,    0, DB_NOT_NULL, 0 },
    { BARCODEID_FNO, "barcodeid",          DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
};

static db_indexfield_t barcodes_pkey_fields[] = { { BARCODEID_FNO },  };

static db_indexdef_t barcodes_indexes[] =
{

    { DB_ALLOC_INITIALIZER(),     /* db_alloc */
      DB_INDEXTYPE_DEFAULT,       /* index_type */
      BARCODES_PKEY_INDEX_NAME,   /* index_name */
      DB_PRIMARY_INDEX,           /* index_mode */
      DB_ARRAY_DIM(barcodes_pkey_fields),  /* nfields */
      barcodes_pkey_fields },              /* fields  */
};

static db_tabledef_t tables[] = 
{
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_DEFAULT,   
        UNITS_TABLE,
        DB_ARRAY_DIM(units_fields),
        units_fields,
        DB_ARRAY_DIM(units_indexes),   // Indexes array size
                     units_indexes,    // Indexes array
        0, NULL    // FKey array size    // FKeys array
    },
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_DEFAULT,   
        BARCODES_TABLE,
        DB_ARRAY_DIM(barcodes_fields),
        barcodes_fields,
        DB_ARRAY_DIM(barcodes_indexes),   // Indexes array size
                     barcodes_indexes,    // Indexes array
        0, NULL    // FKey array size    // FKeys array
    },
    { 
        DB_ALLOC_INITIALIZER(), 
        DB_TABLETYPE_DEFAULT,   
        STORAGE_TABLE,
        DB_ARRAY_DIM(storage_fields),
        storage_fields,
        DB_ARRAY_DIM(storage_indexes),   // Indexes array size
                     storage_indexes,    // Indexes array
        DB_ARRAY_DIM(storage_fkeys),     // FKey array size
                     storage_fkeys       // FKeys array
    },
};

dbs_schema_def_t db_schema = 
{
    DB_ARRAY_DIM(tables),
    tables,
    0, NULL // sequences
};

