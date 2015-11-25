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

#define EXAMPLE_DATABASE "sql_select_query.ittiadb"

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
    if ( 0 == execute_command( hdb,
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
select_query( db_t hdb )
{
    int rc = EXIT_FAILURE;

    db_cursor_t sql_cursor = NULL;
    int tables = 0;
    int row;

    int32_t id, data;
    const db_bind_t row_def[] = {
        DB_BIND_VAR( 0, DB_VARTYPE_SINT32, id ),
        DB_BIND_VAR( 1, DB_VARTYPE_SINT32, data ),
    };
    db_row_t r = db_alloc_row( row_def, DB_ARRAY_DIM( row_def ) );

    /*  Select all rows in a table. */
    if ( 0 == execute_command( hdb, "select id, data from storage", &sql_cursor, NULL ) )
    {
        for ( row = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++row ) {
            db_fetch( sql_cursor, r, NULL );
            process_record( id, data );
        }
        db_close_cursor( sql_cursor );

        fprintf( stdout, "%d rows of \"storage\" table processed\n", row );
    }
    else {
        goto select_query_exit;
    }

    /* Select only the first 30 rows in the table. */
    if ( 0 == execute_command( hdb,
                               "select id, data"
                               "  from storage"
                               "  order by id"
                               "  offset 0 rows"
                               "  fetch first 30 rows only",
                               &sql_cursor,
                               NULL ) )
    {
        for ( row = 0, db_seek_first( sql_cursor );
              !db_eof( sql_cursor );
              db_seek_next( sql_cursor ), ++row
              )
        {
            db_fetch( sql_cursor, r, NULL );
            process_record( id, data );
        }

        fprintf( stdout, "%d rows of \"storage\" table processed\n", row );
    }
    else {
        goto select_query_exit;
    }

    rc = EXIT_SUCCESS;

select_query_exit:

    db_free_row( r );
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

    if ( NULL != hdb ) {
        rc = ins_data( hdb );
    }

    if ( EXIT_SUCCESS == rc ) {
        db_commit_tx( hdb, 0 );
    }

    rc = select_query( hdb );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}
