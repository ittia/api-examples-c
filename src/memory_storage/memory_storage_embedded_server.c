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

/** @file memory_storage_embedded_server.c
 *
 * Start an embedded database server to host a memory storage database using
 * shared memory.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


#define SHM_NAME "example"
#define DATABASE_NAME "example.ittiadb"
//#define DATABASE_PASSWORD "password"
#define DATABASE_PASSWORD ""

#define DATABASE_FILE_NAME "memory_storage_embedded_server.ittiadb"

#define DATABASE_PEER_NAME "file"
#define DATABASE_PEER_ADDRESS 1

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
static db_indexdef_t table_t_indexes[] = {
    {
        DB_ALLOC_INITIALIZER(),
        DB_INDEXTYPE_DEFAULT,
        "ID",
        DB_PRIMARY_INDEX,
        DB_ARRAY_DIM(index_t_id_fields),
        index_t_id_fields,
    }
};

static db_tabledef_t db_tables[] = {
    {
        DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        "T1",
        DB_ARRAY_DIM(table_t_fields), table_t_fields,
        DB_ARRAY_DIM(table_t_indexes), table_t_indexes,
        0, NULL,
    },
    {
        DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        "T2",
        DB_ARRAY_DIM(table_t_fields), table_t_fields,
        DB_ARRAY_DIM(table_t_indexes), table_t_indexes,
        0, NULL,
    }
};

dbs_schema_def_t db_schema =
{
    DB_ARRAY_DIM(db_tables),
    db_tables,
    0, NULL,
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

int
init_database()
{
    db_t hdb;
    db_rep_peerdef_t peerdef;

    hdb = db_open_memory_storage( DATABASE_NAME, NULL );
    if ( NULL == hdb ) {
        print_error_message( NULL, "Couldn't open local connection to memory storage" );
        return EXIT_FAILURE;
    }

    /* Create the database schema. */
    if (dbs_create_schema(hdb, &db_schema) < 0) {
        print_error_message( NULL, "Couldn't initialize database objects" );
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
        return EXIT_FAILURE;
    }

    /* Set an access password to restrict access from other processes. */
    if ( DB_OK != db_set_access_password(hdb, DATABASE_PASSWORD, 0) )   {
        print_error_message( NULL, "Couldn't set access password" );
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
        return EXIT_FAILURE;
    }

    /* Register a file storage database as a replication peer to save and load tables into. */
    db_rep_peerdef_init(&peerdef);
    peerdef.peer_address = DATABASE_PEER_ADDRESS;
    strncpy(peerdef.peer_uri, DATABASE_FILE_NAME, (sizeof peerdef.peer_uri) - 1);
    db_rep_create_peer(hdb, DATABASE_PEER_NAME, &peerdef);
    db_rep_peerdef_destroy(&peerdef);

    db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );

    return EXIT_SUCCESS;
}

int
load_database()
{
    db_t hdb;
    db_cursor_t cursor;
    db_rep_snapshot_info_t snapshot_info;
    char table_name[DB_MAX_TABLE_NAME + 1] = "";
    int32_t table_level = 1;
    db_bind_t binds[] = {
        DB_BIND_STR(1, DB_VARTYPE_ANSISTR, table_name),
        DB_BIND_VAR(3, DB_VARTYPE_SINT32, table_level),
    };

    hdb = db_open_memory_storage( DATABASE_NAME, NULL );
    if ( NULL == hdb ) {
        print_error_message( NULL, "Couldn't open local connection to memory storage" );
        return EXIT_FAILURE;
    }

    /* IN mode copies from the peer database into memory storage. */
    snapshot_info.flags = DB_REP_MODE_IN;
    snapshot_info.auth_info = NULL;

    cursor = db_open_table_cursor(hdb, "TABLES", NULL);
    for (db_seek_first(cursor); !db_eof(cursor); db_seek_next(cursor)) {
        db_qfetch(cursor, binds, DB_ARRAY_DIM(binds), NULL);
        db_commit_tx(hdb, 0);

        /* Skip catalog tables. */
        if (table_level == 1) {
            continue;
        }

        fprintf( stdout, "Loading table %s\n", table_name );
        if (DB_OK != db_rep_snapshot_ex(hdb, table_name, DATABASE_PEER_NAME, &snapshot_info)) {
            print_error_message( NULL, "Couldn't load table %s", table_name );
            db_close_cursor(cursor);
            db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
            return EXIT_FAILURE;
        }
    }
    db_commit_tx(hdb, 0);

    db_close_cursor(cursor);
    db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );

    return EXIT_SUCCESS;
}

int
save_database()
{
    db_t hdb;
    db_cursor_t cursor;
    db_rep_snapshot_info_t snapshot_info;
    char table_name[DB_MAX_TABLE_NAME + 1] = "";
    int32_t table_level = 1;
    db_bind_t binds[] = {
        DB_BIND_STR(1, DB_VARTYPE_ANSISTR, table_name),
        DB_BIND_VAR(3, DB_VARTYPE_SINT32, table_level),
    };

    hdb = db_open_memory_storage( DATABASE_NAME, NULL );
    if ( NULL == hdb ) {
        print_error_message( NULL, "Couldn't open local connection to memory storage" );
        return EXIT_FAILURE;
    }

    /* OUT mode copies from memory storage into the peer database. */
    snapshot_info.flags = DB_REP_MODE_OUT;
    snapshot_info.auth_info = NULL;

    cursor = db_open_table_cursor(hdb, "TABLES", NULL);
    for (db_seek_first(cursor); !db_eof(cursor); db_seek_next(cursor)) {
        db_qfetch(cursor, binds, DB_ARRAY_DIM(binds), NULL);
        db_commit_tx(hdb, 0);

        /* Skip catalog tables. */
        if (table_level == 1) {
            continue;
        }

        fprintf( stdout, "Saving table %s\n", table_name );
        if (DB_OK != db_rep_snapshot_ex(hdb, table_name, DATABASE_PEER_NAME, &snapshot_info)) {
            print_error_message( NULL, "Couldn't save table %s", table_name );
        }
    }

    db_close_cursor(cursor);
    db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );

    return EXIT_SUCCESS;
}

int
init_file_storage()
{
    db_t hdb;
    db_cursor_t query;
    db_rep_config_t rep_config;

    hdb = db_open_file_storage( DATABASE_FILE_NAME, NULL );
    if ( NULL == hdb && get_db_error() != DB_ENOENT ) {
        print_error_message( NULL, "File storage exists, but cannot be opened" );
        return EXIT_FAILURE;
    }

    /* If the file already exists, no initialization is required. */
    if ( NULL != hdb ) {
        int is_valid = dbs_check_schema(hdb, &db_schema);
        if (!is_valid) {
            fprintf( stdout, "File exists, but schema is out-of-date\n" );
        }
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
        return is_valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    fprintf( stdout, "Creating database file %s\n", DATABASE_FILE_NAME );

    hdb = db_create_file_storage( DATABASE_FILE_NAME, NULL );
    if ( NULL == hdb ) {
        print_error_message( NULL, "Couldn't create file storage" );
        return EXIT_FAILURE;
    }

    /* Use a schema that is identical to the memory storage. */
    if (dbs_create_schema(hdb, &db_schema) < 0) {
        print_error_message( NULL, "Couldn't initialize database objects" );
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
        return EXIT_FAILURE;
    }

    /* Set the replication address of the file storage to enable replication. */
    db_rep_config_init(&rep_config);
    rep_config.rep_address = DATABASE_PEER_ADDRESS;
    db_rep_set_config(hdb, &rep_config);
    db_rep_config_destroy(&rep_config);

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return EXIT_SUCCESS;
}

void
event_loop(void)
{
    fprintf( stdout, "\nPress Enter to stop the server.\n");
    fscanf( stdin, "%*c" );

    /* Save the database (this can be called periodically on a timer or in response to an event.) */
    save_database();
}

int
example_main( int argc, char **argv )
{
    int rc = EXIT_FAILURE;
    db_t hdb = NULL;
    db_storage_config_t config;
    db_server_config_t srv_config;

    setbuf( stdout, NULL );
    setbuf( stderr, NULL );

    db_storage_config_init(&config);
    config.storage_mode = DB_MEMORY_STORAGE;
    db_memory_storage_config_init( &config.u.memory_storage );

    config.u.memory_storage.memory_storage_size = 1024 * 1024;

    /* Allocate a new empty memory storage database to share with other processes. */
    if ( DB_OK != db_mount_storage( DATABASE_NAME, NULL, &config ) ) {
        print_error_message( NULL, "Couldn't create memory storage" );
        return EXIT_FAILURE;
    }

    /* Initialize the database schema. */
    rc = init_database();

    /* Create the file storage, if it does not already exist. */
    if (rc == EXIT_SUCCESS) {
        rc = init_file_storage();
    }

    /* Load records from the file storage. */
    if (rc == EXIT_SUCCESS) {
        rc = load_database();
    }

    if (rc != EXIT_SUCCESS) {
        db_unmount_storage( DATABASE_NAME );
        return rc;
    }

    db_server_config_init( &srv_config );
    /* Disable TCP/IP server. */
    srv_config.bind_addr = 0xFFFFFFFFU;
    /* Enable shared memory server. */
    srv_config.shm_flags = DB_SERVER_SHM_ENABLE;
    srv_config.shm_name = SHM_NAME;

    /* Start the ITTIA DB Server in this process to share mounted databases. */
    if ( DB_OK != db_server_start( &srv_config ) ) {
        print_error_message( NULL, "Couldn't start shared memory server" );
        db_unmount_storage( DATABASE_NAME );
        return EXIT_FAILURE;
    }

    fprintf( stdout, "ITTIA DB Server is running in background threads.\n" );

    fprintf( stdout, "Memory storage mounted at:\n\n");
    fprintf( stdout, "idb+shm://%s/%s\n", SHM_NAME, DATABASE_NAME );

    fprintf( stdout, "\nODBC client connection string:\n\n" );
    fprintf( stdout, "Driver={ITTIA DB};Protocol=idb+shm;Server=%s;Database=%s;PWD=%s\n", SHM_NAME, DATABASE_NAME, DATABASE_PASSWORD );

    /* Run the main event loop for this application. */
    event_loop();

    /* Stop accepting new connections and wait for clients to disconnect. */
    db_server_stop( 0 );
    fprintf( stdout, "Server stopped.\n" );

    /* Make durable the final state of the memory storage. */
    rc = save_database();
    if (rc != EXIT_SUCCESS) {
        db_unmount_storage( DATABASE_NAME );
        return rc;
    }

    /* Destroy the memory storage database and free its memory. */
    db_unmount_storage( DATABASE_NAME );

    return EXIT_SUCCESS;
}
