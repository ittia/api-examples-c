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

/** @file text_export.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "csv_import_export.h"
#include "text_exchange_schema.h"

#define EXAMPLE_DATABASE "text_export.ittiadb"

extern dbs_schema_def_t db_schema; //< Declared in text_exchange_schema.c

/**
* Print an error message for a failed database operation.
*/
void
print_error_message( const char * message, ... )
{
    va_list va;

    va_start( va, message );
    if ( get_db_error() == DB_NOERROR ) {
        if ( NULL != message ) {
            fprintf( stderr, "ERROR: " );
            vfprintf( stderr, message, va );
            fprintf( stderr, "\n" );
        }
    }
    else {
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );

        fprintf( stderr, "ERROR %s: %s\n", info.name, info.description );

        if ( NULL != message ) {
            vfprintf( stderr, message, va );
            fprintf( stderr, "\n" );
        }

        /* The error has been handled by the application, so clear error state. */
        clear_db_error();
    }
    va_end( va );
}

int
populate_data( db_t hdb )
{
    /* Populate "storage" table using relative bounds fields. See 
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
        { 1, DB_VARTYPE_SINT64,   DB_BIND_OFFSET( storage_t, f1 ),  DB_BIND_SIZE( storage_t, f1 ), -1, DB_BIND_RELATIVE }, 
        { 2, DB_VARTYPE_FLOAT64,  DB_BIND_OFFSET( storage_t, f2 ),  DB_BIND_SIZE( storage_t, f2 ), -1, DB_BIND_RELATIVE }, 
        { 3, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, f3 ),  DB_BIND_SIZE( storage_t, f3 ), -1, DB_BIND_RELATIVE }, 
    };

    static storage_t rows2ins[] = {
        { "ansi_str1",  1,  1.231, "utf8" },
        { "ansi_str2",    2,  2.231, "utf8" },
        { "ansi_str3",    3,  3.231, "УТФ-8" },
        { "ansi_str4",    4,  4.231, "utf8" },
        { "ansi_str5",    5,  5.231, "utf8" },
    };
    db_row_t row;
    
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't pupulate 'storage' table\n" );
        return EXIT_FAILURE;
    }
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    db_result_t rc = DB_OK;
    int i;
    for( i = 0; i < DB_ARRAY_DIM(rows2ins) && DB_OK == rc; ++i )
        rc = db_insert(c, row, &rows2ins[i], 0);        

    db_free_row( row );
    rc = DB_OK == rc ? db_commit_tx( hdb, 0 ) : rc;

    if( DB_OK != rc ) {
        print_error_message( "Couldn't pupulate 'storage' table\n" );
    }
    db_close_cursor(c);

    return DB_OK == rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

db_t 
create_database(char* database_name, dbs_schema_def_t *schema)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

    if (hdb == NULL)
        return NULL;

    if (dbs_create_schema(hdb, schema) < 0) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }
  
    return hdb;
}


int
example_main(int argc, char **argv) 
{
    db_t hdb = create_database( EXAMPLE_DATABASE, &db_schema );

    int rc = EXIT_FAILURE;
    if( hdb ) {        
        rc = populate_data( hdb );
        if ( EXIT_SUCCESS == rc ) {
            rc = export_data( hdb, STORAGE_TABLE, 0, 0 );
        }
    }

    return rc;
}

