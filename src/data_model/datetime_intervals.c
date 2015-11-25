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

/** @file datetime_intervals.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "portable_inttypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define EXAMPLE_DATABASE "datetime_intervals.ittiadb"

#define STORAGE_TABLE           "storage"
#define STORAGE_PKEY_INDEX_NAME  "pkey_storage"

#define DATETIME_FNO 0
#define DATE_FNO 1
#define TIME_FNO 2

#define MAX_TEXT_LEN 50

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

int
datetime_example( db_t hdb )
{
    int         rc = EXIT_FAILURE;
    db_cursor_t sql_cursor = NULL;
    db_row_t    r = NULL;
    int         i;
    int32_t     d1, d2, d3;
    char        char_data[50], char_data1[50];

    #define DT "2015-01-01"
    #define TM "13:59:48"
    #define TMSP "2015-01-01 13:59:59.123456"
    const char * b =
        "create table storage ("
        "  d  date not null,"
        "  t  time not null,"
        "  dt timestamp not null"
        ")";

    // Insert data using standard & custom date/time formats
    const char * ins_stmts[] = {
        // Insert using standart format strings
        "insert into storage (d, t, dt)"
        "  values( '"DT "', '"TM "', '"TMSP "' )",
        // Insert using custom format
        "insert into storage (d, t, dt)"
        "  values( date_parse('15 31 3', 'YY DD M'),"
        "          '07:09:04',"
        "          date_parse('Fri Dec 13, 2001 at 5PM', 'DDD MMM D\",\" YYYY \"at\" htt', '')  )",

    };

    if( execute_command(hdb, b, NULL, NULL) ) {
        printf( "creating storage table\n" );
        return rc;
    }

    for( i = 0; i < DB_ARRAY_DIM( ins_stmts); ++i ) {
        if( execute_command( hdb, ins_stmts[i], NULL, NULL ) ) {
            printf("in insert statement at index #%d\n", i );
            return rc;
        }
    }

    // Extracting hours, minutes, and seconds from a time column.
    if( execute_command(
            hdb,
            "select extract( hour from t),"
            "       extract( minute from t ),"
            "       extract( second from t ), t "
            "  from storage",
            &sql_cursor,
            NULL
            )
        )
    {
        printf("while hour, minute, second components extracting from time-type column\n");
        return rc;
    }

    r = db_alloc_cursor_row(sql_cursor);

    if( DB_OK != db_seek_first(sql_cursor) || db_eof(sql_cursor) )
    {
        print_error_message(sql_cursor, NULL );
        goto clean_exit;
    }

    if (DB_OK == db_fetch(sql_cursor, r, NULL)) {
        db_get_field_data(r, 0, DB_VARTYPE_SINT32, &d1, sizeof(d1));
        db_get_field_data(r, 1, DB_VARTYPE_SINT32, &d2, sizeof(d2));
        db_get_field_data(r, 2, DB_VARTYPE_SINT32, &d3, sizeof(d3));
        db_get_field_data(r, 3, DB_VARTYPE_ANSISTR, char_data, DB_ARRAY_DIM(char_data));

        printf("Time: %s. Extracted (hr,mm,ss) : (%02d,%02d,%02d)\n", char_data, d1, d2, d3);
    } else {
        print_error_message(sql_cursor, NULL, "while fetching hour/min/second extraction result" );
        goto clean_exit;
    }

    db_free_row(r);
    db_close_cursor(sql_cursor);

    // Extracting years, months, and days from a date column
    if( execute_command(
            hdb,
            "select extract( year from d ),"
            "       extract( month from d ),"
            "       extract( day from d ), d "
            "  from storage",
            &sql_cursor,
            NULL
            )
        )
    {
        printf("while year, month, day components extracting from date-type column\n");
        goto clean_exit;
    }

    r = db_alloc_cursor_row(sql_cursor);
    if( DB_OK != db_seek_first(sql_cursor) || db_eof(sql_cursor) )
    {
        print_error_message(sql_cursor, NULL );
        goto clean_exit;
    }

    if (DB_OK == db_fetch(sql_cursor, r, NULL)) {
        db_get_field_data(r, 0, DB_VARTYPE_SINT32, &d1, sizeof(d1));
        db_get_field_data(r, 1, DB_VARTYPE_SINT32, &d2, sizeof(d2));
        db_get_field_data(r, 2, DB_VARTYPE_SINT32, &d3, sizeof(d3));
        db_get_field_data(r, 3, DB_VARTYPE_ANSISTR, char_data, DB_ARRAY_DIM(char_data));

        printf( "Date: %s. Extracted (year,month,seconds) : (%02d,%02d,%02d)\n", char_data, d1, d2, d3 );
    } else {
        print_error_message(sql_cursor, NULL, "while fetching hour/min/second extraction result" );
        goto clean_exit;
    }

    db_free_row(r);
    db_close_cursor(sql_cursor);

    // Converting date and time columns to UNIX timestamp.
    {
        int64_t d1, d2;
        db_row_t params;

        if( execute_command(
                hdb,
                "select cast ( ( d - UNIX_EPOCH ) second as bigint ), "
                " cast ( ( t - time '00:00:00' ) second as integer ) "
                "from storage ",
                &sql_cursor,
                NULL
                )
            )
        {
            printf("while converting date and time columns to unix-time format\n");
            goto clean_exit;
        }

        r = db_alloc_cursor_row(sql_cursor);

        if( DB_OK != db_seek_first(sql_cursor) || db_eof(sql_cursor) )
        {
            print_error_message(sql_cursor, NULL );
            goto clean_exit;
        }

        if ( DB_OK == db_fetch(sql_cursor, r, NULL)) {
            db_get_field_data(r, 0, DB_VARTYPE_SINT64, &d1, sizeof(d1));
            db_get_field_data(r, 1, DB_VARTYPE_SINT64, &d2, sizeof(d2));

            printf( "Date, Time: "DT ", "TM " -> Unixtime: %" PRId64 ", %" PRId64 "\n", d1, d2 );
        } else {
            print_error_message(sql_cursor, NULL, "while fetching hour/min/second extraction result" );
            goto clean_exit;
        }

        db_free_row(r);
        db_close_cursor(sql_cursor);

        // Converting date and time columns from UNIX timestamp.
        params = db_alloc_row( NULL, 1 );
        dbs_bind_addr( params, 0, DB_VARTYPE_SINT64, &d1, sizeof( d1 ), 0 );

        if( execute_command(
                hdb,
                "select UNIX_EPOCH + cast( $<integer>0 as interval second )",
                &sql_cursor, params
                )
            )
        {
            printf("while converting unixtime to Date type\n");
            db_free_row(params);
            goto clean_exit;
        }

        db_free_row(params);

        r = db_alloc_cursor_row(sql_cursor);

        if( DB_OK != db_seek_first(sql_cursor) || db_eof(sql_cursor) )
        {
            print_error_message(sql_cursor, NULL );
            goto clean_exit;
        }

        if ( DB_OK == db_fetch(sql_cursor, r, NULL)) {
            db_get_field_data(r, 0, DB_VARTYPE_ANSISTR, &char_data, DB_ARRAY_DIM(char_data));
            printf( "Date: "DT " -> Unixtime: %" PRId64 " -> Date again: %s\n", d1, char_data );
        } else {
            print_error_message(sql_cursor, NULL, "while fetching hour/min/second extraction result" );
            goto clean_exit;
        }
        db_free_row(r);
        db_close_cursor(sql_cursor);
    }

    {
        //Adding months, years, or days to a date column.
        //Adding hours, minutes, or seconds to a time column.
        if( execute_command(
                hdb,
                "select d + cast( 1 as interval day ) + interval '1' month + interval '10' year, "
                " t + cast( 1 as interval hour ) + interval '1' minute + interval '10' second "
                "from storage ",
                &sql_cursor,
                NULL
                )
            )
        {
            printf("while hour, minute, second components extracting from time-type column\n");
            goto clean_exit;
        }

        r = db_alloc_cursor_row(sql_cursor);

        if( DB_OK != db_seek_first(sql_cursor) || db_eof(sql_cursor) )
        {
            print_error_message(sql_cursor, NULL );
            goto clean_exit;
        }

        if (DB_OK == db_fetch(sql_cursor, r, NULL)) {
            db_get_field_data(r, 0, DB_VARTYPE_ANSISTR, char_data, DB_ARRAY_DIM(char_data));
            db_get_field_data(r, 0, DB_VARTYPE_ANSISTR, char_data1, DB_ARRAY_DIM(char_data1));

            printf("Date '"DT "' + 1 day + 1 month + 10 years is: %s\n", char_data);
            printf("Time '"TM "' + 1 hour + 1 minute + 10 seconds is: %s\n", char_data1);
        } else {
            print_error_message(sql_cursor, NULL, "while fetching hour/min/second extraction result" );
            goto clean_exit;
        }

        rc = EXIT_SUCCESS;
    }

clean_exit:
    if( r ) {
        db_free_row(r);
    }
    if( sql_cursor ) {
        db_close_cursor(sql_cursor);
    }

    return rc;
}

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(EXAMPLE_DATABASE, NULL);

    if( hdb ) {
        rc = datetime_example( hdb );
    }

    if( 0 == rc ) {
        db_commit_tx( hdb, 0 );
    }

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}
