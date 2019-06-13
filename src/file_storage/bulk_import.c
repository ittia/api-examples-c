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

/** @file bulk_import.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "dbs_sql_line_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_schema.h"

#define EXAMPLE_DATABASE "bulk_import.ittiadb"

/**
 * Print an error message for a failed database operation.
 */
static void
print_error_message( const char * message, db_cursor_t cursor )
{
    if ( get_db_error() == DB_NOERROR ) {
        if ( NULL != message ) {
            fprintf( stderr, "ERROR: %s\n", message );
        }
    }
    else {
        char * query_message = NULL;
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );
        if ( NULL != cursor ) {
            db_get_error_message( cursor, &query_message );
        }

        fprintf( stderr, "ERROR %s: %s\n", info.name, info.description );

        if ( query_message != NULL ) {
            fprintf( stderr, "%s\n", query_message );
        }

        if ( NULL != message ) {
            fprintf( stderr, "%s\n", message );
        }

        /* The error has been handled by the application, so clear error state. */
        clear_db_error();
    }
}

static db_t
create_database(char* database_name, dbs_schema_def_t *schema, db_file_storage_config_t * storage_cfg)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, storage_cfg);

    if (hdb == NULL) {
        print_error_message( "Couldn't create DB", NULL );
        return NULL;
    }

    if (dbs_create_schema(hdb, schema) < 0) {
        print_error_message( "Couldn't initialize DB objects", NULL );

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }

    return hdb;
}

static int
import_data( db_t hdb )
{
    int i;
    db_row_t row;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;
    db_result_t db_rc;
    storage_t row2ins = { "ansi_str1",  1,  1.231, "utf8" };

    // Allocate row. binds_def see in db_schema.h
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't allocate db_row_t\n", NULL );
        return EXIT_FAILURE;
    }

    // Open cursor
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    db_rc = DB_OK;
    // Insertion loop. Do commit after 3 records inserted only. Don't commit last 2 rows
    db_rc = db_begin_tx( hdb, 0 );

    for( i = 0; i < 100 && DB_OK == db_rc; ++i ) {
        row2ins.f1 = i;
        row2ins.f2 = i * i / 1000.;
        db_rc = db_insert(c, row, &row2ins, 0);
    }

    db_rc = ( DB_OK == db_rc ) ? db_commit_tx( hdb, 0 ) : db_rc;

    if( DB_OK != db_rc ) {
        print_error_message( "Error inserting or commiting\n", c );
    }

    db_close_cursor( c );
    db_free_row( row );

    return DB_OK == db_rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    db_t hdb;
    db_file_storage_config_t storage_cfg;

    db_file_storage_config_init(&storage_cfg);

    /* Create a new database with logging disabled. */
    storage_cfg.file_mode &= ~DB_NOLOGGING;
    hdb = create_database( EXAMPLE_DATABASE, &db_schema, &storage_cfg );
    if ( hdb == NULL ) {
        goto exit;
    }

    // Get the data somewhere and do import
    rc = import_data( hdb );

    printf("Enter SQL statements or an empty line to exit\n");
    dbs_sql_line_shell(hdb, EXAMPLE_DATABASE, stdin, stdout, stderr);

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

exit:
    return rc;

}
