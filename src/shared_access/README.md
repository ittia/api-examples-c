% ITTIA DB SQL Shared Access Examples

An application can share ITTIA DB SQL database storages, whether on disk or in memory, between its own threads and with other applications. The integrity of the database is protected by transaction isolation, which ensures that concurrent tasks do not interfere with each other. ITTIA DB SQL uses an intelligent internal locking protocol to minimize blocking, so applications do not need to access data in critical section.

The Shared Access examples show how an application can share a database with concurrent tasks.

# embedded_database_server

The Embedded Database Server example runs the ITTIA DB SQL server in the background of an application, providing remote access to a local database file. This demonstrates:

 - Mounting an existing database file for remote access.
 - Starting an ITTIA DB SQL database server embedded in an application.

```C
if ( DB_OK == db_mount_storage( DB_FILENAME, DB_ALIAS, NULL ) ) {
    db_server_config_t srv_config;
    db_server_config_init( &srv_config );

    /* Start ITTIA DB Server in this process */
    if ( DB_OK == db_server_start( &srv_config ) ) {
        fprintf( stdout, "Server started in background threads.\n" );
        fprintf( stdout, "Database file [%s] mounted with alias [%s].\n", DB_FILENAME, DB_ALIAS );
        fprintf( stdout, "You have %d second(s) to open a connection.\n", seconds_to_work );

        /* This process can still open the mounted database directly, even
         * while it is shared with other processes by the server. */
        hdb = db_open_file_storage( DB_FILENAME, NULL );

        /* Exit after the timer has elapsed. */
        /* Replace this with a real event loop and exit condition. */
        while ( seconds_to_work-- ) {
            os_sleep( WAIT_MILLISEC( 1000 ) );
            fputc( '.', stdout );
        }
        fputc( '\n', stdout );
        
        db_server_stop( 0 );

        rc = EXIT_SUCCESS;
    }
}
```

# online_backup

The Online Backup example copies an open ITTIA DB SQL database file without interrupting modifications made by a background task. This example demonstrates:

 - Database backup in the background of a running application.
 - Restoring a backup by opening the database copy.

# data_change_notification

The Data Change Notification example receives notification events when certain tables are modified in an ITTIA DB SQL database. This example demonstrates:

 - Subscribing to receive notification when a database table is modified.
 - Inserting and updating in a background task.
 - Handling change notification events.
