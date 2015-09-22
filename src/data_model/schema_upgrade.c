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

/** @file schema_upgrade.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 * 
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include "schema_upgrade_schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXAMPLE_DATABASE         "schema_upgrade-1.ittiadb"
#define EXAMPLE_UPGRADE_DATABASE "schema_upgrade-2.ittiadb"

/* Schema is defined by data structures in `schema_upgrade_schema*.c` */
extern dbs_versioned_schema_def_t v1_schema;
extern dbs_versioned_schema_def_t v2_schema;

typedef struct {
    uint64_t    id;
    wchar_t     name[MAX_CONTACT_NAME + 1]; /* Extra char for null termination. */
    uint64_t    ring_id;
    db_ansi_t   sex[MAX_SEX_TITLE + 1];
} contact_t;

/**
* Print an error message for a failed database operation.
*/
static void
print_error_message( const char * message, db_cursor_t cursor )
{
    if ( get_db_error() == DB_NOERROR ) {
        if ( NULL != message ) {
            fprintf( stderr, "ERROR: %s\n", message);
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

/**
 * Set schema version of DB
 */
void
set_schema_version( db_t hdb, int version )
{
    /* Use manual binding both to seek and update. */
    db_table_cursor_t p = { NULL, DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE };
    db_cursor_t c;
    uint32_t db_version = 0;
    db_bind_t row_def[] = {
        DB_BIND_VAR( SCHEMA_VERSION_VERSION, DB_VARTYPE_UINT32, db_version )
    };
    
    c = db_open_table_cursor(hdb, SCHEMA_VERSION_TABLE, &p);

    /* Goto 1'st row. */
    if (db_seek_first(c) == DB_FAIL) {
        print_error_message("DB_FAIL in set_schema_version()", c );
        return;
    }

    if (db_eof( c )) {
        db_version = version;
        // No versioned DB yet. Insert row with version
        if (db_qinsert( c, row_def, DB_ARRAY_DIM( row_def ), &db_version, 0 ) == DB_FAIL) {
            print_error_message( "Error in set_schema_version()", c );
        }
    } else if ( db_version != version ) {
        // Update version with a new value
        db_version = version;
        if (db_qupdate( c, row_def, DB_ARRAY_DIM( row_def ), NULL ) == DB_FAIL)
            print_error_message("Error in set_schema_version()", c);
    }
    (void)db_close_cursor(c);
}

/**
   Get schema version of DB
 */
int 
get_schema_version( db_t hdb )
{
    /* Use manual binding both to seek and update. */
    db_table_cursor_t p = { NULL, 0 };
    db_cursor_t c;
    uint32_t db_version = 0;
    db_bind_t row_def[] = {
        DB_BIND_VAR( SCHEMA_VERSION_VERSION, DB_VARTYPE_UINT32, db_version )
    };
    
    c = db_open_table_cursor(hdb, SCHEMA_VERSION_TABLE, &p);

    /* Go to first row. */
    if (db_seek_first(c) == DB_FAIL) {
        print_error_message( "DBFAIL in get_schema_version", c );
        return 0;
    }
    if( db_eof(c) )
        db_version = 0;
    else
        db_qfetch( c, row_def, DB_ARRAY_DIM(row_def), NULL );
    (void)db_close_cursor(c);

    return (int)db_version;
}

/**
 * Initialize the database.
 */
typedef int (* InitDataCallback) ( db_t db, int version );

db_t 
create_database(char* database_name, dbs_versioned_schema_def_t *versioned_schema, InitDataCallback cb)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

    if (hdb == NULL)
        return NULL;

    if (dbs_create_schema(hdb, &versioned_schema->schema) < 0) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }

    if( cb && 0 != (*cb)( hdb, versioned_schema->version ) ) {
        print_error_message( "Couldn't initialize table data in created database", NULL);
        return NULL;
    }
    set_schema_version( hdb, versioned_schema->version );
    db_commit_tx( hdb, DB_DEFAULT_COMPLETION );

    return hdb;
}

typedef int (* UpgradeCallback) ( db_t db, int old_version, int new_version );

db_t 
open_database(char* database_name, dbs_versioned_schema_def_t *versioned_schema, UpgradeCallback cb)
{
    db_t hdb;

    /* Open an existing file storage database with default parameters. */
    hdb = db_open_file_storage(database_name, NULL);

    if (hdb == NULL)
        return NULL;

    db_begin_tx( hdb, DB_DEFAULT_ISOLATION | DB_LOCK_DEFAULT );
    int version = get_schema_version( hdb );
    db_commit_tx( hdb, DB_DEFAULT_COMPLETION );

    int upgrade_rc = 0;
    if( version != versioned_schema->version
        && ( !cb 
             || 0 != ( upgrade_rc = (*cb)( hdb, version, versioned_schema->version ) )
            )
        )
    {
        if( 1 == upgrade_rc )
            fprintf( 
                stderr, "ITTIA evaluation mode can't be used to perform full upgrade actions set,\n"
                "  in this conditions we just suppose upgrade job is successfull\n"
                );
        else {
            print_error_message( "Opened DB has wrong version and it couldn't be upgraded to version requiested", NULL );
            return NULL;
        }
    }
    
    if ( 0 == upgrade_rc && !dbs_check_schema(hdb, &versioned_schema->schema)) {
        fprintf(stderr, "WARNING: schema conflict in %s. Version: %d\n", database_name, versioned_schema->version );
        return NULL;
    }

    db_begin_tx( hdb, DB_DEFAULT_ISOLATION | DB_LOCK_DEFAULT );
    set_schema_version( hdb, versioned_schema->version );
    db_commit_tx( hdb, DB_DEFAULT_COMPLETION );

    //open_sequences(hdb);

    return hdb;
}

int
init_data_cb( db_t hdb, int version )
{
    if( 1 > version || 2 < version ) {  
        print_error_message("Wrong DB schema version. Can't insert data.", NULL);
        return -1;
    }
    fprintf( stdout, "Populating just creating DB v%d with data\n", version );

    /* Populate "contact" table using relative bounds fields. See 
       ittiadb/manuals/users-guide/api-c-database-access.html#relative-bound-fields
    */
    static const db_bind_t binds_def[] = {
        { CONTACT_ID,         DB_VARTYPE_UINT64,    DB_BIND_OFFSET( contact_t, id ),        DB_BIND_SIZE( contact_t, id ),      -1, DB_BIND_RELATIVE },
        { CONTACT_NAME,       DB_VARTYPE_WCHARSTR,  DB_BIND_OFFSET( contact_t, name ),      DB_BIND_SIZE( contact_t, name ),    -1, DB_BIND_RELATIVE }, 
        { CONTACT_RING_ID,    DB_VARTYPE_UINT64,    DB_BIND_OFFSET( contact_t, ring_id ),   DB_BIND_SIZE( contact_t, ring_id ), -1, DB_BIND_RELATIVE }, 
        { CONTACT_SEX,        DB_VARTYPE_ANSISTR,   DB_BIND_OFFSET( contact_t, sex ),       DB_BIND_SIZE( contact_t, sex ),     -1, DB_BIND_RELATIVE }, 
    };

    contact_t contacts2ins[] = {
        { 1, L"Bob",    1, "Mr"     },
        { 2, L"Alice",  1, "Mrs"    },
        { 3, L"Fred",   1, "Mr"     },
        { 4, L"Mary",   1, "Mrs"    },        
    };
    db_row_t row;
    
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );

    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    c = db_open_table_cursor(hdb, CONTACT_TABLE, &p);

    db_result_t rc = DB_OK;
    int i;
    for( i = 0; i < DB_ARRAY_DIM(contacts2ins) && DB_OK == rc; ++i ) {
        if( 2 == version ) {
            if( 0 == strcmp( contacts2ins[i].sex, "Mrs" ) )
                strcpy( contacts2ins[i].sex, "1" );
            else if( 0 == strcmp( contacts2ins[i].sex, "Mr" ) )
                strcpy( contacts2ins[i].sex, "0" );
            else 
                strcpy( contacts2ins[i].sex, "2" );
        }
        rc = db_insert(c, row, &contacts2ins[i], 0);
    }
    db_free_row( row );
    
    if( DB_OK != rc ) 
        print_error_message( "Couldn't pupulate 'contact' table\n", c );

    db_close_cursor(c);

    return DB_OK == rc ? 0 : -1;
}

static db_result_t
upgrade_to_schema_v2( db_t hdb )
{
    db_result_t rc = DB_OK;
    /* 1. Convert string type v1's 'sex_title' column to int-type 'sex' column, as v2 schema declares */
    /* 1.1. Append a new uint8 type 'sex' column */
    db_fielddef_t sex = { CONTACT_NFIELDS, "sex", DB_COLTYPE_UINT8, 0, 0, DB_NULLABLE, 0 };
    
    rc = db_add_field( hdb, CONTACT_TABLE, &sex );
    if( DB_OK != rc ) {
        print_error_message( "Couldn't append 'sex' column on v1->v2 upgrade", 0 );
        return -1;
    }
    
    /* 1.2. Fill 'sex' column with data according to schema: Mr->0, Mrs->1,Other->2 */
    // Use absolute bound fields ( see ittiadb/manuals/users-guide/api-c-database-access.html#absolute-bound-fields )
    db_ansi_t sex_old[ MAX_SEX_TITLE + 1 ];
    db_len_t sex_ind = DB_FIELD_NULL, sex_ind_new = DB_FIELD_NULL;
    db_row_t row;
    uint8_t sex_new = 0;
    db_bind_t binds_def[] = {
        {CONTACT_SEX, DB_VARTYPE_ANSISTR, DB_BIND_ADDRESS(sex_old), sizeof(sex_old), DB_BIND_ADDRESS(&sex_ind), DB_BIND_ABSOLUTE},
        {CONTACT_NFIELDS, DB_VARTYPE_UINT8, DB_BIND_ADDRESS(&sex_new), sizeof(sex_new), DB_BIND_ADDRESS(&sex_ind_new),   DB_BIND_ABSOLUTE},
    };
    row = db_alloc_row( binds_def, 2 );

    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    c = db_open_table_cursor(hdb, CONTACT_TABLE, &p);

    db_begin_tx( hdb, DB_DEFAULT_ISOLATION | DB_LOCK_DEFAULT );
    for ( rc = db_seek_first( c ); DB_OK == rc && !db_eof( c ); db_seek_next( c ) ) {
        rc = db_fetch( c, row, 0 );
        if( 0 == strcmp( "Mr", sex_old ) )
            sex_new = 0;
        else if( 0 == strcmp( "Mrs", sex_old ) )
            sex_new = 1;
        else 
            sex_new = 2;
        sex_ind_new = 0;
        rc = db_update( c, row, 0 );
    }
    db_commit_tx( hdb, DB_DEFAULT_COMPLETION );

    if( DB_OK != rc )
        print_error_message( "Couldn't fill appended 'sex' column with data", c );
   
    db_free_row( row );
    db_close_cursor( c );


    /* 1.3 Drop v1 sex_title column. */
    /* Attention! Dropping columns doesn't avail in ITTIA evaluation version */
    int eval_mode_db = 0;
    rc = db_drop_field( hdb, CONTACT_TABLE, "sex_title" );
    if( DB_OK != rc ) {
        if( DB_EEVALUATION == get_db_error() ) {
            eval_mode_db = 1;
            rc = DB_OK;
            fprintf( stdout, "No db_drop_field() feature supported in ITTIA Evaluation version\n" );
        } else
            print_error_message( "Couldn't drop 'sex_title' v1 schema column", c );
    }

    /* 2. Indexes modifications */
    // 2.1. drop old pkey
    rc = db_drop_index( hdb, CONTACT_TABLE, CONTACT_BY_ID );
    if ( DB_OK != rc ) {
        print_error_message( "Couldn't drop v1 pkey index", c );
    }

    // 2.2. create a new one
    // 2.2.1. Set NOT NULL property on ring_id field
    // Attention. This feature isn't supported in evaluation ittiadb.

    extern db_fielddef_t contact_fields[];   //< Defined in schema_upgrade_schema_v2.c
    rc = db_update_field( hdb, CONTACT_TABLE, "ring_id", &contact_fields[ CONTACT_RING_ID ] );

    if ( DB_OK != rc ) {
        print_error_message( "Couldn't set not null on ring_id column", c );
    }

    // 2.2.2. Create PKey index
    extern db_indexdef_t contact_indexes[];  //< Defined in schema_upgrade_schema_v2.c
    rc = db_create_index( hdb, CONTACT_TABLE, CONTACT_BY_ID_RING_ID, &contact_indexes[0] );

    if ( DB_OK != rc ) {
        print_error_message( "Couldn't create v2 pkey index", c );
    }
}

static db_result_t
upgrade_to_schema_v3( db_t hdb )
{
    /* Schema version 3 is not yet implemented */
    return DB_OK;
}

static int
upgrade_schema_cb( db_t hdb, int old_version, int new_version )
{
    db_result_t rc = DB_OK;

    fprintf( stdout, "Upgrading schema v%d to v%d\n", old_version, new_version );

    if ( old_version < 1 || new_version > 3 ) {
        print_error_message( "Schema upgrade path not supported", NULL );
        return -1;
    }

    if ( DB_OK == rc && old_version < 2 && new_version >= 2 ) {
        rc = upgrade_to_schema_v2( hdb );
    }

    if ( DB_OK == rc && old_version < 3 && new_version >= 3 ) {
        rc = upgrade_to_schema_v3( hdb );
    }

    return DB_OK != rc ? -1 : 0;
}

int
example_main(int argc, char **argv) 
{
    /* Create a database with the current schema definition */
    db_t hdb = create_database( EXAMPLE_DATABASE, &v2_schema, init_data_cb );

    /* Create database with v1 schema */
    db_t hdb_upgrade = create_database( EXAMPLE_UPGRADE_DATABASE, &v1_schema, init_data_cb );
    
    /* Open database with v1 schema and upgrade to current schema */
    db_shutdown( hdb_upgrade, 0, NULL );
    hdb = open_database( EXAMPLE_UPGRADE_DATABASE, &v2_schema, &upgrade_schema_cb );
    
    if( hdb != NULL ) {
        int v = get_schema_version( hdb_upgrade );
        db_shutdown( hdb_upgrade, 0, NULL );
        db_shutdown( hdb, 0, NULL );
        if (v == v2_schema.version) {
            return EXIT_SUCCESS;
        }
    }
    db_shutdown( hdb_upgrade, 0, NULL );
    db_shutdown( hdb, 0, NULL );
    return EXIT_FAILURE;
}

