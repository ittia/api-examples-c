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

/** @file sql_export.c
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

#include "csv_import_export.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
static int snprintf( char *outBuf, size_t size, const char *format, ... )
{
    int count = -1;
    va_list ap;

    va_start( ap, format );
    if (size != 0) {
        count = _vsnprintf_s( outBuf, size, _TRUNCATE, format, ap );
    }
    if (count == -1) {
        count = _vscprintf( format, ap );
    }
    va_end( ap );

    return count;
}
#endif

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

/*
 *  Export field definition
 */
int export_field( db_fielddef_t * fdef )
{
    fprintf( stdout, "\t%s\t", fdef->field_name );

    switch((int)(fdef->field_type)) {
    case DB_COLTYPE_SINT8_TAG:      fprintf( stdout, "sint8" ); break;
    case DB_COLTYPE_UINT8_TAG:      fprintf( stdout, "uint8" ); break;
    case DB_COLTYPE_SINT16_TAG:     fprintf( stdout, "sint16" ); break;
    case DB_COLTYPE_UINT16_TAG:     fprintf( stdout, "uint16" ); break;
    case DB_COLTYPE_SINT32_TAG:     fprintf( stdout, "sint32" ); break;
    case DB_COLTYPE_UINT32_TAG:     fprintf( stdout, "uint32" ); break;
    case DB_COLTYPE_SINT64_TAG:     fprintf( stdout, "sint64" ); break;
    case DB_COLTYPE_UINT64_TAG:     fprintf( stdout, "sint64" ); break;
    case DB_COLTYPE_FLOAT32_TAG:    fprintf( stdout, "float32" ); break;
    case DB_COLTYPE_FLOAT64_TAG:    fprintf( stdout, "float64" ); break;
        // case DB_COLTYPE_FIXED_TAG:      
    case DB_COLTYPE_CURRENCY_TAG:   fprintf( stdout, "currency" ); break;
    case DB_COLTYPE_DATE_TAG:       fprintf( stdout, "date" ); break;
    case DB_COLTYPE_TIME_TAG:       fprintf( stdout, "time" ); break;
    case DB_COLTYPE_DATETIME_TAG:   fprintf( stdout, "datetime" ); break;
    case DB_COLTYPE_TIMESTAMP_TAG:  fprintf( stdout, "timestamp" ); break;
    case DB_COLTYPE_ANSISTR_TAG:    fprintf( stdout, "ansistr(%d)", fdef->field_size ); break;
#ifndef DB_EXCLUDE_UNICODE
    case DB_COLTYPE_UTF8STR_TAG:    fprintf( stdout, "utf8str(%d)", fdef->field_size ); break;
    case DB_COLTYPE_UTF16STR_TAG:   fprintf( stdout, "utf16str(%d)", fdef->field_size ); break;
    case DB_COLTYPE_UTF32STR_TAG:   fprintf( stdout, "utf32str(%d)", fdef->field_size ); break;
#endif
    case DB_COLTYPE_BINARY_TAG:     fprintf( stdout, "varbinary(%d)", fdef->field_size ); break;
    case DB_COLTYPE_BLOB_TAG:       fprintf( stdout, "blob" ); break;
    default:
        return EXIT_FAILURE;
    };
    if( DB_NOT_NULL && fdef->field_flags )
        fprintf( stdout, " NOT NULL" );

    return EXIT_SUCCESS;
}

typedef enum {
    SCHEMA_AND_DATA,
    POST_DATA
} export_stage_t;

typedef enum {
    ALL_INDEXES,
    PKEYS_ONLY,
    ALL_BUT_PKEYS,
} index_export_filter_t;

/*
 *  Export indexes
 */
int
export_indexes( db_t hdb, db_tabledef_t * tdef, export_stage_t stage, index_export_filter_t filter )
{
    int rc = EXIT_SUCCESS;
    int idx;
    int fidx;
    for( idx = 0; idx < tdef->nindexes; ++idx ) {
        db_indexdef_t * idef = &tdef->indexes[ idx ];
        if( idef->index_mode && DB_PRIMARY_INDEX ) {
            if( filter != ALL_BUT_PKEYS ) {
                if( POST_DATA == stage )
                    fprintf( stdout, "ALTER TABLE %s ADD ", tdef->table_name );
                if( idef->index_name[0] )
                    fprintf( stdout, "CONSTRAINT %s ", idef->index_name );

                fprintf( stdout, "PRIMARY KEY ( " );
                for( fidx = 0; fidx < idef->nfields; ++fidx ) {
                    if( fidx ) fputs( ", ", stdout );
                    fprintf( stdout, "%s", tdef->fields[ idef->fields[fidx].fieldno ].field_name  );
                }
                fprintf( stdout, " )%s", stage == POST_DATA ? ";\n" : "" );
            }
        } else if( ALL_INDEXES == filter || ALL_BUT_PKEYS ) {
            fprintf( stdout, "CREATE INDEX %s ON %s( ", idef->index_name, tdef->table_name );
            for( fidx = 0; fidx < idef->nfields; ++fidx ) {
                if( fidx ) fputs( ", ", stdout );
                fprintf( stdout, "%s", tdef->fields[ idef->fields[fidx].fieldno ].field_name  );
            }
            fprintf( stdout, " );\n" );
        }
    }
    return rc;
}

/*
 *  Export table schema & data. 
 *  Export table's data before foreign keys, indexes and primary keys ( except clustered tables ). 
 */
int
export_table( db_t hdb, const char * tname, export_stage_t stage )
{
    int rc = EXIT_FAILURE;
    db_tabledef_t tdef = { DB_ALLOC_INITIALIZER() };
    int fieldno;
    
    if( DB_OK == db_describe_table( hdb, tname, &tdef, DB_DESCRIBE_TABLE_FIELDS | DB_DESCRIBE_TABLE_INDEXES ) ) {
        if( SCHEMA_AND_DATA == stage ) {
            fprintf( stdout, "-- ==== Schema of table %s\n", tname );
            fprintf( stdout, "CREATE %s TABLE %s (\n", 
                     tdef.table_type == DB_TABLETYPE_MEMORY ? "MEMORY" : "", tname
                );
            for( fieldno = 0, rc = 0; fieldno < tdef.nfields && 0 == rc; ++fieldno ) {
                db_fielddef_t *fdef = &tdef.fields[fieldno];
                if( fieldno )
                    fputs( ",\n", stdout );
                rc = export_field( fdef );
            }
            if( EXIT_SUCCESS == rc ) {
                if( DB_TABLETYPE_CLUSTERED == tdef.table_type ) {
                    fputs( ",\n\t", stdout );
                    rc = export_indexes( hdb, &tdef, stage, PKEYS_ONLY );
                }
                fprintf( stdout, "\n) %s;\n", DB_TABLETYPE_CLUSTERED == tdef.table_type ? "CLUSTER BY PRIMARY KEY" : "" );
            }
            if( EXIT_SUCCESS == rc && DB_TABLETYPE_MEMORY != tdef.table_type ) {
                char lprefix[ DB_MAX_OBJECT_NAME + 20 ];
                snprintf( lprefix, DB_MAX_OBJECT_NAME + 20, "INSERT INTO %s VALUES( ", tdef.table_name );
                csv_export_options_t opts = { NO_HEADER, TABLE_SOURCE, '\'', lprefix, " );" };
                rc = export_data( hdb, tname, 0, &opts );
            }

        } else {
            // Post data definitions
            rc = export_indexes( hdb, &tdef, stage, DB_TABLETYPE_CLUSTERED == tdef.table_type ? ALL_BUT_PKEYS : ALL_INDEXES );
        }

    } else 
        print_error_message( NULL, "raised by db_describe_table() on %s table", tname );
        
    return rc;
}

/*
 *  Export all tables' schema (without fkeys and indexes) and data
 */
int
export_tables( db_t hdb, export_stage_t stage )
{
    int rc = EXIT_FAILURE;

    db_cursor_t sql_cursor = NULL;
    db_row_t    r = NULL;
    int tables = 0;
    char tname[ DB_MAX_OBJECT_NAME + 1 ];

    if( 0 == execute_command( hdb, "select table_name from tables where table_id >= 23", &sql_cursor, NULL ) ) {
        r = db_alloc_row( NULL, 1 );
        dbs_bind_addr( r, 0, DB_VARTYPE_ANSISTR, &tname, DB_ARRAY_DIM(tname), 0 );
        for( rc = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ) && 0 == rc; db_seek_next( sql_cursor ), ++tables ) { 
            db_fetch( sql_cursor, r, NULL );
            rc = export_table( hdb, tname, stage );
            fputs( "\n", stdout );
        }

        db_free_row( r );
        db_close_cursor( sql_cursor );
    }
    
    return rc;
}

/*
 *  Export sequences
 */
int
export_sequences( db_t hdb )
{
    int rc = EXIT_FAILURE;
    db_cursor_t sql_cursor = NULL;
    db_row_t    r = NULL;
    int sequences = 0;
    char sname[ DB_MAX_OBJECT_NAME + 1 ];

    if( 0 == execute_command( hdb, "select sequence_name from sequences", &sql_cursor, NULL ) ) {
        r = db_alloc_row( NULL, 1 );
        dbs_bind_addr( r, 0, DB_VARTYPE_ANSISTR, &sname, DB_ARRAY_DIM(sname), 0 );
        rc = 0;
        for( db_seek_first( sql_cursor ); !db_eof( sql_cursor ) && 0 == rc; db_seek_next( sql_cursor ), ++sequences ) {
            db_seqdef_t seq_def;
            db_fetch( sql_cursor, r, NULL );
            if( DB_OK == db_describe_sequence( hdb, sname, &seq_def ) )
                fprintf( stdout, "CREATE SEQUENCE %s START WITH %"PRId64";\n", sname, seq_def.seq_start.int64 );
        }

        db_free_row( r );
        db_close_cursor( sql_cursor );
        rc = EXIT_SUCCESS;
    }
    
    return rc;
}

int
example_main(int argc, char **argv) 
{
    db_t hdb;
    if( argc < 2 ) {
        fprintf(
            stdout, "Usage:\n"
            " %s <existing ittia database>\n",
            argv[0]    
            );
        return EXIT_FAILURE;
    }

    /* Open an existing file storage database with default parameters. */
    hdb = db_open_file_storage(argv[1], NULL);

    if (hdb == NULL) {
        fprintf(stderr, "Couldn't open database file: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int rc = export_tables( hdb, SCHEMA_AND_DATA )
        || export_tables( hdb, POST_DATA )
        || export_sequences( hdb )
        ;
    
    
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}

