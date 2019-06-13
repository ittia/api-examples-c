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

/** @file storage_encryption.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "dbs_sql_line_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXAMPLE_BACKUP_DATABASE "storage_encryption-backup.ittiadb"


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

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;

    /* Hard-coded database file name and encryption key. */
    static const char * EXAMPLE_DATABASE
        = "storage_encryption.ittiadb";
    static const char encryption_key[256 / 8]
        = "32byte_256bit_length_encrypt_key";

    db_t hdb;
    db_file_storage_config_t storage_cfg;
    db_auth_info_t auth_info;

    /* Initialize configuration data structures */
    db_file_storage_config_init( &storage_cfg );
    db_auth_info_init( &auth_info );

    /* Set AES-256 ecryption parameters */
    storage_cfg.auth_info = &auth_info;
    auth_info.cipher_type = DB_CIPHER_AES256_CTR;
    auth_info.cipher_data = (void *) encryption_key;
    auth_info.cipher_data_size = sizeof( encryption_key );

    /* Create encrypted database */
    hdb = db_create_file_storage( EXAMPLE_DATABASE, &storage_cfg );

    if ( hdb == NULL ) {
        print_error_message( "unable to create database", NULL );
        goto exit;
    }

    /* Close all connections to the database to forget the encryption key */
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    /* Try to open database with the wrong encryption key */
    {
        static const char wrong_key[256 / 8] = "32byte_256bit_wrongx_encrypt_key";

        auth_info.cipher_data = (void *)wrong_key;
        auth_info.cipher_data_size = sizeof( wrong_key );

        hdb = db_open_file_storage(EXAMPLE_DATABASE, &storage_cfg);
        if ( NULL != hdb ) {
            fprintf(stderr, "AES-encrypted DB has erroneously opened with intentionally-wrong encryption key\n" );
            db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
            goto exit;
        }

        /* Using the wrong encryption key is a storage error */
        if ( get_db_error() != DB_ESTORAGE ) {
            /* Some other error has occurred */
            goto exit;
        }
    }

    /* Open the database file with the correct encryption key. */
    auth_info.cipher_data = (void *)encryption_key;
    auth_info.cipher_data_size = sizeof( encryption_key );
    hdb = db_open_file_storage( EXAMPLE_DATABASE, &storage_cfg );
    if ( NULL == hdb ) {
        print_error_message( "unable to open database", NULL );
        goto exit;
    }

    /* Use backup to make a copy of the database with a different encryption key */
    {
        static const char backup_key[256 / 8] = "32byte_256bit_backup_encrypt_key";

        db_backup_t backup_cfg;

        backup_cfg.file_mode = DB_UTF8_NAME;
        backup_cfg.backup_flags = 0;
        backup_cfg.cipher_type = DB_CIPHER_AES256_CTR;
        backup_cfg.cipher_data = (void *)backup_key;
        backup_cfg.cipher_data_size = sizeof( backup_key );

        if ( DB_OK != db_backup_ex( hdb, EXAMPLE_BACKUP_DATABASE, &backup_cfg ) ) {
            db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
            print_error_message( "unable to backup database", NULL );
            goto exit;
        }

        /* Close original database */
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );

        /* Try to open backup database */
        auth_info.cipher_type = DB_CIPHER_AES256_CTR;
        auth_info.cipher_data = (void *)backup_key;
        auth_info.cipher_data_size = sizeof( backup_key );

        hdb = db_open_file_storage( EXAMPLE_BACKUP_DATABASE, &storage_cfg );
        if ( NULL == hdb ) {
            print_error_message( "unable to open backup database", NULL );
            goto exit;
        }

        printf("Enter SQL statements or an empty line to exit\n");
        dbs_sql_line_shell(hdb, EXAMPLE_BACKUP_DATABASE, stdin, stdout, stderr);

        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL );
    }

    rc = EXIT_SUCCESS;

exit:
    db_auth_info_destroy( &auth_info );
    db_file_storage_config_destroy( &storage_cfg );

    return rc;
}
