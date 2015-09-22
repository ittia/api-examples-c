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

#ifndef FKEY_DB_SCHEMA_H_INCLUDED
#define FKEY_DB_SCHEMA_H_INCLUDED

#define UNITID_FNO      0
#define BARCODEID_FNO   1
#define ARTICLEID_FNO   2
#define ARTICLENAME_FNO 3

#define UNITNAME_FNO    1
#define BARCODE_FNO     0

#define STORAGE_TABLE           "storage"
#define UNITS_TABLE             "units"
#define BARCODES_TABLE          "barcodes"

#define SORAGE_PKEY_INDEX_NAME  "pkey_storage"
#define UNIT_FKEY_NAME          "fkey_unit"
#define BARCODE_FKEY_NAME       "fkey_bcode"

#define UNITS_PKEY_INDEX_NAME   "pkey_units"
#define BARCODES_PKEY_INDEX_NAME "pkey_barcodes"

#define MAX_NAME_LEN        100
#define MAX_UNAME_LEN       10
#define MAX_BCODE_LEN       13

extern dbs_schema_def_t db_schema;

//------- Declarations to use while populating table
typedef struct {
    int32_t   unitid;
    int32_t   barcodeid;
    int32_t   articleid;
    db_ansi_t articlename[ MAX_NAME_LEN + 1 ];
} storage_db_row_t;
typedef struct {
    int32_t   unitid;
    db_ansi_t unitname[ MAX_UNAME_LEN + 1 ];
} units_db_row_t;
typedef struct {
    int32_t   barcodeid;
    db_ansi_t barcode[ MAX_BCODE_LEN + 1 ];
} barcodes_db_row_t;

static const db_bind_t storage_binds_def[] = {
    { 
        UNITID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( storage_db_row_t, unitid ),
        DB_BIND_SIZE( storage_db_row_t, unitid ), -1, DB_BIND_RELATIVE
    },
    { 
        BARCODEID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( storage_db_row_t, barcodeid ),
        DB_BIND_SIZE( storage_db_row_t, barcodeid ), -1, DB_BIND_RELATIVE
    },
    { 
        ARTICLEID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( storage_db_row_t, articleid ),
        DB_BIND_SIZE( storage_db_row_t, articleid ), -1, DB_BIND_RELATIVE
    },
    { 
        ARTICLENAME_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( storage_db_row_t, articlename ),
        DB_BIND_SIZE( storage_db_row_t, articlename ), -1, DB_BIND_RELATIVE
    },
};

static const db_bind_t units_binds_def[] = {
    { 
        UNITID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( units_db_row_t, unitid ),
        DB_BIND_SIZE( units_db_row_t, unitid ), -1, DB_BIND_RELATIVE
    },
    { 
        UNITNAME_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( units_db_row_t, unitname ),
        DB_BIND_SIZE( units_db_row_t, unitname ), -1, DB_BIND_RELATIVE
    },
};

static const db_bind_t barcodes_binds_def[] = {
    { 
        BARCODE_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( barcodes_db_row_t, barcode ),
        DB_BIND_SIZE( barcodes_db_row_t, barcode ), -1, DB_BIND_RELATIVE
    },
    { 
        BARCODEID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( barcodes_db_row_t, barcodeid ),
        DB_BIND_SIZE( barcodes_db_row_t, barcodeid ), -1, DB_BIND_RELATIVE
    },
};

#endif  // FKEY_DB_SCHEMA_H_INCLUDED
