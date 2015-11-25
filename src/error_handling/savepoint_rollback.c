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

/** @file savepoint_rollback.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_schema.h"

#define EXAMPLE_DATABASE "savepoint_rollback.ittiadb"

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

int
do_transaction_with_savepoint( db_t hdb )
{
    db_result_t rc = DB_OK;
    int i;
    int64_t current_packet = 0;
    int is_wrong_packet = 0;
    int in_tx = 0;
    int good_readings = 0;
    int bad_readings = 0;
    db_savepoint_t sp1 = NULL;

    static storage_t readings[ 9 ] = {
        // Good&Bad readings
        { 1, 2, "2015-07-29 01:05:01", "1.9" },
        { 2, 3, "2015-07-29 01:01:01", "11.1" },
        { 1, 1, "2015-07-32 01:01:01", "-1.1" },    //< Invalid date
        { 2, 3, "2015-07-29 01:10:01", "13.5" },
        { 3, 3, "2015-07-30 01:01:01", "1.6" },
        { 3, 1, "2015-02-30 01:20:01", "-f5.0" },   //< Invalid temperature
        { 3, 3, "2015-07-30 01:05:01", "2.7" },
        { 3, 3, "2015-07-30 01:10:01", "3.8" },
        { 3, 3, "2015-07-30 01:10:01", "3.8" },     //< Pkey violaton

    };
    db_row_t row;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't pupulate 'storage' table", NULL );
        return EXIT_FAILURE;
    }

    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    db_begin_tx( hdb, 0 );

    for( i = 0; i < DB_ARRAY_DIM(readings); ++i ) {
        // Set savepoint in "override" mode
        sp1 = db_set_savepoint( hdb, "sp1", DB_SAVEPOINT_OVERRIDE );
        if( sp1 ) {
            // Try to insert reading
            if( DB_OK == db_insert(c, row, &readings[i], 0) ) {
                good_readings++;
                continue;
            } else if( DB_OK == db_rollback_savepoint( hdb, sp1) )
            {   // If insert failed, do rollback to sp1, skip reading & try with next one
                bad_readings++;
                clear_db_error();
                continue;
            }
        }
        print_error_message( "Can't insert reading", c );
    }
    if( db_is_active_tx(hdb) && DB_OK == db_commit_tx( hdb, 0 ) ) {
        fprintf(stdout, "Count of readings (overall/good/bad): (%d/%d/%d). Overall - good - bad: %d\n",
                DB_ARRAY_DIM( readings ), good_readings, bad_readings,
                DB_ARRAY_DIM( readings ) - good_readings - bad_readings
                );
    }

    db_free_row( row );
    db_close_cursor(c);

    return DB_ARRAY_DIM( readings ) == good_readings + bad_readings ? EXIT_SUCCESS : EXIT_FAILURE;
}


int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    db_t hdb;                   // db to create and insert rows

    hdb = create_database( EXAMPLE_DATABASE, &db_schema, NULL );
    if (!hdb) {
        goto exit;
    }

    // Start transactions with savepoints
    rc = do_transaction_with_savepoint( hdb );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

exit:
    return rc;
}
