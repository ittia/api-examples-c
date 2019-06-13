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

/** @file text_import.c
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

#include "csv_import_export.h"

#define EXAMPLE_DATABASE "text_import.ittiadb"

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

static db_t
create_database(char* database_name, dbs_schema_def_t *schema)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

    if (hdb == NULL) {
        return NULL;
    }

    if (dbs_create_schema(hdb, schema) < 0) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }

    return hdb;
}

static const char *csv =
    "int64_field,float64_field,ansi_field,utf8_field\r\n"
    "1,1.243,\"ansi_field\",\"utf8\"\r\n"
    "2,1.253,\"ansi_fklasjdflield\",\"Дискобол\"\r\n"
    "3,1.263,\"ansi_field\",\"utf8\",\r\n"
    "4,1.273,\"ansi_field\",\"utf8\""
;

int
example_main(int argc, char **argv)
{
    // Create database v1 schema
    db_t hdb = create_database( EXAMPLE_DATABASE, &db_schema );
    int rc = EXIT_FAILURE;
    if( hdb ) {
        rc = csv_import( hdb, "storage", csv, strlen(csv), 0 );

        printf("Enter SQL statements or an empty line to exit\n");
        dbs_sql_line_shell(hdb, EXAMPLE_DATABASE, stdin, stdout, stderr);

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
    }

    return rc;
}
