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

/** @file full_memory_storage.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 *
 * In this example we:
 *
 *  - Create a memory storage that is smaller than the amount of data that will be inserted.
 *  - Insert into a memory table many times in separate transactions.
 *  - When memory storage becomes full, delete some old records and try inserting again.
 *
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"
#include "portable_inttypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory_db_schema.h"

#define EXAMPLE_DATABASE "full_memory_storage.ittiadb"

/**
    Records count in 'hosts' table. We use it to estimate when 
    our cache (in it's 'hosts' part) is getting too big.

    If hosts_rows_count >= HOSTS_DB_LIMIT, then we remove 
    OLD_HOSTS_REMOVE_CHUNK from 'hosts' table.
 */
static int hosts_rows_count = 0;

/**
    Records count in 'connstat' table. We use it to estimate when 
    our cache (in it's 'connstat' part) is getting too big:
    
    If connstat_rows_count >= CONNSTAT_DB_LIMIT, then we remove 
    OLD_CONNS_REMOVE_CHUNK from 'connstat' table.
 */
static int connstat_rows_count = 0;

/// Maximum records allowed to hold by "hosts" table @sa hosts_rows_count
#define HOSTS_DB_LIMIT 800
/// Maximum records we allow to store in "connstat" table @sa connstat_rows_count
#define CONNS_DB_LIMIT ( HOSTS_DB_LIMIT * 4 )

/// When HOSTS_DB_LIMIT reached drop this count of old hosts rows @sa hosts_rows_count
#define OLD_HOSTS_REMOVE_CHUNK 10
/// When CONNS_DB_LIMIT reached drop this count of old connstat rows @sa connstat_rows_count
#define OLD_CONNS_REMOVE_CHUNK 20

// Synthetic data generator definitions:
#define HOSTS_COUNT 1000    ///< Count of unique host IPs which example data provider can generate
#define PORTS_COUNT 10      ///< Count of unique ports which example data provider can generate

// Helper macro to pull & print (if any) db error
#define GET_ECODE(x,m,c) do {                       \
        x = get_db_error();                         \
        if( DB_NOERROR != x )                       \
            print_error_message(m, c);              \
    } while(0)

/// Input/output statistics for remote host connection
/** 
    This is a structure which 'external' (to this module)
    part of software uses to exchange data with this module 
*/
typedef struct {
    char hostip[MAX_IP_LEN];    ///< IP of remote host
    int  dport;    ///< remote connection port
    int  sport;    ///< local connection port
    int64_t  io_bytes;  ///< in + out bytes transfered
} io_stat_row_t;

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

/**
 * Helper function to create DB
 */
db_t 
create_database(char* database_name, dbs_schema_def_t *schema, db_storage_config_t * storage_cfg)
{
    fprintf( stdout, "enter create_database\n" );
    db_t hdb;

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
        print_error_message( "Couldn't create DB", NULL );
        return NULL;
    }

    if (dbs_create_schema(hdb, schema) < 0) {
        print_error_message( "Couldn't initialize DB objects", NULL );

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
//        remove(database_name);
        return NULL;
    }

    fprintf( stdout, "leave create_database\n" );
    return hdb;
}
/// Sequence we use to provide module with pkey values & 'age' of cache records 
static db_sequence_t age_seq = NULL;
static void
open_sequences(db_t hdb)
{
    age_seq = db_open_sequence(hdb, AGE_SEQUENCE);
}
static void
close_sequences()
{
    db_close_sequence( age_seq );
}

/**
 * Next sequence value helper function.
 */
static uint64_t 
seq_next( db_sequence_t hseq )
{
    db_seqvalue_t v;

    if (db_next_sequence(hseq, &v) == NULL)
        return 0;

    return v.int32.low;
}

/**
 *  Get 'current' age of cache
 */
static int64_t 
next_age()
{
    return seq_next( age_seq );
}

/**
 *  Remove 'count' of the most old/aged records from 'hosts' table.
 *  Recalc 'hosts_rows_count' & connstat_rows_count statistics.
 */
int
sdb_remove_old_hosts( db_t hdb, int count )
{
    int rc = DB_NOERROR;
    // Define a cursor sorted by HOSTS_BY_AGE_INDEX_NAME index.
    db_table_cursor_t   p = { HOSTS_BY_AGE_INDEX_NAME, DB_CAN_MODIFY | DB_LOCK_DEFAULT };
    db_cursor_t c;
    db_row_t row;
    int32_t ccount;
    int32_t hostid;    

    clear_db_error();

    fprintf(stdout, "About to delete %d/%d hosts ", count, hosts_rows_count );
    c = db_open_table_cursor(hdb, HOSTS_TABLE, &p);

    row = db_alloc_row( NULL, 2 );
    dbs_bind_addr( row, HOSTID_FNO, DB_VARTYPE_SINT32, 
                   &hostid, sizeof(hostid), NULL );
    dbs_bind_addr( row, CONNCOUNT_FNO, DB_VARTYPE_SINT32, 
                   &ccount, sizeof(ccount), NULL );

    if( DB_OK != db_seek_first(c) )
        GET_ECODE(rc, "Couldn't seek_first hosts table while remove some host", c );

    // Do 'count' of deletes in a loop
    for( ; !db_eof(c) && count && rc == DB_NOERROR; --count ) {
        if( DB_OK == db_fetch( c, row, NULL ) ) {
            /* 
               Fetch (from record about to be deleted) current connections count, to
               recalculate our global 'connstat_rows_count' statistics.
            */
            if( DB_OK == db_delete( c, DB_DELETE_SEEK_NEXT ) ) {
                // Delete & decrement [hosts|connstat]_rows_count
                --hosts_rows_count;
                /* Deletes from hosts table do cascade deletion ( by cascade detete fkey ) 
                   from 'connstat' table, so correct 'connstat_rows_count' too */
                connstat_rows_count -= ccount;
                fprintf( stdout, "(hostid, ccount): (%d,%d)\n", hostid, ccount );
            } else
                GET_ECODE( rc, "Couldn't delete row from hosts table", c );
        } else
            GET_ECODE( rc, "Couldn't fetch hosts table while deleting hosts", c );
    }
    if( DB_NOERROR == rc )
        GET_ECODE(rc, "Couldn't remove record(s) from host table", c );

    db_free_row( row );
    db_close_cursor( c );
    return rc;
}

/**
 *  Append record into 'hosts' table
 */
int
sdb_add_host( db_t hdb, const char *hostip, int64_t iostat, int32_t *hostid )
{
    int rc = DB_NOERROR;
//    int tries = 3;
    host_db_row_t data;
    db_row_t row;
    static db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    if( HOSTS_DB_LIMIT <= hosts_rows_count ) 
        rc = sdb_remove_old_hosts( hdb, OLD_HOSTS_REMOVE_CHUNK );

    if( DB_NOERROR == rc ) {
        memset( &data, 0, sizeof(host_db_row_t) );
    
        *hostid = data.hostid = data.age = next_age();
        data.iostat = iostat;
        strncpy( data.hostip, hostip, MAX_IP_LEN );

        row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );
        c = db_open_table_cursor(hdb, HOSTS_TABLE, &p);

        fprintf( stdout, "AddHost_%d: (id,ip,age): (%"PRId32", %s, %"PRId64")\n", hosts_rows_count, *hostid, hostip, data.age );
        if( DB_OK != db_insert( c, row, &data, 0 ) ) {
            rc = get_db_error();
            print_error_message( "Couldn't insert hosts table record.", c );
        } else
            hosts_rows_count++;

        db_free_row( row );
        db_close_cursor( c );
    }
    return rc;
}

/**
 *  Lookup 'hosts' table to find out 'hostid' by 'hostip'.
 */
int 
sdb_find_host_by_ip( db_t hdb, const char * hostip, int32_t * hostid )
{
    int rc = DB_ENOTFOUND;
    db_table_cursor_t   p = { HOSTS_BY_IP_INDEX_NAME, DB_SCAN_FORWARD | DB_LOCK_DEFAULT };
    db_cursor_t c;
    db_row_t row;    
    char ip[MAX_IP_LEN + 1];

    strncpy( ip, hostip, MAX_IP_LEN ); ip[ MAX_IP_LEN ] = 0;

    c = db_open_table_cursor(hdb, HOSTS_TABLE, &p);
    
    row = db_alloc_row( NULL, 2 );
    dbs_bind_addr(row, HOSTIP_FNO, DB_VARTYPE_ANSISTR, &ip, DB_ARRAY_DIM(ip), NULL );

    if( DB_OK == db_seek( c, DB_SEEK_FIRST_EQUAL, row, 0, 1 ) ) {
        dbs_bind_addr(row, HOSTID_FNO, DB_VARTYPE_SINT32, hostid, sizeof(int32_t), NULL );
        db_fetch( c, row, NULL );        
        rc = DB_NOERROR;
    } else {
        rc = get_db_error();
        if( rc != DB_ENOTFOUND )
            print_error_message( "Error to seek host by ip", c );
        clear_db_error();
    }
    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

/**
 *  Increment summary host statistics. - Just update 'hosts' record.
 */
int sdb_inc_host_stat_( db_t hdb, int32_t hostid, int64_t new_age, int32_t ccount_delta, int64_t iostat_delta ) 
{
    int rc = DB_ENOTFOUND;
    db_table_cursor_t   p = { HOSTS_PKEY_INDEX_NAME, DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE };
    int32_t ccount;
    int64_t iostat;
    int64_t age = new_age;
    db_cursor_t c;
    db_row_t row;    
    db_result_t db_rc = DB_FAIL;

    c = db_open_table_cursor(hdb, HOSTS_TABLE, &p);
    row = db_alloc_row( NULL, 4 );
    // Seek 'hosts' by hostid
    dbs_bind_addr(row, HOSTID_FNO, DB_VARTYPE_SINT32, &hostid, sizeof(int32_t), NULL);
    if( DB_OK == db_seek( c, DB_SEEK_FIRST_EQUAL, row, 0, 1 ) ) {
        dbs_bind_addr(row, HOSTIOSTAT_FNO, DB_VARTYPE_SINT64, &iostat, sizeof(int64_t), NULL );
        dbs_bind_addr(row, CONNCOUNT_FNO, DB_VARTYPE_SINT32, &ccount, sizeof(int32_t), NULL);
            
        if( DB_OK == db_fetch( c, row, NULL ) ) {
            if( new_age > 0 )   // If age should be updated bind it to updating row
                dbs_bind_addr(row, HOSTAGE_FNO, DB_VARTYPE_SINT64, &age, sizeof(age), NULL );
            ccount += ccount_delta;
            iostat += iostat_delta;
            db_update( c, row, NULL );
            fprintf( stdout, "hostid: %d: %d->conncount, %"PRId64"->iostat, %"PRId64"->age\n", hostid, ccount, iostat, age );
            GET_ECODE( rc, "Couldn't update hosts.conncount column", c );
        } else GET_ECODE( rc, "Can't fetch hosts while updating hosts.conncount column", c );
    } else
        GET_ECODE( rc, "Couldn't seek hosts table", c );

    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

/**
 *  Removes 'count' of the most aged 'connstat' records.
 *  Corrects summary hosts statistics.
 */
int
sdb_remove_old_conns( db_t hdb, int count )
{
    int rc = DB_NOERROR;
    db_table_cursor_t   p = { CONNSTAT_BY_AGE_INDEX_NAME, DB_CAN_MODIFY | DB_LOCK_DEFAULT };
    db_cursor_t c;
    db_row_t row;
    int32_t hostid;
    int64_t iostat;
    
    fprintf(stdout, "About to delete %d/%d connstat record(s)\n", count, connstat_rows_count );

    c = db_open_table_cursor(hdb, CONNSTAT_TABLE, &p);
    row = db_alloc_row(NULL, 2);
    dbs_bind_addr(row, HOSTID_FNO, DB_VARTYPE_SINT32, &hostid, sizeof(int32_t), NULL);
    dbs_bind_addr(row, IOSTAT_FNO, DB_VARTYPE_SINT64, &iostat, sizeof(int64_t), NULL );

    db_seek_first(c);
    while( !db_eof(c) && DB_NOERROR == rc && count--) {
        db_fetch( c, row, 0 );
        // Decrement summary statistics of host who owns this connection
        rc = sdb_inc_host_stat_( hdb, hostid, -1, -1, -iostat );
        if( DB_NOERROR == rc ) {
            if( DB_OK == db_delete(c, DB_DELETE_SEEK_NEXT)  ) 
                connstat_rows_count--; 
            else
                GET_ECODE( rc, "Couldn't db_delete on connstat table", c );
        }
    }
    rc = DB_NOERROR == rc ? get_db_error() : rc;

    if( DB_NOERROR != rc )
            print_error_message( "Couldn't remove record(s) from connstat table", c );

    db_free_row( row );
    db_close_cursor( c );
    return rc;
}

/**
 *  Insert/Update 'connstat' table with 'stat' data.
 */
int
sdb_inc_conn_stat( db_t hdb, int32_t hostid, const io_stat_row_t *stat )
{
    int rc = DB_ENOTFOUND;
    db_table_cursor_t   p = { CONNSTAT_PKEY_INDEX_NAME, DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE };
    db_cursor_t c;
    db_row_t row;    
    db_result_t db_rc = DB_FAIL;
    int64_t iostat = 0;
    int64_t age = 0; 
    io_stat_row_t stat_ = *stat;
    int is_new_conn = 0;

    c = db_open_table_cursor(hdb, CONNSTAT_TABLE, &p);
    
    row = db_alloc_row( NULL, 5 );

    dbs_bind_addr(row, HOSTID_FNO, DB_VARTYPE_SINT32, &hostid, sizeof(int32_t), NULL );
    dbs_bind_addr(row, DPORT_FNO, DB_VARTYPE_UINT32, &stat_.dport, sizeof(uint32_t), NULL );
    dbs_bind_addr(row, SPORT_FNO, DB_VARTYPE_UINT32, &stat_.sport, sizeof(uint32_t), NULL );
    dbs_bind_addr(row, IOSTAT_FNO, DB_VARTYPE_SINT64, &iostat, sizeof(int64_t), NULL );
    dbs_bind_addr(row, CONNAGE_FNO, DB_VARTYPE_SINT64, &age, sizeof(age), NULL );

    // Search connstat table for existing record
    if( DB_OK == db_seek( c, DB_SEEK_FIRST_EQUAL, row, 0, 3 ) ) {
        // Found.  - Increment iostat & update.
        db_fetch( c, row, NULL );
        iostat += stat_.io_bytes;
        age = next_age();
        db_update( c, row, NULL );
        fprintf( stdout, "hostid: %d: %"PRId64"->iostat\n", hostid, iostat );
        GET_ECODE(rc, "Couldn't update connstat row", c);
    } else {
        rc = get_db_error();
        if( rc == DB_ENOTFOUND ) {
            // Not found. Inserting...
            clear_db_error();
            // If count or records in 'connstat' table reached CONNS_DB_LIMIT, drop some old rows before insert.
            if( CONNS_DB_LIMIT > connstat_rows_count
                || DB_NOERROR == ( rc = sdb_remove_old_conns( hdb, OLD_CONNS_REMOVE_CHUNK ) )
                )
            {
                rc = DB_NOERROR;
                iostat = stat->io_bytes;
                age = next_age();
                if( DB_OK != db_insert( c, row, NULL, 0 ) )
                    GET_ECODE(rc, "Couldn't insert connstat table record.", c);
                else {
                    is_new_conn = 1;
                    connstat_rows_count++;
                }
            } else
                GET_ECODE(rc, "Couldn't search connstat table record.", c);           
        } else
            GET_ECODE(rc, "Couldn't search connstat table", c );
    }
    // Dont forget to increment summary statistics we collect in 'hosts' table
    if( DB_NOERROR == rc )
        rc = sdb_inc_host_stat_( hdb, hostid, age, is_new_conn, stat_.io_bytes );

    if( DB_NOERROR != rc )
        print_error_message( "Couldn't increment hosts.conncount column.", c );

    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

/// Increment connection in/out statistics by io_stat.io_bytes count
int
sdb_inc_io_stat( db_t hdb, const io_stat_row_t * io_stat)
{
    int rc = DB_FAILURE;
    int32_t hostid = 0;   
    int tries = 3;

    int save_hosts_rows_count = hosts_rows_count;
    int save_connstat_rows_count = connstat_rows_count;

    db_begin_tx( hdb, 0  );

    // Find out hostid
    rc = sdb_find_host_by_ip( hdb, io_stat->hostip, &hostid );
    if( rc == DB_ENOTFOUND ) {
        clear_db_error();
        // No such host found in cache. Put it into
        rc = sdb_add_host( hdb, io_stat->hostip, 0, &hostid );
    }
    if( DB_NOERROR == rc ) {
        // Increment statistics
        rc = sdb_inc_conn_stat( hdb, hostid, io_stat );
    }

    if( DB_NOERROR == rc )
        db_commit_tx( hdb, 0  );
    else {
        db_abort_tx( hdb, DB_FORCED_COMPLETION );
        hosts_rows_count = save_hosts_rows_count;
        connstat_rows_count = save_connstat_rows_count;
        fprintf(stderr, "rollback...\n");
    }

    if( DB_NOERROR != rc )
        print_error_message("Increment io stat error", NULL);

    if( DB_ENOPAGESPACE == rc || DB_ENOMEM == rc ) {
        clear_db_error();
        fprintf( stdout, "No memory left. Consider to lower value of HOSTS_DB_LIMIT/CONNSTAT_DB_LIMIT macro\n" 
                 "  or reserve more mem for ITTIA DB storage (memory_page_size/memory_storage_size)\n"
            );
    }

    return rc;
}

/// Example data generator
void 
generate_iostat_row( io_stat_row_t * r, int solt )
{
    static char hosts[ HOSTS_COUNT % ( 240 * 240 * 240) ][ MAX_IP_LEN + 1 ];
    static uint32_t ports[ PORTS_COUNT % 65000 ];
    static int host_idx = -1, port_idx = 0;

    if( -1 == host_idx ) {
        host_idx = 0;
        for( ; host_idx < HOSTS_COUNT %( 240 * 240 * 240 ); ++host_idx ) {
            sprintf( hosts[ host_idx ], "192.%d.%d.%d", 
                     host_idx/240/240 % 240 + 10, 
                     host_idx/240 % 240 + 10,
                     host_idx % 240 + 10
                );
            //fprintf( stdout, "%d. Genip: %s\n", host_idx, hosts[ host_idx-1 ] );
        }
        for( ; port_idx < PORTS_COUNT; ++port_idx ) ports[ port_idx ] = 5000 + port_idx;            
    }

    host_idx += port_idx % PORTS_COUNT ? 0 : 1;

    strncpy( r->hostip, hosts[ host_idx % HOSTS_COUNT ], MAX_IP_LEN ); r->hostip[ MAX_IP_LEN ] = 0;
    r->dport = ports[ port_idx++ % PORTS_COUNT ];
//    r->sport = ports[ port_idx % PORTS_COUNT ];
    r->sport = ports[ ( PORTS_COUNT - ( port_idx++ % PORTS_COUNT ) ) % PORTS_COUNT ];
    r->io_bytes = 10;

    return;
}

/// Example statistics collector
int 
collect_statistics( db_t hdb )
{
    int rc = EXIT_SUCCESS;
    // Generate collection of io_stat_row_t recors and insert them to DB
    int i;
    for( i = 0; i < 50000 && 0 == rc; ++i ) {
        io_stat_row_t r;
        generate_iostat_row( &r, i );
        if (DB_NOERROR != sdb_inc_io_stat( hdb, &r )) {
            rc = EXIT_FAILURE;
        }
    }
    return rc;
}

void 
resolver_thread_proc( void * args )
{
    /* 
       This thread search "hosts" table for records with NULL in 'hostname' column.
       Using hosts.hostip value it 'resolve real' hostname and update those record.
    */
    // ......    
}

#ifdef DEBUG
// Just for debug. - Print out connstat content
void dump_connstat( db_t hdb )
{
    cs_db_row_t data;
    db_row_t row;
    static db_table_cursor_t p = {
        CONNSTAT_PKEY_INDEX_NAME,   //< No index
        DB_SCAN_FORWARD | DB_LOCK_DEFAULT
    };
    db_cursor_t c;
    int cnt = 0;

    memset( &data, 0, sizeof(cs_db_row_t) );

    row = db_alloc_row( cs_binds_def, DB_ARRAY_DIM( cs_binds_def ) );
    c = db_open_table_cursor(hdb, CONNSTAT_TABLE, &p);
    fprintf( stdout, "connstat table dump:\n" );
    for( db_seek_first(c); !db_eof(c); db_seek_next(c) ) {
        db_fetch(c, row, &data );
        fprintf( stdout, "%d,\t%d,\t%d,\t%"PRId64",\t%"PRId64"\n",
                 data.hostid, data.dport, data.sport, data.iostat, data.age);
        cnt++;
    }
    fprintf( stdout, "connstat table records count: %d\n", cnt );
    db_free_row( row );
    db_close_cursor( c );
}
#endif

int
example_main(int argc, char **argv) 
{
    int rc = EXIT_FAILURE;
    db_t hdb;                   // db to create and insert rows
    db_storage_config_t storage_config;
    
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    db_storage_config_init(&storage_config);
    db_memory_storage_config_init(&storage_config.u.memory_storage);

    storage_config.storage_mode = DB_MEMORY_STORAGE;
    storage_config.u.memory_storage.memory_page_size = DB_DEF_PAGE_SIZE * 4/*DB_MIN_PAGE_SIZE*/;
    storage_config.u.memory_storage.memory_storage_size = storage_config.u.memory_storage.memory_page_size * 100;
    hdb = create_database( EXAMPLE_DATABASE, &db_schema, &storage_config );

    db_memory_storage_config_destroy(&storage_config.u.memory_storage);
    db_storage_config_destroy(&storage_config);
    if (!hdb)
        goto exit;

    open_sequences( hdb );

    // Start resolver thread
    // Start traffic statistics generation
    rc = collect_statistics( hdb );

#ifdef DEBUG
    if( DB_NOERROR == rc ) {
        dump_connstat( hdb );
    }
#endif

    close_sequences();
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

  exit:
    return rc;
}

#undef HOSTS_COUNT
#undef PORTS_COUNT
#undef HOSTS_DB_LIMIT
#undef CONNS_DB_LIMIT
#undef OLD_HOSTS_REMOVE_CHUNK
#undef OLD_CONNS_REMOVE_CHUNK
#undef GET_ECODE

