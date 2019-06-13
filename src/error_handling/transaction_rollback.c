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

/** @file transaction_rollback.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "dbs_sql_line_shell.h"
#include "portable_inttypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_schema.h"

#define EXAMPLE_DATABASE "transaction_rollback.ittiadb"

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

/** Load packets of readings in DB table. Each packet in personal transaction */
static int
load_readings( db_t hdb )
{
    db_result_t rc = DB_OK;
    int i;
    int64_t current_packet = 0;
    int is_wrong_packet = 0;
    int in_tx = 0;
    int good_packets = 0;

    static storage_t readings[] = {
        // sensorid, packetid, timestamp, temperature
        // Invalid packet
        { 1, 1, "2015-07-32 01:01:01", "-1.1" },
        { 1, 1, "2015-07-29 01:05:01", "-1.2f" },
        { 2, 1, "2015-07-29 01:01:01", "-1.3" },
        { 2, 1, "2015-07-29 25:05:01", "-2.4" },
        { 2, 1, "2015-07-29 01:10:01", "-3.5" },
        { 3, 1, "2015-07-29 01:01:01", "-1.6" },
        { 3, 1, "2015-07-29 01:05:01", "-2.7" },
        { 3, 1, "2015-07-29 01:10:01", "-3.8" },
        { 3, 1, "2015-07-29 01:15:01", "-4.9" },
        { 3, 1, "2015-02-30 01:20:01", "-5.0" },

        // Good packet
        { 1, 2, "2015-07-29 01:01:01", "-1.1" },
        { 1, 2, "2015-07-29 01:05:01", "1.9" },
        { 2, 3, "2015-07-29 01:01:01", "11.1" },
        { 2, 3, "2015-07-29 01:10:01", "13.5" },
        { 3, 3, "2015-07-30 01:01:01", "1.6" },
        { 3, 3, "2015-07-30 01:05:01", "2.7" },
        { 3, 3, "2015-07-30 01:10:01", "3.8" },

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

    for( i = 0; i < DB_ARRAY_DIM(readings) && DB_OK == rc; ++i ) {
        /* Start a new transaction when a new packet is encountered */
        if( current_packet != readings[i].packetid ) {
            /* Commit only if the last packet contained accurate data */
            if( is_wrong_packet ) {
                /* Previous packet had bad data, so roll back its changes */
                if ( DB_OK != (rc = db_abort_tx( hdb, 0 )) ) {
                    print_error_message( "unable to roll back packet data", c );
                }
            }
            else if ( in_tx ) {
                /* Commit previous packet's changes */
                if ( DB_OK == (rc = db_commit_tx( hdb, 0 )) ) {}
                good_packets++;
            }
            else {
                print_error_message( "unable to commit packet data", c );
            }
            is_wrong_packet = 0;
            current_packet = readings[i].packetid;
            rc = db_begin_tx( hdb, 0 );
            in_tx = 1;
        }
        // Try to insert reading
        if( DB_OK != db_insert(c, row, &readings[i], 0) ) {
            // Some error occured on insertion
            int ecode = get_db_error();
            is_wrong_packet = 1;
            // See the kind of error
            if( DB_EINVALIDNUMBER == ecode
                || DB_ECONVERT == ecode
                || DB_EINVTYPE == ecode
                || DB_EDUPLICATE == ecode
                )
            {
                // It seems data conversion error. Continue loading loop
                clear_db_error();
                fprintf( stderr, "Couldn't insert reading. Skip packet %" PRId64 "\n", readings[i].packetid );
            } else {
                // Some sort of system error occured. Break loading loop
                print_error_message( "Unhandled error while insert reading. Break loading", c );
                break;
            }
        }
    }
    // If last good packet's changes is uncommited, commit them now
    if( in_tx && !is_wrong_packet ) {
        if( DB_OK == db_commit_tx( hdb, 0 ) ) {
            good_packets++;
        }
        else {
            print_error_message( "Couldn't commit packet data", c );
        }
    }

    db_free_row( row );
    db_close_cursor(c);

    return good_packets > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
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

    // Start transactions generation. Part of transactions commit with DB_LAZY_COMPLETION flag to make them to be deffered
    rc = load_readings( hdb );

    printf("Enter SQL statements or an empty line to exit\n");
    dbs_sql_line_shell(hdb, EXAMPLE_DATABASE, stdin, stdout, stderr);

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

exit:
    return rc;
}
