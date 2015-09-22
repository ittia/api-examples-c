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

/** @file memory_disk_hybrid.c
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

#include "disk_db_schema.h"

#define EXAMPLE_DATABASE "memory_disk_hybriqd.ittiadb"

typedef enum {
    IN_MEM  = 0,
    ON_DISK = 1,
    CT_GUARD   = 2
} cache_type_t;

/// Caches sizes. - To monitor if cache should be shaped a bit
static int64_t cache_sizes[ CT_GUARD ] = { 0, 0 };
/// Limits to count of records into in-mem (500) and on-disk (10000) caches
static int64_t cache_size_limits[ CT_GUARD ] = { 100, 220 };
/**
 * When cache_sizes[x] limit reaches cache_size_limits[x], delete cache_del_chunks[x] count of the 
 * most aged records from cache 'x'
 */
static int cache_del_chunks[ CT_GUARD ] = { 1, 100 };

#define EMULATE_HOSTSNAMES_COUNT 230
#define CLIENT_REQUESTS 1460

// Helper macro to pull & print (if any) db error
#define GET_ECODE(x,m,c) do {                       \
        x = get_db_error();                         \
        if( DB_NOERROR != x )                       \
            print_error_message(m, c);              \
    } while(0)

int sdb_merge_mem_cache_to_disk();
int resolve_ip_by_hostname( const char * hostname, char *ip);

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

static db_t hdb;
/**
 * Helper function to create DB
 */
db_t 
open_or_create_database(char* database_name, dbs_schema_def_t *schema, db_storage_config_t * storage_cfg)
{
    fprintf( stdout, "enter open_or_create_database\n" );

    int is_opened = 0;

    if( !storage_cfg || DB_FILE_STORAGE == storage_cfg->storage_mode ) {
        /* Create a new file storage database with default parameters. */
        db_file_storage_config_t *file_storage_cfg = storage_cfg ? &storage_cfg->u.file_storage : 0;
        /* Open an existing file storage database with default parameters. */
        hdb = db_open_file_storage(database_name, file_storage_cfg);
        if(hdb == NULL) {
            clear_db_error();
            hdb = db_create_file_storage(database_name, file_storage_cfg);
        } else
            is_opened = 1;
    } else {
        /* Create a new memory storage database with default parameters. */
        db_memory_storage_config_t *mem_storage_cfg = storage_cfg ? &storage_cfg->u.memory_storage : 0;
        hdb = db_create_memory_storage(database_name, mem_storage_cfg);        
    }

    if(hdb == NULL) {
        print_error_message( "Couldn't create DB", NULL );
        return NULL;
    }

    if( !is_opened && dbs_create_schema(hdb, schema) < 0) {
        print_error_message( "Couldn't initialize DB objects", NULL );

        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
//        remove(database_name);
        return NULL;
    }

    fprintf( stdout, "leave open_or_create_database\n" );
    return hdb;
}

// Forward declarations
int sdb_copy_disk_to_mem_cache();
static void open_sequences(db_t hdb);
static void close_sequences();
static void generate_hostname( char * );

int
example_main(int argc, char **argv) 
{
    int rc = DB_FAILURE;
    db_t hdb;                   // db to create and insert rows
    db_storage_config_t storage_config;
    
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    db_storage_config_init(&storage_config);
    db_file_storage_config_init(&storage_config.u.file_storage);

    storage_config.storage_mode = DB_FILE_STORAGE;
    storage_config.u.file_storage.memory_page_size = DB_DEF_PAGE_SIZE * 4/*DB_MIN_PAGE_SIZE*/;
    storage_config.u.file_storage.memory_storage_size = storage_config.u.memory_storage.memory_page_size * 100;
    hdb = open_or_create_database( EXAMPLE_DATABASE, &disk_db_schema, &storage_config );

    db_file_storage_config_destroy(&storage_config.u.file_storage);
    db_storage_config_destroy(&storage_config);
    if (!hdb)
        goto exit;

    if( 0 == sdb_copy_disk_to_mem_cache() ) {
        
        open_sequences( hdb );

        // Emulate clients' requests
        int i = 0;
        rc = DB_NOERROR;
        for( ; i < CLIENT_REQUESTS && DB_NOERROR == rc ; ++i ) {
            char hostname[ MAX_HOSTNAME_LEN + 1 ];
            char ip[ MAX_IP_LEN + 1 ];
            generate_hostname( hostname );            
            rc = resolve_ip_by_hostname( hostname, ip);
            printf( "%d. Hostname: %s, ip: %s, cache_sizes(%"PRId64", %"PRId64"):, rc: %d\n", 
                    i, hostname, ip, cache_sizes[IN_MEM], cache_sizes[ON_DISK], rc );
        }

        rc = sdb_merge_mem_cache_to_disk();

        if( DB_NOERROR == rc && db_is_active_tx( hdb ) )
            db_commit_tx( hdb, 0 );

        close_sequences();

    } else
        fprintf( stderr, "Couldn't load in-mem cache data\n" );

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

  exit:
    return rc == DB_NOERROR ? EXIT_SUCCESS : EXIT_FAILURE;
}

//--------------------- Example Data generators -------------------
/// Emulate DNS-server side for our example
/**
 *  For simplicity of our example, suppose dns request has always successfull result, and each request 
 *  always returns different ip-addreses if even hostname is the same.
 */
void do_dns_server_request( const char * hostname, char * ip )
{
    static int ip_idx = 0;   
    snprintf( ip, MAX_IP_LEN, "%d.%d.%d.%d", 192, 168 + ip_idx++ % 80, 250 - ip_idx % 242, 1 + ip_idx % 250 );
}

/**
   generate names of form "host_a".."host_z", "host_aa".."host_zz"
 */
static 
void generate_hostname( char *hname)
{
    static int hidx = 0;
    int suffix_len = 1;
    
    hidx %= EMULATE_HOSTSNAMES_COUNT;

    if( (int)(hidx / 26 / 26) ) {
        hidx = 0;
        suffix_len = 1;
    } else if( (int)(hidx / 26 ) ) {
        suffix_len = 2;
    } else suffix_len = 1;

    int i = 0;
    strncpy( hname, "host_", MAX_HOSTNAME_LEN );
    
    int hidx_ = hidx++;
    for( ; i < suffix_len; ++i, hidx_/=26 ) {
        hname[ 5 + i ] = 65 + hidx_ % 26;        
    }
    hname[ 5 + suffix_len ] = 0;
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


//--------------------- Example meat ----------------------
static int sdb_search_cache( cache_type_t ctype, const char *hostname, cache_db_row_t *result );
static int sdb_put_into_cache( cache_type_t ctype, cache_db_row_t *result );

/**
   Main entry points for clients.
   - Search for hostname (in order):
     - inside in-mem cache. If found get cache record and return ip addres. Increment request count statistics;
     - inside on-disk cache. If found copy found cache record to in-mem cache. Increment inmem request count statistics;
   - If search both chaches failed, request 'dns-server' (fake generator in this example) to resolve. Put resolved
     data in both in-mem & on-disk caches;
 */
int 
resolve_ip_by_hostname( const char * hostname, char *ip)
{
    int rc = DB_FAILURE;
    cache_db_row_t cache_row;

    // Search in-mem cahce. Searching in-mem cache automatically increase in-mem statistics (age & requests count)
    rc = sdb_search_cache( IN_MEM, hostname, &cache_row );

    if( DB_NOERROR != rc ) {
        if( rc == DB_ENOTFOUND ) {
            rc = sdb_search_cache( ON_DISK, hostname, &cache_row );

            if( DB_NOERROR == rc ) {
                // If on-disk cache contains data requested, copy this record to in-mem cache for late usage by clients
                sdb_put_into_cache( IN_MEM, &cache_row );
            } else if( DB_ENOTFOUND == rc ) {
                do_dns_server_request( hostname, ip );
                strncpy( cache_row.hostname, hostname, MAX_HOSTNAME_LEN ); cache_row.hostname[ MAX_HOSTNAME_LEN ] = 0;
                strncpy( cache_row.hostip, ip, MAX_IP_LEN ); cache_row.hostname[ MAX_IP_LEN ] = 0;
                cache_row.reqcount = 1;
                cache_row.age = next_age();
                rc = sdb_put_into_cache( ON_DISK, &cache_row );
/*
                if( DB_NOERROR == rc )
                    rc = sdb_put_into_cache( IN_MEM, &cache_row );
*/
            }
        } 
    }

    if( DB_NOERROR == rc ) {
        strncpy( ip, cache_row.hostip, MAX_IP_LEN ); ip[ MAX_IP_LEN ] = 0;
    }
    return rc;
}

/**
 *  Search requested cache type for hostname and update statistics, if found.
 */
static 
int sdb_search_cache( cache_type_t ctype, const char *hostname, cache_db_row_t *result )
{
    int rc = DB_FAILURE;
    db_row_t search_row;
    db_row_t result_row;
    db_cursor_t c;
    static db_table_cursor_t p = {
        HOSTS_PKEY_INDEX_NAME,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    const char * table_name = ctype == IN_MEM ? MEM_HOSTS_TABLE : DISK_HOSTS_TABLE;

    search_row = db_alloc_row( NULL, 1 );
    dbs_bind_addr( search_row, HOSTNAME_FNO, DB_VARTYPE_ANSISTR, 
                   (char*)hostname, MAX_HOSTNAME_LEN, NULL );
    
    result_row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );

    c = db_open_table_cursor(hdb, table_name, &p);
    if( DB_OK == db_seek( c, DB_SEEK_FIRST_EQUAL, search_row, 0, 1 ) 
        && DB_OK == db_fetch( c, result_row, result )
        )
    {
        if( IN_MEM == ctype ) {
            result->reqcount++;
            result->age = next_age();
            db_update( c, result_row, result );
        }
    }
    rc = get_db_error();
    if( DB_NOERROR != rc && DB_ENOTFOUND != rc )
        print_error_message( "Fail to search cache", c );
    clear_db_error();

    db_free_row( search_row );
    db_free_row( result_row );
    db_close_cursor( c );

    return rc;
}

static int
sdb_shape_cache( cache_type_t ctype )
{
    int rc = DB_FAILURE;
    db_row_t row;
    db_cursor_t c;
    static db_table_cursor_t p = {
        HOSTS_BY_AGE_INDEX_NAME,       //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    const char * table_name = ctype == IN_MEM ? MEM_HOSTS_TABLE : DISK_HOSTS_TABLE;
    int count2del = cache_del_chunks[ ctype ];
    fprintf( stdout, "Delete %d rows from %s cache\n", count2del, ctype == IN_MEM ? "IN-MEM" : "ON-DISK" );

    row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );

    c = db_open_table_cursor(hdb, table_name, &p);

    for( db_seek_first(c); !db_eof(c) && count2del; --count2del ) {
        db_delete( c, DB_DELETE_SEEK_NEXT );
        cache_sizes[ ctype ]--;
    }

    rc = get_db_error();
    if( DB_NOERROR != rc )
        print_error_message( "Occured when deleting cache records", c );
    db_free_row(row);
    db_close_cursor(c);

    return rc;
}

int sdb_put_into_cache( cache_type_t ctype, cache_db_row_t *data )
{
    int rc = DB_FAILURE;
    db_row_t row;
    db_cursor_t c;
    static db_table_cursor_t p = {
        NULL,       //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    const char * table_name = ctype == IN_MEM ? MEM_HOSTS_TABLE : DISK_HOSTS_TABLE;
    
    if( cache_sizes[ ctype ] > cache_size_limits[ ctype ] ) {
        sdb_shape_cache( ctype );
    }

    row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );

    c = db_open_table_cursor(hdb, table_name, &p);    
    db_insert( c, row, data, 0 );

    rc = get_db_error();
    if( DB_NOERROR == rc ) {
        cache_sizes[ ctype ]++;
        static int commits = 0;        
        if( ON_DISK == ctype && 0 == commits++ % 50) {
            db_commit_tx( hdb, 0 );
        }
    } else
        print_error_message( "Couldn't put data into cache", c );

    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

/// Copy the most recent cache records into in-mem cache ( on application start )
int
sdb_copy_disk_to_mem_cache()
{
    int rc = DB_NOERROR;
    db_row_t row;
    db_cursor_t c_src;
    db_cursor_t c_dst;
    cache_db_row_t data;
    int count2cpy = cache_sizes[ IN_MEM ];

    static db_table_cursor_t p_src = {
        HOSTS_BY_AGE_INDEX_NAME,       //< index
        DB_CAN_MODIFY | DB_SCAN_BACKWARD | DB_LOCK_EXCLUSIVE
    };
    
    static db_table_cursor_t p_dst = {
        NULL,       //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    
    row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );

    c_src = db_open_table_cursor(hdb, DISK_HOSTS_TABLE, &p_src);
    c_dst = db_open_table_cursor(hdb, MEM_HOSTS_TABLE, &p_dst);
    
    for( db_seek_last(c_src); !db_bof(c_src) && DB_NOERROR == rc; db_seek_prior( c_src ) ) {
        ++cache_sizes[ON_DISK];
        if( --count2cpy > 0 ) {         
            if( DB_OK == db_fetch( c_src, row, &data ) ) {
                if( DB_OK == db_insert( c_dst, row, &data, 0 ) ) {
                    ++cache_sizes[IN_MEM];
                }
            }
        }
        rc = get_db_error();
    }
    GET_ECODE( rc, "while copying from on-disk to in-mem caches", NULL );

    db_free_row( row );
    db_close_cursor( c_src );
    db_close_cursor( c_dst );

    return rc;
}

/// When application starts copy the most recent cache records into in-mem cache
int
sdb_merge_mem_cache_to_disk()
{
    int rc = DB_NOERROR;
    db_row_t row;
    db_cursor_t c_src;
    db_cursor_t c_dst;
    cache_db_row_t data;

    int upd_cnt = 0;
    int ins_cnt = 0;
    int inmem_real_size = 0;

    static db_table_cursor_t p_src = {
        HOSTS_BY_AGE_INDEX_NAME,       //< index
        DB_CAN_MODIFY | DB_SCAN_BACKWARD | DB_LOCK_EXCLUSIVE
    };
    
    static db_table_cursor_t p_dst = {
        HOSTS_PKEY_INDEX_NAME,       //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    
    row = db_alloc_row( host_binds_def, DB_ARRAY_DIM( host_binds_def ) );

    c_src = db_open_table_cursor(hdb, MEM_HOSTS_TABLE, &p_src);
    c_dst = db_open_table_cursor(hdb, DISK_HOSTS_TABLE, &p_dst);
    
    for( db_seek_last(c_src); !db_bof(c_src) && DB_NOERROR == rc; db_seek_prior( c_src ) ) {

        inmem_real_size++;
        if( DB_OK == db_fetch( c_src, row, &data ) ) {
            if( DB_OK == db_seek( c_dst, DB_SEEK_FIRST_EQUAL, row, &data, 1 ) ) {
                db_update( c_dst, row, &data );
                upd_cnt++;
            } else {
                rc = get_db_error();
                if( DB_ENOTFOUND == rc ) {
                    clear_db_error();
                    db_insert( c_dst, row, &data, 0 );
                    ins_cnt++;
                }
            }
        }
        rc = get_db_error();

    }
    
    GET_ECODE( rc, "while copying from on-disk to in-mem caches", NULL );
    fprintf( stdout, "Mem->Disk merge result (error_code, inmem_size, inserted, updated): (%d, %d, %d, %d)\n", rc, inmem_real_size, ins_cnt, upd_cnt );

    db_free_row( row );
    db_close_cursor( c_src );
    db_close_cursor( c_dst );

    return rc;
}

