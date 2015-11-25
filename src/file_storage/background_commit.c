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

/** @file background_commit.c
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

#define EXAMPLE_DATABASE "background_commit.ittiadb"

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

db_t
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

typedef struct {
    int lazy_tx;
    int forced_tx;
} trans_stat_t;

int check_data( db_t hdb, const trans_stat_t * stat )
{
    int rc = EXIT_FAILURE;
    db_result_t db_rc = DB_OK;

    db_row_t row;

    storage_t row_data;
    db_cursor_t c;  // Cursor to scan table to check
    // Cursor parameters
    db_table_cursor_t p = {
        PKEY_INDEX_NAME,   //< Index by which to sort the table
        DB_SCAN_FORWARD | DB_LOCK_DEFAULT
    };

    // Open cursor
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );

    db_rc = db_seek_first(c);

    // Scan table and check that pkey values are monotonically increase
    {
        int counter = 0;
        trans_stat_t s = { 0, 0 };
        for(; !db_eof(c) && DB_OK == db_rc; db_rc = db_seek_next(c) )
        {
            db_rc = db_fetch(c, row, &row_data);
            if( row_data.f1 != ++counter ) {
                break;
            }
            s.lazy_tx += row_data.f2 == 50 ? 1 : 0;
            s.forced_tx += row_data.f2 == 16 ? 1 : 0;
        }

        if( DB_OK != db_rc ) {
            print_error_message( "Error to scan table of backup db\n", c );
        }
        else if( row_data.f1 != counter ) {
            fprintf( stderr, "Pkey field values sequence violation detected in backup db: (%" PRId64 ", %d)\n",
                     row_data.f1, counter
                     );
        }
        else if( s.lazy_tx != stat->lazy_tx ) {
            fprintf( stderr, "Unexpected count of records which was commited in lazy-commit mode: %d, but expected %d.\n",
                     s.lazy_tx, stat->lazy_tx );
        }
        else if( s.forced_tx != stat->forced_tx ) {
            fprintf( stderr, "Unexpected count of records which was commited in force-commit mode: %d, but expected %d.\n",
                     s.forced_tx, stat->forced_tx );
        }
        else {
            fprintf( stdout, "%d records is inside\n", counter );
            rc = EXIT_SUCCESS;
        }
    }

    db_close_cursor( c );
    db_free_row( row );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}

int
perform_transactions( db_t hdb, trans_stat_t *stat )
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
    for( i = 0; i < 100 && DB_OK == db_rc; ++i ) {
        int do_lazy_commit = i % 5;
        db_rc = db_begin_tx( hdb, 0 );
        row2ins.f1 = i+1;
        row2ins.f2 = do_lazy_commit ? 50 : 16;
        db_rc = db_insert(c, row, &row2ins, 0);
        db_commit_tx( hdb, do_lazy_commit ? DB_LAZY_COMPLETION : DB_DEFAULT_COMPLETION );
        if( do_lazy_commit ) {
            stat->lazy_tx++;
        }
        else {
            stat->forced_tx++;
        }

        if( i % 30 ) {
            db_flush_tx( hdb, DB_FLUSH_JOURNAL );
        }
    }

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
    db_t hdb;                   // db to create and insert rows
    trans_stat_t stat = { 0, 0 };

    hdb = create_database( EXAMPLE_DATABASE, &db_schema, NULL );
    if (!hdb) {
        goto exit;
    }

    // Start transactions generation. Part of transactions commit with DB_LAZY_COMPLETION flag to make them to be deferred
    rc = perform_transactions( hdb, &stat );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
    if( rc != EXIT_SUCCESS ) {goto exit; }

    rc = EXIT_FAILURE;
    // Reopen DB and check if all transactions are really there
    hdb = db_open_file_storage(EXAMPLE_DATABASE, NULL);
    if (!hdb) {
        goto exit;
    }

    rc = check_data( hdb, &stat );


exit:
    return rc;

}
