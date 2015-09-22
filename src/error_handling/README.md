% ITTIA DB SQL Error Handling Examples

ITTIA DB SQL uses database transactions to protect data integrity. When an error occurs in either the application or database API, incomplete changes to the database storage can be discarded by rolling back the current transaction. This approach preserves the consistency of related database records even if data is malformed or would violate defined constraints before it is stored in the database.

# transaction_rollback

The Transaction Rollback example uses transactions to maintain consistency when an error is encountered in a data stream. This demonstrates:

 - Handling constraint violations with transaction rollback.

The example reads packets of data from a simulated sensor. When a packet is completed, the results are committed, but only if the packet data is determined to be accurate.

```C
/* Start a new transaction when a new packet is encountered */
if( current_packet != readings[i].packetid ) {
    /* Commit only if the last packet contained accurate data */
    if( is_wrong_packet ) {
        /* Previous packet had bad data, so roll back its changes */
        if ( DB_OK != (rc = db_abort_tx( hdb, 0 )) ) {
            print_error_message( "unable to roll back packet data", c );
        }
    }
    else if ( in_tx ) {
        /* Commit previous packet's changes */
        if ( DB_OK == (rc = db_commit_tx( hdb, 0 )) ) {}
        good_packets++;
    }
    else {
        print_error_message( "unable to commit packet data", c );
    }
    is_wrong_packet = 0;
    current_packet = readings[i].packetid;
    rc = db_begin_tx( hdb, 0 );
    in_tx = 1;
}
```

# savepoint_rollback

The Savepoint Rollback example uses savepoints to handle errors in a function that is part of a larger transaction. This demonstrates:

 - Handling recoverable errors within a large multi-step transaction.
