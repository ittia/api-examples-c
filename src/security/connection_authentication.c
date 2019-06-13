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

/** @file connection_authentication.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STORAGE_TABLE "table"
#define MAX_STRING_FIELD 10

static db_fielddef_t storage_fields[] =
{
    { 0, "ansi_field",      DB_COLTYPE_ANSISTR,     MAX_STRING_FIELD, 0, DB_NULLABLE, 0 },
    { 1, "int64_field",     DB_COLTYPE_SINT64,      0,                0, DB_NULLABLE, 0 },
    { 2, "float64_field",   DB_COLTYPE_FLOAT64,     0,                0, DB_NULLABLE, 0 },
    { 3, "utf8_field",      DB_COLTYPE_UTF8STR,     2*MAX_STRING_FIELD, 0, DB_NOT_NULL, 0 },
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
        0,  // Indexes array size
        0, // Indexes array
        0, NULL,
    },
};

dbs_schema_def_t db_schema =
{
    DB_ARRAY_DIM(tables),
    tables
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
create_database(const char* database_name, dbs_schema_def_t *schema, db_storage_config_t * storage_cfg)
{
    db_t hdb;

    fprintf( stdout, "enter create_database\n" );

    if( !storage_cfg || DB_FILE_STORAGE == storage_cfg->storage_mode ) {
        /* Create a new file storage database with default parameters. */
        db_file_storage_config_t *file_storage_cfg = storage_cfg ? &storage_cfg->u.file_storage : 0;
        hdb = db_create_file_storage(database_name, file_storage_cfg);
    } else {
        /* Create a new memory storage database with default parameters. */
        db_memory_storage_config_t *mem_storage_cfg = storage_cfg ? &storage_cfg->u.memory_storage : 0;
        hdb = db_create_memory_storage(database_name, mem_storage_cfg);
    }

    if (hdb == NULL) {
        print_error_message( NULL, "Couldn't create DB" );
        return NULL;
    }

    if (dbs_create_schema(hdb, schema) < 0) {
        print_error_message( NULL, "Couldn't initialize DB objects" );

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
//        remove(database_name);
        return NULL;
    }

    fprintf( stdout, "leave create_database\n" );
    return hdb;
}

static void
fill_example_storage_config(
    int32_t mode,
    db_storage_config_t *storage_config,
    db_auth_info_t * auth_info
    )
{
    db_storage_config_init(storage_config);
    storage_config->storage_mode = mode;

    if( DB_FILE_STORAGE == mode ) {
        db_file_storage_config_init(&storage_config->u.file_storage);
        if( auth_info ) {
            storage_config->u.file_storage.auth_info = auth_info;
        }
    }
    else {
        db_memory_storage_config_init(&storage_config->u.memory_storage);
        storage_config->u.memory_storage.memory_page_size = DB_DEF_PAGE_SIZE * 4 /*DB_MIN_PAGE_SIZE*/;
        storage_config->u.memory_storage.memory_storage_size = storage_config->u.memory_storage.memory_page_size * 4;
        if( auth_info ) {
            storage_config->u.memory_storage.auth_info = auth_info;
        }
    }
}

static void
destroy_example_storage_config(
    int32_t mode,
    db_storage_config_t *storage_config
    )
{
    if( DB_FILE_STORAGE == mode ) {
        db_file_storage_config_destroy(&storage_config->u.file_storage);
    }
    else {
        db_memory_storage_config_destroy(&storage_config->u.memory_storage);
    }

    db_storage_config_destroy(storage_config);
}

/*
 *  Mount storage
 */
static int
mount_db(
    int32_t mode,
    const db_fname_t * dbname,
    const db_fname_t * alias,
    const char * encryption_key,    ///< Encryption key ( NULL, if none )
    size_t enc_key_len              ///< Encryption key len ( 0, if none )
    )
{
    int rc = EXIT_FAILURE;
    db_storage_config_t storage_config;
    db_auth_info_t auth_info;               // db_auth_info_t keeps AES parameters
    if( encryption_key ) {
        auth_info.cipher_type = DB_CIPHER_AES256_CTR;       //< Cipher length
        auth_info.cipher_data = (const void *)encryption_key;    //< Encryption key
        auth_info.cipher_data_size = (uint32_t)enc_key_len;           //< Encryption key length (in bytes)
    }

    fill_example_storage_config( mode, &storage_config, &auth_info );

    if( DB_OK == db_mount_storage( dbname, alias, &storage_config ) ) {
        rc = EXIT_SUCCESS;
    }
    else {
        print_error_message( NULL, "Couldn't mount DB: [%s] with alias: [%s]", dbname, alias ? alias : "<null>" );
    }

    destroy_example_storage_config( mode, &storage_config );

    return rc;
}

/*
 *  Open connection and call do_describe_table() to check table 'table' existance and its definition
 */
static int
check_conn_to_mounted_db( int32_t mode, const db_fname_t * dbname )
{
    int rc = EXIT_FAILURE;
    db_tabledef_t tdef = { DB_ALLOC_INITIALIZER() };
    db_t hdb;
    db_storage_config_t storage_config;
    db_auth_info_t auth_info = {
        DB_CIPHER_NONE,     //< No encryption key needed to set conn to mounted DB
        NULL, 0, NULL,
        "some_password"     //< All we need here is access password
    };

    fill_example_storage_config( mode, &storage_config, &auth_info );
    if( DB_FILE_STORAGE == mode ) {
        hdb = db_open_file_storage( dbname, &storage_config.u.file_storage );
    }
    else {
        hdb = db_open_memory_storage( dbname, &storage_config.u.memory_storage );
    }
    destroy_example_storage_config( mode, &storage_config );

    if( hdb ) {
        if( DB_OK == db_describe_table( hdb, STORAGE_TABLE, &tdef, DB_DESCRIBE_TABLE_FIELDS ) ) {
            rc = tdef.nfields == DB_ARRAY_DIM( storage_fields ) ? EXIT_SUCCESS : EXIT_FAILURE;
            if( EXIT_SUCCESS != rc ) {
                print_error_message( NULL, "check_conn: table [%s]'s description wrong in DB: [%s]", STORAGE_TABLE, dbname );
            }
        }
        else {
            print_error_message( NULL, "check_conn: Couldn't get table [%s]'s description from DB: [%s]", STORAGE_TABLE, dbname );
        }

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    }
    else {
        print_error_message( NULL, "check_conn: Couldn't open DB: %s", dbname );
    }

    return rc;
}

#define DB_FILENAME     "connection_authentication.ittiadb"
#define DB_FILEALIAS    "file_alias.ittiadb"
#define DB_MEMNAME      "inmem.ittiadb"
#define DB_MEMALIAS     "mem_alias.ittiadb"

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    db_t file_hdb = NULL;      // File-storage db
    db_t memory_hdb = NULL;    // In-mem-storage db

    char encryption_key[ 256/8 ];  // 256bit encryption key to encrypt DB's data

    db_storage_config_t storage_config;
    db_auth_info_t auth_info;               // db_auth_info_t keeps AES parameters

    db_server_config_t srv_config;

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    strncpy( encryption_key, "32byte_256bit_length_encrypt_key", DB_ARRAY_DIM( encryption_key ) );

    /*
     *  Start DbServer
     */
    db_server_config_init( &srv_config );

    /*
     *  Create an encrypted database file and set a password to protect it.
     */
    // Set AES ecryption params
    auth_info.cipher_type = DB_CIPHER_AES256_CTR;       //< Cipher length
    auth_info.cipher_data = (void *) encryption_key;    //< Encryption key
    auth_info.cipher_data_size = DB_ARRAY_DIM( encryption_key );    //< Encryption key length (in bytes)

    fill_example_storage_config(DB_FILE_STORAGE, &storage_config, &auth_info);
    file_hdb = create_database( DB_FILENAME, &db_schema, &storage_config );
    destroy_example_storage_config( DB_FILE_STORAGE, &storage_config );

    if( file_hdb && DB_OK == db_set_access_password( file_hdb, "some_password", 0 ) ) {
        /*
         *  Mount the file storage database, using the encryption key.
         */
        db_shutdown(file_hdb, DB_SOFT_SHUTDOWN, NULL);  // A storage cannot be mounted while open.
        if( EXIT_SUCCESS ==
            mount_db( DB_FILE_STORAGE, DB_FILENAME, DB_FILEALIAS, encryption_key, DB_ARRAY_DIM( encryption_key ) )
            )
        {
            /*
             *  Mount the memory storage. No encryption.
             */
            if( 0 == mount_db( DB_MEMORY_STORAGE, DB_MEMNAME, DB_MEMALIAS, NULL, 0 ) ) {
                /*
                 *  Then open mounted memory storage, create db schema inside it and set a password to protect it.
                 */
                fill_example_storage_config(DB_MEMORY_STORAGE, &storage_config, NULL);
                memory_hdb = db_open_memory_storage( DB_MEMNAME, &storage_config.u.memory_storage );
                destroy_example_storage_config( DB_FILE_STORAGE, &storage_config );

                if( memory_hdb ) {
                    // Now create DB schema in mounted in-mem storage, and set access-password
                    if( 0 <= dbs_create_schema(memory_hdb, &db_schema)
                        &&  DB_OK == db_set_access_password( memory_hdb, "some_password", 0 )
                        )
                    {
                        if( DB_OK == db_server_start( &srv_config ) ) {

                            // Access mounted DB using local connections
                            rc = check_conn_to_mounted_db( DB_MEMORY_STORAGE, DB_MEMNAME );
                            rc = rc || check_conn_to_mounted_db( DB_FILE_STORAGE, DB_FILENAME );
                            // Access mounted DB using remote connections with real storages names
                            rc = rc || check_conn_to_mounted_db( DB_FILE_STORAGE, "idb+shm:///"DB_FILENAME );
                            rc = rc || check_conn_to_mounted_db( DB_MEMORY_STORAGE, "idb+shm:///"DB_MEMNAME );
                            // Access mounted DB using remote connections with alias names
                            rc = rc || check_conn_to_mounted_db( DB_MEMORY_STORAGE, "idb+shm:///"DB_MEMALIAS );
                            rc = rc || check_conn_to_mounted_db( DB_FILE_STORAGE, "idb+shm:///"DB_FILEALIAS );

                            db_server_stop( 0 );
                        }
                        else {
                            print_error_message( NULL, "Couldn't start DB server" );
                        }
                    }
                }
                else {
                    print_error_message( NULL, "Couldn't initialize DB objects on in-mem storage" );
                }
                db_unmount_storage( DB_MEMNAME );
            }
            else {
                print_error_message( NULL, "Couldn't create in-mem storage" );
            }
            db_unmount_storage( DB_FILENAME );
        }
        else {
            print_error_message( NULL, "Couldn't mount file storage" );
        }
    }
    else {
        print_error_message( NULL, "Couldn't create file storage or set access password to it" );
    }

    if( file_hdb ) {
        db_shutdown(file_hdb, DB_SOFT_SHUTDOWN, NULL);
    }
    if( memory_hdb ) {
        db_shutdown(memory_hdb, DB_SOFT_SHUTDOWN, NULL);
    }

    db_server_config_destroy( &srv_config );

    return rc;
}
