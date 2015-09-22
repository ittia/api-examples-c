% ITTIA DB SQL File Storage Examples

The File Storage examples show how a C/C++ application can create and use ITTIA DB SQL database files on a file system. Database files are continuously updated as database transactions are committed by the application, and ITTIA DB SQL provides options to control how often this will result in I/O operations being performed.


# atomic_file_storage

The Atomic File Storage example uses transactions to save data in a persistent database file. This demonstrates:

 - Creating a local ITTIA DB SQL database file with a custom storage configuration.
 - Committing changes in an atomic database transaction.
 - Closing a database to discard uncommitted changes.

A database is created by specifying a file name with a relative or absolute path.

```C
#define EXAMPLE_DATABASE "atomic_file_storage.ittiadb"

db_t hdb;
db_file_storage_config_t storage_cfg;

/* Init storage configuration with default settings. */
db_file_storage_config_init(&storage_cfg);

/* Create database with a custom page size. */
storage_cfg.page_size = DB_DEF_PAGE_SIZE * 2;

/* Create a new database file. */
hdb = db_create_file_storage( EXAMPLE_DATABASE, &storage_cfg );
```

After the file is created, the application can add tables, indexes, and other schema elements to the database using the `db_t` connection handle.

The database is modified by inserting, updating, or deleting records using database cursors or SQL statements. Changes are only saved when a transaction is committed. When the database connection is shut down, uncommitted changes are automatically rolled back. This behavior ensures that only complete updates are saved permanently.

```C
db_rc = db_begin_tx( hdb, 0 );
for( i = 0; i < DB_ARRAY_DIM(rows2ins) && DB_OK == db_rc; ++i ) {
    db_rc = db_insert(c, row, &rows2ins[i], 0);
    if( i == 2 ) {
        db_rc = db_commit_tx( hdb, 0 );
        db_rc = db_begin_tx( hdb, 0 );
    }
}
if( DB_OK != db_rc ) {
    print_error_message( "Error inserting or commiting\n", c );
}

db_close_cursor( c );
/* Close database with last two rows left uncommitted. */
db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
```

# bulk_import

The Bulk Import example shows how to efficiently import existing data into a new ITTIA DB SQL database file. This demonstrates:

 - Creating a database file without a transaction log.
 - Recovering from errors during a bulk import.

ITTIA DB SQL uses a transaction log to automatically recover from a crash or unexpected power loss. When a database file is first created, however, the transaction log may cause unnecessary overhead, since the file can be recreated if a problem occurs.

```C
db_t hdb;
db_file_storage_config_t storage_cfg;

db_file_storage_config_init(&storage_cfg);

/* Create a new database with logging disabled. */
storage_cfg.file_mode &= ~DB_NOLOGGING;
hdb = create_database( EXAMPLE_DATABASE, &db_schema, &storage_cfg );
```

A transaction log will be created the next time the file is opened, if the `DB_NOLOGGING` flag is not used. The transaction log is removed automatically when all connections to the database file are closed.


# background_commit

The Background Commit example achieves a high transaction throughput rate by flushing multiple transaction to the storage media at once. By leveraging ITTIA DB SQL transaction completion modes, the application has full control over transaction durability without compromising atomicity. This demonstrates:

 - Committing transactions with lazy transaction completion mode.
 - Manually flushing the transaction journal to persist changes.
 - Closing the database to persist changes.

```C
for( i = 0; i < 100 && DB_OK == db_rc; ++i ) {
    int do_lazy_commit = i % 5;
    db_rc = db_begin_tx( hdb, 0 );
    row2ins.f1 = i+1;
    row2ins.f2 = do_lazy_commit ? 50 : 16;
    db_rc = db_insert(c, row, &row2ins, 0);
    db_commit_tx( hdb, do_lazy_commit ? DB_LAZY_COMPLETION : DB_DEFAULT_COMPLETION );
    if( do_lazy_commit )
        stat->lazy_tx++;
    else
        stat->forced_tx++;

    if( i % 30 ) {
        db_flush_tx( hdb, DB_FLUSH_JOURNAL );
    }
}
```

Transactions committed with the default completion mode are saved to the database file immediately, before the `db_commit_tx` function returns. Transactions committed with the lazy completion mode may not be written to the file until `db_flush_tx` is called.
