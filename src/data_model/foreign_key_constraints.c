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

/** @file foreign_key_constraints.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foreign_key_constraints_db_schema.h"

#define EXAMPLE_DATABASE "foreign_key_constraints.ittiadb"

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
load_table( 
    db_t hdb, const char *table_name, void * data, size_t data_size, size_t data_count, const db_bind_t *binds, size_t binds_count
)
{
    db_result_t db_rc = DB_OK;
    db_row_t row;
    db_cursor_t c;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    
    c = db_open_table_cursor(hdb, table_name, &p);

    row = db_alloc_row( binds, binds_count );
    if( row ) {
        int i;
        for (i = 0; i < data_count; ++i) {
            uint8_t* object = &((uint8_t*)data)[i * data_size];
            db_rc = db_insert(c, row, object, 0);

            if (DB_OK != db_rc) {
                break;
            }
        }

        if (DB_OK == db_rc) {
            db_free_row(row);
            db_close_cursor(c);
            return EXIT_SUCCESS;
        }
    }

    fprintf(stderr, "Couldn't populate table %s", table_name );
    print_error_message( "", c );

    db_free_row( row );
    db_close_cursor( c );

    return EXIT_FAILURE;
}

int 
load_data( db_t hdb )
{    
    barcodes_db_row_t bcodes[] = { 
        { 1, "1234567890123" },
        { 2, "2345678901231" },
        { 3, "3456789012312" },
    };
    units_db_row_t units[] = { 
        { 1, "pcs" },
        { 2, "kg"  },
        { 3, "m"   },
    };
    storage_db_row_t articles[] = { 
        { 1, 1, 1, "Painbrush" },
        { 2, 2, 2, "Sugar"     },
        { 3, 3, 3, "Fabric"    },
    };
    int rc = EXIT_FAILURE;
    db_begin_tx( hdb, 0 );
    rc = 
        load_table( hdb, BARCODES_TABLE, bcodes, sizeof(barcodes_db_row_t), DB_ARRAY_DIM( bcodes ), 
                    barcodes_binds_def, DB_ARRAY_DIM(barcodes_binds_def) )
        || load_table( hdb, UNITS_TABLE, units, sizeof(units_db_row_t), DB_ARRAY_DIM( units ), 
                       units_binds_def, DB_ARRAY_DIM(units_binds_def) )
        || load_table( hdb, STORAGE_TABLE, articles, sizeof(storage_db_row_t), DB_ARRAY_DIM( articles ), 
                       storage_binds_def, DB_ARRAY_DIM(storage_binds_def) )
        ;
    if( EXIT_SUCCESS == rc )
        db_commit_tx( hdb, 0 );

    return rc;
}

int
do_fkey_changes( db_t hdb )
{
    int rc = EXIT_FAILURE;
    db_result_t db_rc = DB_OK;
    db_row_t row;
    db_cursor_t c;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    int32_t barcodeid, unitid;

    // Try to delete from delete-restrict referenced record. ITTIA must reject deletion.
    db_begin_tx( hdb, 0 );
    c = db_open_table_cursor(hdb, BARCODES_TABLE, &p);
    if( DB_OK == db_seek_first(c) ) {
        if( DB_OK == db_delete( c, 0 ) ) {
            fprintf( stderr, "DB missingly let to delete delete-restricted refferencing row\n" );
            db_commit_tx( hdb, 0 );
            goto exit;
        }
        clear_db_error();
        db_abort_tx( hdb, 0 );
    }

    // Try to update update-cascade referenced record. ITTIA must accept update.
    db_begin_tx( hdb, 0 );
    row = db_alloc_row( NULL, 1 );
    dbs_bind_addr( row, BARCODEID_FNO, DB_VARTYPE_SINT32, 
                   &barcodeid, sizeof(barcodeid), NULL );
    barcodeid = 4;
    if( DB_OK != db_update( c, row, 0 ) ) {
        print_error_message( "Couldn't update update-cascade referenced record", c );
        goto exit;
    }
    db_commit_tx( hdb, 0 );

    db_free_row(row);
    // Try to delete delete-cascade referenced record. ITTIA must accept deletion.
    db_close_cursor( c );
    c = db_open_table_cursor(hdb, UNITS_TABLE, &p);
    if( DB_OK == db_seek_first(c) ) {
        if( DB_OK != db_delete( c, DB_DELETE_SEEK_NEXT ) ) {
            print_error_message( "Couldn't delete delete-cascade referenced record", c );
            goto exit;
        }
        db_commit_tx( hdb, 0 );
    }

    // Try to update update-restrict referenced record. ITTIA must reject update.
    db_begin_tx( hdb, 0 );
    row = db_alloc_row( NULL, 1 );
    dbs_bind_addr( row, UNITID_FNO, DB_VARTYPE_SINT32, 
                   &unitid, sizeof(unitid), NULL );
    unitid = 4;
    if( DB_OK == db_update( c, row, 0 ) ) {
        fprintf( stderr, "DB missingly let to update update-restricted refferencing row\n" );
        goto exit;
    }
    db_commit_tx( hdb, 0 );
    rc = EXIT_SUCCESS;

  exit:
    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

int
example_main(int argc, char **argv) 
{
    int rc = EXIT_FAILURE;
    db_t hdb;                   // db to create and insert rows
    
    hdb = create_database( EXAMPLE_DATABASE, &db_schema, NULL );
    if (!hdb)
        goto exit;

    if( 0 == load_data( hdb ) ) {
        // Start transactions generation. Part of transactions commit with DB_LAZY_COMPLETION flag to make them to be deffered
        rc = do_fkey_changes( hdb );
    }
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

  exit:
    return rc;
}

