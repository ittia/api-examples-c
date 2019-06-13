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

/** @file data_change_notification.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "thread_utils.h"

#define DB_FILENAME "data_change_notification.ittiadb"
#define ROW_COUNT 10    //< count of rows to insert, update and delete

/* Database schema: table, field, and index definitions. */

#define T_ID 0
#define T_N 1
#define T_S 2

#define MAX_S_LEN 100

static db_fielddef_t table_t_fields[] = {
    { T_ID, "ID", DB_COLTYPE_UINT32,    0, 0, DB_NOT_NULL, NULL, 0 },
    { T_N,  "N",  DB_COLTYPE_SINT32,    0, 0, DB_NULLABLE, NULL, 0 },
    { T_S,  "S",  DB_COLTYPE_UTF8STR,   MAX_S_LEN, 0, DB_NULLABLE, NULL, 0 }
};

static db_indexfield_t index_t_id_fields[] = {
    { T_ID, 0 }
};
static db_indexdef_t index_t_id = {
    DB_ALLOC_INITIALIZER(),
    DB_INDEXTYPE_DEFAULT,
    "ID",
    DB_PRIMARY_INDEX,
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

dbs_schema_def_t db_schema =
{
    1,
    &table_t
};

typedef struct {
    uint32_t id;
    int32_t n;
    char s[ MAX_S_LEN + 1];
    db_len_t s_isnull;
} storage_t;

static const db_bind_t t_binds[] = {
    { T_ID, DB_VARTYPE_UINT32,  DB_BIND_OFFSET( storage_t, id ),  DB_BIND_SIZE( storage_t, id ), -1, DB_BIND_RELATIVE },
    { T_N,  DB_VARTYPE_SINT32,  DB_BIND_OFFSET( storage_t, n ),   DB_BIND_SIZE( storage_t, n ),  -1, DB_BIND_RELATIVE },
    { T_S,  DB_VARTYPE_UTF8STR, DB_BIND_OFFSET( storage_t, s ),   DB_BIND_SIZE( storage_t, s ),  DB_BIND_OFFSET( storage_t, s_isnull ), DB_BIND_RELATIVE },
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

char changes_complete = 0;

/// Arguments to changes procedure
typedef struct {
    int rc;                             ///< Result code
} change_args_t;

static mutex_t mutex;

/// Backup thread perform this:
static void
changes_proc( change_args_t * data )
{
    db_t hdb;
    db_cursor_t t_cursor;
    db_row_t t_row;
    int i;
    storage_t row_data = { 0, 0, "", DB_NTS };
    db_result_t dbrc = DB_OK;

    hdb = db_open_file_storage(DB_FILENAME, NULL);

    if(hdb) {
        t_row = db_alloc_row(t_binds, DB_ARRAY_DIM(t_binds));

        /* Configure transactions. */
        dbrc = db_set_tx_default(hdb, DB_GROUP_COMPLETION | DB_READ_COMMITTED);

        /* Open unordered table cursor. */
        t_cursor = db_open_table_cursor(hdb, table_t.table_name, NULL);

        /* Insert rows. */
        for(i = 1; i <= ROW_COUNT && DB_OK == dbrc; i++) {
            db_begin_tx(hdb, 0);

            row_data.id = i;
            row_data.n = ROW_COUNT / 2 - i;
            row_data.s_isnull = sprintf(row_data.s, "%d", i);
            dbrc = db_insert(t_cursor, t_row, &row_data, 0);
            dbrc = DB_OK == dbrc ? db_commit_tx(hdb, 0) : dbrc;
        }
        // Update all rows
        db_seek_first(t_cursor);
        while( !db_eof(t_cursor) && DB_OK == dbrc ) {
            db_begin_tx(hdb, 0);

            db_fetch(t_cursor, t_row, &row_data);
            row_data.n = ROW_COUNT / 2 - i;
            dbrc = db_update(t_cursor, t_row, &row_data);

            dbrc = DB_OK == dbrc ? db_commit_tx(hdb, 0) : dbrc;
            db_seek_next(t_cursor);

            ++i;
        }
        // Delete all rows
        db_seek_first(t_cursor);
        while( !db_eof(t_cursor) && DB_OK == dbrc ) {
            db_begin_tx(hdb, 0);
            dbrc = db_delete(t_cursor, DB_DELETE_SEEK_NEXT);
            dbrc = DB_OK == dbrc ? db_commit_tx(hdb, 0) : dbrc;
        }
        db_close_cursor(t_cursor);
        db_free_row(t_row);
        db_shutdown( hdb, DB_SOFT_SHUTDOWN, NULL);
    }

    data->rc = DB_OK == dbrc ? EXIT_SUCCESS : EXIT_FAILURE;
    if( DB_OK != dbrc ) {
        print_error_message( NULL, "Data changing error" );
    }

    // Report to parent thread the backup complete
    mutex_lock( &mutex );
    changes_complete = 1;
    mutex_unlock( &mutex );
}

static void
get_row_data( db_row_t row, storage_t * data)
{
    if (DB_LEN_FAIL == db_get_field_data( row, T_ID, DB_VARTYPE_UINT32, &data->id, sizeof( uint32_t ) ) ) {
        data->id = 0;
    }
    if (DB_LEN_FAIL == db_get_field_data( row, T_N, DB_VARTYPE_SINT32, &data->n,   sizeof( int32_t ) ) ) {
        data->n = -1;
    }
    if (DB_LEN_FAIL == db_get_field_data( row, T_S, DB_VARTYPE_UTF8STR, &data->s,  MAX_S_LEN ) ) {
        data->s[0] = '\0';
    }
}

static int
watch_notifications()
{
    db_t hdb;
    db_result_t rc;
    db_row_t wrow, waux;
    db_event_t evt;
    time_t start_time;
    int complete = 0;

    storage_t r0;   //< orig_row
    storage_t r1;   //< changed_row

    int ins_cnt = 0;
    int upd_cnt = 0;
    int del_cnt = 0;
    enum {
        WATCH1
    };

    hdb = db_open_file_storage( DB_FILENAME, NULL );

    /* Allocate empty rows, which db_wait_ex will reallocate with fields */
    /* appropriate to each event. */
    wrow = db_alloc_row( NULL, 1 );
    waux = db_alloc_row( NULL, 1 );


    /* Watch the contact table for inserts and updates. Include a copy of modified */
    /* fields in each event, including former values in case of an update. */
    rc = db_watch_table(hdb, "T",
                        DB_WATCH_ROW_INSERT | DB_WATCH_ROW_UPDATE | DB_WATCH_ROW_DELETE
                        | DB_WATCH_ALL_FIELDS
                        | DB_WATCH_FORMER_VALUES,
                        WATCH1);

    start_time = time( 0 );

    do {
        rc = db_wait_ex(hdb, WAIT_MILLISEC(100), &evt, wrow, waux);

        if (rc == DB_FAIL) {
            if (get_db_error() == DB_ELOCKED) {
                clear_db_error();
                printf("Event queue is empty.\n");
                mutex_lock( &mutex );
                complete = changes_complete;
                mutex_unlock( &mutex );
            }
            else {
                print_error_message( NULL, "Error waiting for event.");
            }
        }
        else if (evt.event_tag == DB_WATCH_ROW_UPDATE && evt.u.row_update.utid == WATCH1) {
            get_row_data( wrow, &r1 );
            get_row_data( waux, &r0 );
            fprintf( stdout, "Upd from: (%d, %d, %s) to (%d, %d, %s)\n",
                     r0.id, r0.n, r0.s, r1.id, r1.n, r1.s
                     );
            upd_cnt++;
        } else if (evt.event_tag == DB_WATCH_ROW_INSERT && evt.u.row_insert.utid == WATCH1) {
            get_row_data( wrow, &r1 );
            fprintf( stdout, "Ins: (%d, %d, %s)\n",
                     r1.id, r1.n, r1.s
                     );
            ins_cnt++;
        }
        else if (evt.event_tag == DB_WATCH_ROW_DELETE && evt.u.row_delete.utid == WATCH1) {
            get_row_data( wrow, &r0 );
            fprintf( stdout, "Del: (%d, %d, %s)\n",
                     r0.id, r0.n, r0.s
                     );
            del_cnt++;
        }

        if( 5 < time( 0 ) - start_time ) {
            complete = 1;
        }

    } while ( !complete );

    db_unwatch_table(hdb, "T");

    db_free_row( wrow );
    db_free_row( waux );

    return del_cnt == upd_cnt && upd_cnt == ins_cnt && ins_cnt == ROW_COUNT ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
example_main(int argc, char **argv)
{
    int rc = EXIT_FAILURE;
    db_t hdb;      // File-storage db

    // Create database
    hdb = create_database( DB_FILENAME, &db_schema );
    if( !hdb ) {
        goto exit;
    }

    if( mutex_init( &mutex ) ) {
        fprintf( stderr, "Couldn't create mutex\n" );
        goto exit;
    }

    {
        os_thread_t *tid;           // Changes thread
        change_args_t change_args = { EXIT_SUCCESS }; // ... and its parameters

        // Spawn child, data-changing thread
        rc = DB_NOERROR == thread_spawn( (thread_proc_t)changes_proc, &change_args, THREAD_JOINABLE, &tid ) ? EXIT_SUCCESS : EXIT_FAILURE;

        // Main thread goes to watching
        rc = rc || watch_notifications(); //< Replace this with some real exit condition

        // Clean data-changing thread
        thread_join( tid );
        rc = rc || change_args.rc;
    }

    mutex_destroy( &mutex );

exit:

    if( hdb ) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);  // A storage cannot be mounted while open.

    }
    return rc;
}
