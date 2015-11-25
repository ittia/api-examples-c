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

/** @file online_backup.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "portable_inttypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread_utils.h"

#define EXAMPLE_DATABASE "online_backup.ittiadb"
#define EXAMPLE_BACKUP_DATABASE "online_backup-copy.ittiadb"

/// Insert and commit this count of rows before spawn backup thread
#define COMMITED_RECORDS_BEFORE_BACKUP    10

#define STORAGE_TABLE "table"
#define MAX_STRING_FIELD 10
#define PKEY_INDEX_NAME "pkey"
static db_fielddef_t storage_fields[] =
{
    { 0, "ansi_field",      DB_COLTYPE_ANSISTR,     MAX_STRING_FIELD, 0, DB_NULLABLE, 0 },
    { 1, "int64_field",     DB_COLTYPE_SINT64,      0,                0, DB_NOT_NULL, 0 },
    { 2, "float64_field",   DB_COLTYPE_FLOAT64,     0,                0, DB_NULLABLE, 0 },
    { 3, "utf8_field",      DB_COLTYPE_UTF8STR,     2*MAX_STRING_FIELD, 0, DB_NOT_NULL, 0 },
};

// v1 PKey fields
static db_indexfield_t pkey_fields[] =
{
    { 1 }
};

db_indexdef_t indexes[] =
{
    { DB_ALLOC_INITIALIZER(),       /* db_alloc */
      DB_INDEXTYPE_DEFAULT,         /* index_type */
      PKEY_INDEX_NAME,              /* index_name */
      DB_PRIMARY_INDEX,             /* index_mode */
      DB_ARRAY_DIM(pkey_fields),    /* nfields */
      pkey_fields },                /* fields */
};

/* Database schemas. */
static db_tabledef_t tables[] =
{
    {
        DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        STORAGE_TABLE,
        DB_ARRAY_DIM(storage_fields),
        storage_fields,
        DB_ARRAY_DIM(indexes),   // Indexes array size
        indexes,                 // Indexes array
        0, NULL,
    },
};

dbs_schema_def_t db_schema =
{
    DB_ARRAY_DIM(tables),
    tables
};

//------- Declarations to use while populating table

/* Populate table using relative bounds fields. So declare appropriate structure. See
   ittiadb/manuals/users-guide/api-c-database-access.html#relative-bound-fields
 */
typedef struct {
    db_ansi_t   f0[MAX_STRING_FIELD + 1];   /* Extra char for null termination. */
    uint64_t    f1;
    double      f2;
    char        f3[MAX_STRING_FIELD*2 + 1];
} storage_t;

static const db_bind_t binds_def[] = {
    { 0, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( storage_t, f0 ),  DB_BIND_SIZE( storage_t, f0 ), -1, DB_BIND_RELATIVE },
    { 1, DB_VARTYPE_SINT64,   DB_BIND_OFFSET( storage_t, f1 ),  DB_BIND_SIZE( storage_t, f1 ), -1, DB_BIND_RELATIVE },
    { 2, DB_VARTYPE_FLOAT64,  DB_BIND_OFFSET( storage_t, f2 ),  DB_BIND_SIZE( storage_t, f2 ), -1, DB_BIND_RELATIVE },
    { 3, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, f3 ),  DB_BIND_SIZE( storage_t, f3 ), -1, DB_BIND_RELATIVE },
};

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
create_database(char* database_name, dbs_schema_def_t *schema)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

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

/// Arguments to backup procedure
typedef struct {
    char dbname[ FILENAME_MAX + 1 ];    ///< Backup source db name
    char dbbname[ FILENAME_MAX + 1 ];   ///< Backup result database
    int is_complete;                    ///< Backup thread completion flag
    int rc;                             ///< Result of backup execution. 0 - if success.
} backup_args_t;

static mutex_t mutex;

/// Backup thread perform this:
void backup_proc( backup_args_t * data )
{
    db_backup_t backup_cfg;
    db_t hdb;

    // Just open DB to backup ...
    hdb = db_open_file_storage(data->dbname, NULL);
    data->rc = EXIT_FAILURE;

    if(hdb) {
        // Fill parameters
        backup_cfg.file_mode = DB_UTF8_NAME;
        backup_cfg.backup_flags = 0;
        backup_cfg.cipher_type = DB_CIPHER_NONE;

        // Launch backup execution
        if( DB_OK != db_backup_ex( hdb, data->dbbname, &backup_cfg ) ) {
            print_error_message( "Couldn't backup DB", NULL );
        }
        else {
            data->rc = EXIT_SUCCESS;
        }
    } else {
        print_error_message( "Couldn't open DB", NULL );
    }
    // Report to parent thread the backup complete
    mutex_lock( &mutex );
    data->is_complete = 1;
    mutex_unlock( &mutex );
}

/// Check backed up DB. - Try to open & check table
int check_backed_up_data(char *dbname)
{
    int rc = EXIT_FAILURE;
    db_result_t db_rc = DB_OK;

    db_t hdb;                   // db to create and insert rows
    db_row_t row;

    // Use absolute bound fields schema
    int64_t f1_data;    //< Fetch f1 field here
    const db_bind_t row_def =
    {
        1,                                /* field_no   */
        DB_VARTYPE_SINT64,                /* data_type  */
        DB_BIND_ADDRESS(&f1_data),        /* data_ptr   */
        sizeof(f1_data),                  /* data_size  */
        DB_BIND_ADDRESS(NULL),            /* data_ind   */
        DB_BIND_ABSOLUTE,                 /* data_flags */
    };

    db_cursor_t c;  // Cursor to scan table to check
    // Cursor parameters
    db_table_cursor_t p = {
        PKEY_INDEX_NAME,   //< Index by which to sort the table
        DB_SCAN_FORWARD | DB_LOCK_DEFAULT
    };

    // open backup and check that at least our 10 records (commited before backup started) is in table
    hdb = db_open_file_storage(dbname, NULL);
    if(!hdb) {
        print_error_message( "Can't open backup database\n", NULL );
        return rc;
    }
    // Open cursor
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);
    row = db_alloc_row( &row_def, 1 );
    db_rc = db_seek_first(c);

    // Scan table and check that pkey values are monotonically increase
    {
        int counter = 0;
        for(; !db_eof(c) && DB_OK == db_rc; db_rc = db_seek_next(c) )
        {
            db_rc = db_fetch(c, row, NULL);
            if( f1_data != ++counter ) {
                break;
            }
        }
        if( DB_OK != db_rc ) {
            print_error_message( "Error to scan table of backup db\n", c );
        }
        else if( f1_data != counter ) {
            fprintf( stderr, "Pkey field values sequence violation detected in backup db: (%" PRId64 ", %d)\n",
                     f1_data, counter
                     );
        }
        else if( COMMITED_RECORDS_BEFORE_BACKUP > counter ) {
            fprintf( stderr, "Not all records which should be in backup are really there. At least %d expected. But only %d is in.\n",
                     COMMITED_RECORDS_BEFORE_BACKUP, counter );
        }
        else {
            fprintf( stdout, "%d records backed up\n", counter );
            rc = EXIT_SUCCESS;
        }
    }

    db_close_cursor( c );
    db_free_row( row );
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    backup_args_t backup_args = { 0, 0, 0, 0 };
    db_t hdb;                   // db to create and insert rows

    static storage_t row2ins = { "ansi_str",  0,  1.231, "utf8" };
    db_row_t row;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;
    os_thread_t *tid;           // Backup thread
    db_result_t db_rc;
    int is_enough;

    strncpy( backup_args.dbname, EXAMPLE_DATABASE, FILENAME_MAX );
    backup_args.dbname[FILENAME_MAX] = '\0';
    strncpy( backup_args.dbbname, EXAMPLE_BACKUP_DATABASE, FILENAME_MAX );
    backup_args.dbbname[FILENAME_MAX] = '\0';

    if( mutex_init( &mutex ) ) {
        fprintf( stderr, "Couldn't create mutex\n" );
        goto exit;
    }

    // Create database
    hdb = create_database( backup_args.dbname, &db_schema );

    if (!hdb) {
        goto exit;
    }

    // Allocate row
    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't allocate db_row_t\n", NULL );
        goto exit;
    }

    // Open cursor
    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    // Insertion loop. Insert COMMITED_RECORDS_BEFORE_BACKUP records and start backup thread
    do {
        ++row2ins.f1;   // Increment pkey value to put it in 'table.int64_field' field
        db_rc = db_insert(c, row, &row2ins, 0);
        if( COMMITED_RECORDS_BEFORE_BACKUP <= row2ins.f1 ) {
            // If backup is [about to be] started, do insertions in explicit transactions
            db_rc = db_commit_tx( hdb, 0 );
            db_rc = db_begin_tx( hdb, 0 );
        }
        if( COMMITED_RECORDS_BEFORE_BACKUP == row2ins.f1 ) {
            // Start backup thread
            rc = thread_spawn( (thread_proc_t)backup_proc, &backup_args, THREAD_JOINABLE, &tid );
            if( rc ) {
                fprintf(stderr, "Coulnt' start backup thread" );
                goto exit;
            }
        }
        // Check if backup complete
        mutex_lock( &mutex );
        is_enough = backup_args.is_complete;
        mutex_unlock( &mutex );
    } while (DB_OK == db_rc && !is_enough);

    fprintf( stdout, "%" PRId64 " records inserted in source db\n", ++row2ins.f1 );   // Increment pkey value to put it in 'table.int64_field' field

    if( DB_OK != db_rc ) {
        print_error_message( "Couldn't insert db row\n", c );
    }
    else {
        db_rc = DB_OK == db_rc ? db_commit_tx( hdb, 0 ) : db_rc;
        if( DB_OK != db_rc ) {
            print_error_message( "Couldn't do final commit\n", c );
        }
    }
    db_close_cursor( c );
    db_free_row( row );
    // close source db
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    // Clean backup thread
    thread_join( tid );
    mutex_destroy( &mutex );

    rc = backup_args.rc || ( db_rc != DB_OK );
    rc = rc ? rc : check_backed_up_data( backup_args.dbbname );

exit:
    return rc;
}
