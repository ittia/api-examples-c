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

/** @file sql_select_query.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#define EXAMPLE_DATABASE "sql_parameters.ittiadb"

/**
 * Print an error message for a failed database operation.
 */
static void
print_error_message( db_cursor_t cursor, const char * message, ... )
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
            vfprintf( stderr, message, va );
            fprintf( stderr, "\n" );
        }

        /* The error has been handled by the application, so clear error state. */
        clear_db_error();
    }
    va_end( va );
}

/**
 * Execute an SQL statement, checking for errors.
 */
static int
execute_command( db_t hdb, const char* stmt, db_cursor_t* cursor, db_row_t parameters )
{
    db_cursor_t sql_c;

    sql_c = db_prepare_sql_cursor( hdb, stmt, 0 );

    if ( NULL == sql_c ) {
        print_error_message( NULL, "prepare SQL failed" );
        return EXIT_FAILURE;
    }

    if ( !db_is_prepared( sql_c ) ) {
        print_error_message( sql_c, "prepare SQL failed" );
        db_close_cursor( sql_c );
        return EXIT_FAILURE;
    }

    if ( db_execute( sql_c, parameters, NULL ) != DB_OK ) {
        print_error_message( sql_c, "execute SQL failed" );
        db_close_cursor( sql_c );
        return EXIT_FAILURE;
    }

    /* Close the cursor, unless the caller requests it. */
    if ( NULL != cursor ) {
        *cursor = sql_c;
    }
    else {
        db_close_cursor( sql_c );
    }

    return EXIT_SUCCESS;
}

static int
ins_data( db_t hdb )
{
    if( 0 == execute_command( hdb,
                              "create table storage ("
                              "  id sint32 not null primary key,"
                              "  data sint32"
                              ")",
                              NULL, NULL
                              ) &&
        0 == execute_command( hdb,
                              "insert into storage (id, data)"
                              "  select N, 100 - N from $nat(100)",
                              NULL, NULL
                              )
        )
    {
        return EXIT_SUCCESS;
    }
    print_error_message( NULL, "creating & inserting data into \"storage\" table" );
    return EXIT_FAILURE;
}

static void
process_record( int32_t id, int32_t data )
{
    /* Stub function: process a record from the database. */
}

static int
select_parametrized_query( db_t hdb )
{
    int rc = EXIT_FAILURE;

    db_cursor_t sql_cursor = NULL;
    int tables = 0;
    int rows;

    int32_t id, data;
    const db_bind_t row_def[] = {
        DB_BIND_VAR( 0, DB_VARTYPE_SINT32, id ),
        DB_BIND_VAR( 1, DB_VARTYPE_SINT32, data ),
    };
    db_row_t r = db_alloc_row( row_def, DB_ARRAY_DIM( row_def ) );

    int32_t id_param, data_param;
    const db_bind_t params_def[] = {
        DB_BIND_VAR( 0, DB_VARTYPE_SINT32, id_param ),
        DB_BIND_VAR( 1, DB_VARTYPE_SINT32, data_param ),
    };
    db_row_t params_row = db_alloc_row( params_def, DB_ARRAY_DIM( params_def ) );

    int32_t offset_param, ffirst_param;

    /* Prepare query with two parameter placeholders */
    sql_cursor = db_prepare_sql_cursor( hdb,
                                        "select id, data"
                                        "  from storage"
                                        "  where id > ? and data > ?",
                                        0
                                        );

    if( NULL == sql_cursor ) {
        print_error_message( sql_cursor, "preparing parametrized query" );
        goto select_query_exit;
    }

    /* Set query parameters */
    id_param = 98;
    data_param = 0;

    /* Execute prepared cursor */
    if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
        print_error_message( sql_cursor, "executing parametrized query" );
        goto select_query_exit;
    }
    for( rows = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++rows ) {
        db_fetch( sql_cursor, r, NULL );
        process_record( id, data );
    }

    fprintf( stdout, "%d rows of 'storage' table processed with (id, data) = (%d, %d)\n", rows, id_param, data_param );
    /* Re-execute with changed parameters */
    db_unexecute( sql_cursor );
    id_param--; data_param++;

    if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
        print_error_message( sql_cursor, "executing select with (id, data) = (%d, %d)", id_param, data_param );
        goto select_query_exit;
    }
    for( rows = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++rows ) {
        db_fetch( sql_cursor, r, NULL );
        process_record( id, data );
    }
    fprintf( stdout, "%d rows of 'storage' table processed with (id, data) = (%d, %d)\n", rows, id_param, data_param );

    /* Re-execute with changed parameters */
    db_unexecute( sql_cursor );
    id_param--;

    if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
        print_error_message( sql_cursor, "executing select with (id, data) = (%d, %d)", id_param, data_param );
        goto select_query_exit;
    }
    for( rows = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++rows ) {
        db_fetch( sql_cursor, r, NULL );
        process_record( id, data );
    }
    fprintf( stdout, "%d rows of 'storage' table processed with (id, data) = (%d, %d)\n", rows, id_param, data_param );

    db_free_row( params_row );
    db_close_cursor( sql_cursor );

    /* Parameterize FETCH FIRST and OFFSET clauses to read the entire table in pages. */

    sql_cursor = db_prepare_sql_cursor(
            hdb,
            "select id, data"
            "  from storage"
            "  order by id"
            "  offset ? rows"
            "  fetch first ? rows only",
            0
            );
    if( NULL == sql_cursor ) {
        print_error_message( sql_cursor, "preparing parametrized query" );
        goto select_query_exit;
    }

    params_row = db_alloc_row( NULL, 2 );
    dbs_bind_addr( params_row, 0, DB_VARTYPE_SINT32, &offset_param, sizeof(int32_t), 0 );
    dbs_bind_addr( params_row, 1, DB_VARTYPE_SINT32, &ffirst_param, sizeof(int32_t), 0 );

    offset_param = 0; ffirst_param = 10;

    do {
        /* Execute prepared cursor */
        if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
            print_error_message( sql_cursor, "while executing select with (offset, ffirst) = (%d, %d)\n",
                                 offset_param, ffirst_param
                                 );
            goto select_query_exit;
        }

        for( rows = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++rows ) {
            db_fetch( sql_cursor, r, NULL );
            process_record( id, data );
        }
        db_unexecute( sql_cursor );

        fprintf( stdout, "%d rows of \"storage\" table processed with (offset, ffirst) = (%d, %d)\n", rows, offset_param, ffirst_param );

        offset_param += ffirst_param;
    } while (rows > 0);

    rc = EXIT_SUCCESS;

select_query_exit:

    db_free_row( r );
    db_free_row( params_row );
    db_close_cursor( sql_cursor );

    return rc;
}

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;

    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(EXAMPLE_DATABASE, NULL);

    if( NULL != hdb ) {
        rc = ins_data( hdb );
    }

    if( EXIT_SUCCESS == rc ) {
        db_commit_tx( hdb, 0 );
    }

    rc = select_parametrized_query( hdb );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}
