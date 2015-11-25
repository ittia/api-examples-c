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

/** @file atomic_file_storage.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "portable_inttypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_schema.h"

#define EXAMPLE_DATABASE "atomic_file_storage.ittiadb"

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

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    int i;
    db_t hdb;
    db_file_storage_config_t storage_cfg;
    static storage_t rows2ins[] = {
        { "ansi_str1",  1,  1.231, "utf8" },
        { "ansi_str2",    2,  2.231, "utf8" },
        { "ansi_str3",    3,  3.231, "УТФ-8" },
        { "ansi_str4",    4,  4.231, "utf8" },
        { "ansi_str5",    5,  5.231, "utf8" },
    };
    db_row_t row;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;
    db_result_t db_rc;

    /* Init storage configuration with default settings. */
    db_file_storage_config_init(&storage_cfg);

    /* Create database with a custom page size. */
    storage_cfg.page_size = DB_DEF_PAGE_SIZE * 2;

    /* Create a new database file. */
    hdb = db_create_file_storage( EXAMPLE_DATABASE, &storage_cfg );

    if (hdb == NULL) {
        print_error_message( "Couldn't create DB", NULL );
        goto exit;
    }

    if (dbs_create_schema( hdb, &db_schema ) < 0) {
        print_error_message( "Couldn't initialize DB objects", NULL );

        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
        /* Remove incomplete database. */
        remove( EXAMPLE_DATABASE );
        goto exit;
    }

    // close db
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    // open db with default params
    hdb = db_open_file_storage(EXAMPLE_DATABASE, NULL);
    if(!hdb) {
        print_error_message( "Couldn't re-open database with default params\n", NULL );
        goto exit;
    }

    // Check the page size of opened db is equal to used on creation
    {
        db_storage_config_t cfg;
        db_rc = db_get_storage_config( hdb, &cfg );
        if( DB_OK != db_rc ) {
            print_error_message( "Couldn't get open database's storage config\n", NULL );
            goto exit;
        }

        if( cfg.u.file_storage.page_size != DB_DEF_PAGE_SIZE * 2 ) {
            fprintf( stderr, "Warning! Unexpected page size %d detected in existing database. Page size %d expected.\n",
                     cfg.u.file_storage.page_size, DB_DEF_PAGE_SIZE * 2
                     );
        }
    }

    // Allocate row
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't allocate db_row_t\n", NULL );
        goto exit;
    }

    // Open cursor
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    db_rc = db_begin_tx( hdb, 0 );
    /* Insert loop. Commit after inserting 3 records; don't commit the last 2 rows. */
    for( i = 0; i < DB_ARRAY_DIM(rows2ins) && DB_OK == db_rc; ++i ) {
        db_rc = db_insert(c, row, &rows2ins[i], 0);
        if( i == 2 ) {
            db_rc = db_commit_tx( hdb, 0 );
            db_rc = db_begin_tx( hdb, 0 );
        }
    }
    if( DB_OK != db_rc ) {
        print_error_message( "Error inserting or commiting\n", c );
    }

    db_close_cursor( c );
    /* Close database with last two rows left uncommitted. */
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    // Reopen DB again & check only commited records are inside
    hdb = db_open_file_storage(EXAMPLE_DATABASE, &storage_cfg);

    // Open cursor
    p.index = PKEY_INDEX_NAME;
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    db_rc = db_seek_first(c);

    // Scan table and check pkey values
    {
        int counter = 0;
        storage_t row_data;
        for(; !db_eof(c) && DB_OK == db_rc; db_rc = db_seek_next(c) )
        {
            db_rc = db_fetch( c, row, &row_data );
            if( row_data.f1 != ++counter ) {
                break;
            }
        }
        if( DB_OK != db_rc ) {
            print_error_message( "Error to scan table after db reopening\n", c );
        }
        else if( row_data.f1 != counter ) {
            fprintf( stderr, "Pkey field values sequence violation detected: (%" PRId64 ", %d)\n",
                     row_data.f1, counter);
        }
        else if( 3 < counter ) {
            fprintf( stderr, "Unexpected records count in table after reopen: %d, but %d expected.\n", 3, counter );
        }
        else if( 3 > counter ) {
            fprintf( stderr, "There is lesser records in table then expected: %d, but expected: %d.\n", counter, 3 );
        }
        else {
            rc = EXIT_SUCCESS;
        }
    }

    db_close_cursor( c );
    db_free_row( row );
    // close source db
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

exit:
    return rc;
}
