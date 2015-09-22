% ITTIA DB SQL Statement Examples

The SQL Statement examples show how a C/C++ application can use SQL to access and modify a database file. SQL is a declarative programming language that handles the details of locating and processing records stored in indexed tables. An application uses browsable cursors to retrieve data from an SQL statement's result set. Query parameters can also be used to supply data and filter criteria to the SQL statement.

# sql_select_query

The SQL Select Query example prepares and executes an SQL query in an ITTIA DB SQL database. This demonstrates:

 - Preparing and executing a SELECT statement.
 - Iterating over a result set and fetching rows.
 - Limiting the result set with FETCH FIRST and OFFSET clauses.

When executed, an SQL select query produces a result set. This example uses a simple query that selects all rows and columns from a table.

```SQL
select id, data
  from storage
```

Each field in the result set is bound to a local variable using a binding array. The fields are numbered according to their position in the select list, starting with 0.

```C
int32_t id, data;
const db_bind_t row_def[] = {
    DB_BIND_VAR( 0, DB_VARTYPE_SINT32, id ),
    DB_BIND_VAR( 1, DB_VARTYPE_SINT32, data ),
};
db_row_t r = db_alloc_row( row_def, DB_ARRAY_DIM( row_def ) );
```

After an SQL cursor is executed, records are processed by iterating over the cursor and fetching data through the row bindings. The fetch operation copies data from the result set into the local variables.

```C
for ( row = 0, db_seek_first( sql_cursor ); !db_eof( sql_cursor ); db_seek_next( sql_cursor ), ++row ) {
    db_fetch( sql_cursor, r, NULL );
    process_record( id, data );
}
```

# sql_parameters

The SQL Parameters example sets SQL data and search criteria with query parameters, to avoid modifying the SQL query string at run-time. This demonstrates:

 - Executing a prepared SQL query multiple times with different parameter values.
 - Paging a result set with parameterized FETCH FIRST and OFFSET clauses.

An SQL query is parameterized by replacing literal values with `?` placeholders.

```SQL
select id, data
  from storage
  where id > ? and data > ?
```

Each placeholder has a corresponding a parameter field, starting with field number 0 and numbered according to the order they appear in the SQL statement. The parameter fields can be bound to local variables with a binding array.

```C
int32_t id_param, data_param;
const db_bind_t params_def[] = {
    DB_BIND_VAR( 0, DB_VARTYPE_SINT32, id_param ),
    DB_BIND_VAR( 1, DB_VARTYPE_SINT32, data_param ),
};
db_row_t params_row = db_alloc_row( params_def, DB_ARRAY_DIM( params_def ) );
```

Before the cursor is executed, the parameter variables must each be assigned a value.

```C
/* Set query parameters */
id_param = 98;
data_param = 0;

/* Execute prepared cursor */
if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
    print_error_message( sql_cursor, "executing parametrized query" );
    goto select_query_exit;
}
```

Unexecuting the cursor returns it to the prepared state so that it can be executed again with different parameters.

```C
db_unexecute( sql_cursor );
id_param--; data_param++;

if( DB_OK != db_execute( sql_cursor, params_row, NULL ) ) {
```
