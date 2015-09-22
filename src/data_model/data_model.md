% ITTIA DB SQL Data Modeling Examples

ITTIA DB SQL uses the relational data model to organize data into indexed tables. Related information is divided into separate tables that can be joined together with SQL queries in a flexible way. The database schema defines the structure of the tables, including foreign key constraints that enforce table relationships, and can evolve over time as the needs of the application change with each upgrade.

The Data Modeling examples show how to use ITTIA DB SQL data types, constraints, and related features to manage data in an embedded application.

# schema_upgrade

The Schema Upgrade example simulates an application update that includes changes to the database schema. A schema version number is incremented for each new release of the application, and a schema version table is used to determine how a database file should be upgraded each time it is opened. This example demonstrates:

 - Using a schema version table to manage application upgrades.
 - Adding a new column to an existing non-empty table.
 - Adding indexes to an existing table to support new queries.
 - Removing indexes that the application no longer needs.

In this example, a `SCHEMA_VERSION` table is created to store an application-defined version number. This table will always contain a single row.

```SQL
create table schema_version (
  version bigint not null
)
```

When a database is opened, the example uses the contents of this table and a schema data structure to determine whether or not the schema must be upgraded.

```C
hdb = open_database( EXAMPLE_UPGRADE_DATABASE, &v2_schema, &upgrade_schema_cb );
```

If an upgrade is required, a callback function is used to perform the upgrade. Each incremental upgrade is implemented in a separate function so that the upgrade process can start from any previous version.

```C
static int
upgrade_schema_cb( db_t hdb, int old_version, int new_version )
{
    db_result_t rc = DB_OK;

    fprintf( stdout, "Upgrading schema v%d to v%d\n", old_version, new_version );

    if ( old_version < 1 || new_version > 3 ) {
        print_error_message( "Schema upgrade path not supported", NULL );
        return -1;
    }

    if ( DB_OK == rc && old_version < 2 && new_version >= 2 ) {
        rc = upgrade_to_schema_v2( hdb );
    }

    if ( DB_OK == rc && old_version < 3 && new_version >= 3 ) {
        rc = upgrade_to_schema_v3( hdb );
    }

    return DB_OK != rc ? -1 : 0;
}
```

# foreign_key_constraints

The Foreign Key Constraints example uses table relationships to ensure consistency between related tables. Foreign keys can be used to either cascade or prevent update and delete operations in referenced tables. This example demonstrates:

 - Cascading update and delete operations.
 - Restricted update and delete operations.

# unicode_character_strings

The Unicode Character Strings example stores text translated into multiple languages in an ITTIA DB SQL database table. To look up a localized string, the application supplies an identifier and a language code. This example demonstrates:

 - Storing localized text in a database table.
 - Reading Unicode-encoded text from a database table.

# datetime_intervals

The Datetime and Intervals example uses ITTIA DB SQL data types to store date, time, and duration values. This example demonstrates:

 - Adding a timestamp to a row inserted with SQL.
 - Using standard date and time format strings.
 - Using custom date and time format strings.
 - Extracting hours, minutes, and seconds from a time column.
 - Extracting years, months, and days from a date column.
 - Converting date and time columns to and from a UNIX timestamp.
 - Adding months, years, or days to a date column.
 - Adding hours, minutes, or seconds to a time column.

# text_import

The Text Import example reads text from a comma-separated values (CSV) format into a database table. This example demonstrates:

 - Converting text to other data types.
 - Reporting type conversion errors.

# text_export

The Text Export example outputs the contents of a database table in a comma-separated values (CSV) format. This demonstrates:

 - Converting ITTIA DB SQL data types to text.
 - Reading column names from a database cursor.
 - Escaping quote characters for compatibility with spreadsheet applications.

# sql_export

The SQL Export example converts an ITTIA DB SQL database to a standard SQL format. This demonstrates:

 - Converting a database schema to SQL DDL statements.
 - Converting table data to SQL INSERT statements.
