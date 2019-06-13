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

/** @file embedded_database_server.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "dbs_sql_line_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Database schema: table, field, and index definitions. */

#define T_ID 0
#define T_N 1
#define T_S 2

static db_fielddef_t table_t_fields[] = {
    { T_ID, "ID", DB_COLTYPE_UINT32,    0, 0, DB_NOT_NULL, NULL, 0 },
    { T_N,  "N",  DB_COLTYPE_SINT32,    0, 0, DB_NULLABLE, NULL, 0 },
    { T_S,  "S",  DB_COLTYPE_UTF8STR, 100, 0, DB_NULLABLE, NULL, 0 }
};

static db_indexfield_t index_t_id_fields[] = {
    { T_ID, 0 }
};
static db_indexdef_t index_t_id = {
    DB_ALLOC_INITIALIZER(),
    DB_INDEXTYPE_DEFAULT,
    "ID",
    DB_MULTISET_INDEX,
    DB_ARRAY_DIM(index_t_id_fields),
    index_t_id_fields,
};

static db_tabledef_t table_t = {
    DB_ALLOC_INITIALIZER(),
    DB_TABLETYPE_DEFAULT,
    "T",
    DB_ARRAY_DIM(table_t_fields),
    table_t_fields,
    1,  // Indexes array size
    &index_t_id, // Indexes array
    0, NULL,
};

static dbs_schema_def_t db_schema =
{
    1,
    &table_t
};

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
 * Helper function to create DB
 */
static db_t
create_database(char* database_name, dbs_schema_def_t *schema)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

    if (hdb == NULL) {
        print_error_message( NULL, "Couldn't create DB" );
        return NULL;
    }

    if (dbs_create_schema(hdb, schema) < 0) {
        print_error_message( NULL, "Couldn't initialize DB objects" );

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }

    return hdb;
}

#define DB_FILENAME "embedded_database_server.ittiadb"
#define DB_ALIAS    "eds.ittiadb"

int
example_main( int argc, char **argv )
{
    int rc = EXIT_FAILURE;
    db_t hdb;
    int seconds_to_work = 3;

    setbuf( stdout, NULL );
    setbuf( stderr, NULL );

    if ( argc > 1 ) {
        seconds_to_work = atoi( argv[1] );
        if ( !seconds_to_work ) {
            fprintf( stdout, "Usage:\n%s <seconds_to_work>\n", argv[0] );
            return rc;
        }
    }
    /* Create a database to share with clients */
    hdb = create_database( DB_FILENAME, &db_schema );
    if ( !hdb ) {
        goto exit;
    }

    /* A storage cannot be mounted while open */
    db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );

    if ( DB_OK == db_mount_storage( DB_FILENAME, DB_ALIAS, NULL ) ) {
        db_server_config_t srv_config;
        db_server_config_init( &srv_config );

        /* Start ITTIA DB Server in this process */
        if ( DB_OK == db_server_start( &srv_config ) ) {
            fprintf( stdout, "Server started in background threads.\n" );
            fprintf( stdout, "Database file [%s] mounted with alias [%s].\n", DB_FILENAME, DB_ALIAS );
            fprintf( stdout, "You have %d second(s) to open a connection.\n", seconds_to_work );

            /* This process can still open the mounted database directly, even
             * while it is shared with other processes by the server. */
            hdb = db_open_file_storage( DB_FILENAME, NULL );

            printf("Enter SQL statements or an empty line to exit\n");
            dbs_sql_line_shell(hdb, DB_FILENAME, stdin, stdout, stderr);

            db_server_stop( 0 );

            rc = EXIT_SUCCESS;
        }
    }

exit:

    if ( NULL != hdb ) {
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
    }

    return rc;
}
