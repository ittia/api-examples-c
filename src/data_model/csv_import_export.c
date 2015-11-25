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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "csv_import_export.h"

const char *EOL = "\r\n";
const char FIELD_DELIM = ',';
const char FIELD_QUOTE = '"';

/// Callback to fetch more csv data from input buffer
typedef ptrdiff_t ( *read_more_data_callback_t )( char ** d, void * context );
/// Callback to call to store just parsed field data
typedef int (*got_field_callback_t)( int lineno, int fieldno, size_t pos, const char * data, size_t len, void * db_context);
/// Callback to call to storee just parsed line
typedef int (*got_line_callback_t)( int lineno, int fields, size_t pos, void * db_context);

/// Internal data to use in read_more_data_callback_t callback of 'In-mem string buffer' type
typedef struct {
    const char * buffer;    ///< Input buffer
    size_t buffer_size;     ///< Buffer size
    size_t read_size;       ///< Overall read size
} read_more_inmem_data_context_t;

/// Utility DB error log function
extern void print_error_message(char * message, ...);

/// Method to prepare DB context (row, cursor, fields binding ...) and launch csv parser
static int do_import( db_t hdb, const char *tname, const csv_import_options_t *, read_more_data_callback_t cb, void *cb_data );

/// Callback to call by parse_input() csv parser to store in DB row just parsed field data
static int got_field_cb( int lineno, int fieldno, size_t pos, const char * data, size_t len, void * db_context);
/// Callback to call by parse_input() csv parser to insert in DB just parsed line
static int got_line_cb( int lineno, int fields, size_t pos, void * db_context);

/// Callback to call by parser_input() to fetch more csv data from input 'stream'
static ptrdiff_t get_more_inmem_data_cb( char **data, void * cb_data );

/// Csv parser function
static void parse_input( read_more_data_callback_t cb, void *cb_data, got_field_callback_t got_field_cb, got_line_callback_t got_line_cb, void *db_cb_data );

/// Callback that fetch more data from input data_provider (in-memory string buffer)
/** Returns size of data read in call or -1 if some error */
static ptrdiff_t
get_more_inmem_data_cb( char ** data, void * cb_data )
{
    int chunk_len;
    read_more_inmem_data_context_t * ctx = (read_more_inmem_data_context_t *)cb_data;

    if( ctx->read_size >= ctx->buffer_size ) {
        return 0;
    }
    *data = (char *)ctx->buffer + ctx->read_size;
    // Just to emulate file io do reading in 10byte chunks
#define CHUNK_SIZE 10
    chunk_len = ( ctx->buffer_size - ctx->read_size > CHUNK_SIZE ) ? CHUNK_SIZE : ( ctx->buffer_size - ctx->read_size );
#undef CHUNK_SIZE
    ctx->read_size += chunk_len;

    return chunk_len;
}

/// Perform import from inmem buffer
int
csv_import(
    db_t hdb, const char * table_name, const char * buffer, size_t buffer_size,
    csv_import_options_t * import_options )
{
    csv_import_options_t ioptions = { LINE_COMMIT, USE_HEADER };
    read_more_inmem_data_context_t cb_data = { buffer, buffer_size, 0 };

    if( import_options ) {
        ioptions = *import_options;
    }

    return do_import( hdb, table_name, &ioptions, get_more_inmem_data_cb, &cb_data );
}

typedef struct {
    db_t hdb;
    db_cursor_t tab;
    db_row_t hrow;
    db_fielddef_t * fields;
    int num_fields;
    int have_tx;
    csv_import_options_t ioptions;
    int failed;
} db_context_t;

/// Put extracted field data into DB row (db_context->hrow)
static int got_field_cb( int lineno, int fieldno, size_t in_csv_pos, const char * data, size_t len, void * db_context)
{
    db_context_t * ctx = (db_context_t *)db_context;
    if( lineno == 0 ) {
        switch( ctx->ioptions.header_mode ) {
        case IGNORE_HEADER: return 0;
        case NO_HEADER: break;
        case USE_HEADER: {
            int j;
            /// Handle column name got from csv header
            db_fieldno_t col_num = db_find_field( ctx->tab, data );
            if (col_num < 0) {
                print_error_message("unable to find column %s in target table\n", data);
                return -1;
            }
            for (j = fieldno; j < ctx->num_fields; j++) {
                if (ctx->fields[j].fieldno == col_num) {
                    /* mapping found */
                    if (fieldno != j) {
                        db_fielddef_t t = ctx->fields[ fieldno ];
                        ctx->fields[ fieldno ] = ctx->fields[j];
                        ctx->fields[j] = t;
                    }
                    break;
                }
            }

            if (j == ctx->num_fields) {
                print_error_message("column %s mapped twice\n", data);
                return -1;
            }

            return 0;
        }
        }
    }

    if( !ctx->failed && ctx->num_fields > fieldno ) {
        db_result_t res = DB_OK;
        if (len == 0 && (ctx->fields[ fieldno ].field_flags & DB_NULL_MASK) == DB_NULLABLE) {
            res = db_set_null(ctx->hrow, ctx->fields[ fieldno ].fieldno);
            fprintf( stdout, "line.field: %d.%d = <null>\n", lineno, fieldno );
        } else {
            switch((intptr_t)ctx->fields[ fieldno ].field_type) {
            case (intptr_t)DB_COLTYPE_UTF8STR:
            case (intptr_t)DB_COLTYPE_UTF16STR:
            case (intptr_t)DB_COLTYPE_UTF32STR:
                res = db_set_field_data( ctx->hrow, ctx->fields[ fieldno ].fieldno,
                                         DB_VARTYPE_UTF8STR, data, len);
                break;
            default:
                res = db_set_field_data( ctx->hrow, ctx->fields[ fieldno ].fieldno,
                                         DB_VARTYPE_ANSISTR, data, len);
            }
            //fprintf( stdout, "line.field: %d.%d = [%s]\n", lineno, fieldno, data );
        }
        if (res == DB_FAIL) {
            print_error_message("while importing into column '%s', line %d. Input pos: %d(b)",
                                ctx->fields[ fieldno ].field_name, lineno, in_csv_pos );
            ctx->failed = 1;
        }
        if (ctx->failed ) {
            fprintf( stderr, "Line %d. %s\n", lineno,
                     ctx->ioptions.commit_mode == LINE_COMMIT ? "Skip whole line" : "Cancel processing"
                     );
        }
    }
    return 0;
}

/// Insert prepared row (db_context->hrow) in DB table (db_context->tab)
static int got_line_cb( int lineno, int fields, size_t pos, void * db_context)
{
    int fieldno;
    db_context_t * ctx = (db_context_t *)db_context;

    if( lineno == 0 && ( USE_HEADER == ctx->ioptions.header_mode || IGNORE_HEADER == ctx->ioptions.header_mode ) ) {
        return 0;
    }

    if( fields ) {
        if( !ctx->failed && !ctx->have_tx ) {
            if ( db_begin_tx( ctx->hdb, 0) == DB_FAIL ) {
                print_error_message("unable to start transaction for importing line %d", lineno);
                ctx->failed = 1;
            }
            else {
                ctx->have_tx = 1;
            }
        }
        if( !ctx->failed ) {
            if (db_insert(ctx->tab, ctx->hrow, NULL, 0) == DB_FAIL) {
                print_error_message("while importing line %d", lineno);
                ctx->failed = 1;
            }
        }
    }

    if (ctx->ioptions.commit_mode == LINE_COMMIT) {

        if (ctx->have_tx) {
            if(ctx->failed) {
                db_abort_tx( ctx->hdb, 0 );
            } else if (db_commit_tx( ctx->hdb, 0 ) == DB_FAIL) {
                print_error_message("unable to finalize transaction for importing line %d", lineno);
            }
            ctx->have_tx = 0;
        }
        /* next line might be more successful */
        ctx->failed = 0;
    } else if (ctx->failed) {
        return -1;
    }

    /* prepare the next row */
    for ( fieldno = 0; fieldno < ctx->num_fields; fieldno++ ) {
        db_set_null(ctx->hrow, ctx->fields[fieldno].fieldno);
    }
    return 0;
}

static void parse_input(
    read_more_data_callback_t cb, void *cb_data,
    got_field_callback_t got_field_cb, got_line_callback_t got_line_cb,
    void *db_cb_data
    )
{
    typedef enum {
        SIMPLE_ACCUM, QUOTED_ACCUM, CHECK_EOL, CHECK_QUOTE
    } parse_state_t;

    parse_state_t parse_state = SIMPLE_ACCUM;

    size_t hw_pos = 0;
    int lines = 0;
    int fieldno = 0;
    int eof = 0;
    char * ch = 0;
    char * ep = 0;
    char * c_buffer = 0;
    size_t c_buffer_size = 5;
    size_t symb_len = 0;
    int EOL_LEN = strlen( EOL );
    int eol_idx = 0;
    int do_fetch;
    int got_field = 0;
    int got_line = 0;
    int line_len = 0;

    c_buffer = (char *)malloc(c_buffer_size);

    while( !eof ) {
        // Read more data from csv-source
        if( ep == ch && !eof ) {
            ptrdiff_t sz = (*cb)( &ch, cb_data );

            if( 0 > sz ) {
                fprintf( stderr, "Error to read csv data source. Position in source(byte): %d\n", hw_pos );
            }

            ep = ch + sz;
            eof = 0 >= sz;
        }

        //printf("Input to subcycle. ch=%c\n", *ch);
        while( !eof && ch != ep ) {
            do_fetch = 0;
            got_field = got_line = 0;
            //printf("ch=[%c]; parse_state: %d\n", *ch, parse_state);
            switch(parse_state) {
            case QUOTED_ACCUM:
                if( FIELD_QUOTE == *ch ) {
                    parse_state = CHECK_QUOTE;
                }
                else {
                    do_fetch = 1;
                }
                break;
            case CHECK_QUOTE:
                if( FIELD_QUOTE == *ch ) {
                    do_fetch = 1;
                    parse_state = QUOTED_ACCUM;
                    break;
                }
                else {
                    parse_state = SIMPLE_ACCUM;
                }
            // NO break INTENTIONALLY HERE;
            case SIMPLE_ACCUM:
                if( FIELD_DELIM == *ch ) {got_field = 1; }
                else if( FIELD_QUOTE == *ch ) {parse_state = QUOTED_ACCUM; }
                else if( EOL[0] == *ch ) {
                    if( EOL_LEN > 1 ) {
                        parse_state =  CHECK_EOL;
                        do_fetch = 1;
                        eol_idx = 0;
                    }
                    else {
                        got_field = 1;
                    }
                }
                else {
                    do_fetch = 1;
                }
                break;
            case CHECK_EOL:
                do_fetch = 1;
                if( *ch != EOL[ ++eol_idx ] ) {
                    parse_state = SIMPLE_ACCUM;
                } else if( EOL_LEN - 1 == eol_idx ) {
                    parse_state = SIMPLE_ACCUM;
                    got_field = got_line = 1;
                    do_fetch = -eol_idx;
                }
                break;
            }
            if( 0 < do_fetch ) {
                if( symb_len >= c_buffer_size ) {
                    c_buffer_size *= 2;
                    c_buffer = (char*)realloc(c_buffer, c_buffer_size);
                    if( NULL == c_buffer ) {
                        print_error_message("out of memory during import");
                        return;
                    }
                }
                c_buffer[ symb_len++ ] = *ch;
            } else if( 0 > do_fetch ) {
                symb_len += do_fetch;
                c_buffer[ symb_len ] = 0;
            }
            line_len += do_fetch;
            ++ch, ++hw_pos; //,++line_len;
            if( got_field || got_line ) {
                break;
            }
        }

        if( ( eof || got_field ) && line_len ) {
            c_buffer[symb_len] = 0;
            if( 0 != got_field_cb( lines, fieldno, hw_pos, c_buffer, symb_len, db_cb_data ) ) {
                break;
            }
            symb_len = 0;
            ++fieldno;
        }

        if( ( eof || got_line ) && line_len ) {
            line_len = 0;
            if( 0 != got_line_cb( lines, fieldno, hw_pos, db_cb_data ) ) {
                break;
            }
            ++lines;
            fieldno = 0;
        }
    }

    free(c_buffer);
}

static int
do_import( db_t hdb, const char *table_name, const csv_import_options_t * ioptions, read_more_data_callback_t cb, void *cb_data )
{
    typedef enum {
        SIMPLE_ACCUM, QUOTED_ACCUM, CHECK_EOL, CHECK_QUOTE
    } parse_state_t;

    db_context_t dbctx;
    memset( &dbctx, 0, sizeof( db_context_t ) );

    dbctx.hdb = hdb;
    dbctx.ioptions = *ioptions;

    if ((dbctx.tab = db_open_table_cursor(hdb, table_name, NULL)) != NULL) {

        dbctx.num_fields = db_get_field_count( dbctx.tab );
        dbctx.fields = (db_fielddef_t*) malloc( dbctx.num_fields * sizeof(db_fielddef_t) );

        if (dbctx.fields) {

            int fieldno;
            for (fieldno = 0; fieldno < dbctx.num_fields; fieldno++) {
                db_get_field( dbctx.tab, fieldno, &dbctx.fields[fieldno] );
            }
            fieldno = 0;

            dbctx.hrow = db_alloc_cursor_row( dbctx.tab );
            if (dbctx.hrow ) {
                parse_input( cb, cb_data, got_field_cb, got_line_cb, &dbctx );
                db_free_row( dbctx.hrow );
            }

            if (dbctx.have_tx) {
                db_abort_tx( dbctx.hdb, 0 );
            }

            free(dbctx.fields);
        }
        db_close_cursor( dbctx.tab );
    } else {
        print_error_message("unable to open table '%s'", table_name);
    }

    return dbctx.failed;
}

//----------------------- EXPORT
int export_data(db_t hdb, const char *table_name, const char * file_name, const csv_export_options_t * export_options )
{
    int rc = EXIT_FAILURE;
    db_cursor_t tab = NULL;
    db_row_t    row = NULL;
    int fieldno;
    int field_count;
    char field_buf[4096];
    int argno = 1;

    csv_export_options_t eoptions = { USE_HEADER, TABLE_SOURCE, '\'', 0, 0 };
    FILE * out_file = NULL;
    const char * sep = NULL;
    const char * begin;
    db_fielddef_t * fields = NULL;

    if( export_options ) {
        eoptions = *export_options;
    }

    if (table_name == NULL) {
        print_error_message("table name expected");
        return EXIT_FAILURE;
    }

    if (file_name == NULL) {
        out_file = stdout;
    }
    else {
        out_file = fopen(file_name, "wt");
        if (out_file == NULL) {
            print_error_message("unable to open file '%s': %s", file_name, strerror(errno));
            goto cleanup;
        }
    }

    if (eoptions.source_mode == SQL_SOURCE) {

        /* todo: SQL_SOURCE needs some way to make an argument with spaces.
         * fix extract_arg() to handle quotes
         */
        print_error_message("SQL mode is not supported yet");
        goto cleanup;

        tab = db_prepare_sql_cursor(hdb, table_name, 0);
        if (tab == NULL) {
            print_error_message("unable to prepare SQL statement '%s'", table_name);
            goto cleanup;
        }

        /* todo: support parameters */
        if (db_execute( tab, NULL, 0 ) == DB_FAIL) {
            print_error_message("unable to execute SQL statement '%s'", table_name);
            goto cleanup;
        }
    } else {

        tab = db_open_table_cursor(hdb, table_name, NULL);
        if (tab == NULL) {
            print_error_message("unable to open table '%s'", table_name);
            goto cleanup;
        }
    }
    row = db_alloc_cursor_row( tab );
    if (row == NULL) {
        print_error_message("unable to allocate row");
        goto cleanup;
    }

    field_count = db_get_field_count( tab );
    fields = (db_fielddef_t*) malloc( sizeof(db_fielddef_t) * field_count);
    if (fields == NULL) {
        print_error_message("out of memory");
        goto cleanup;
    }

    for (fieldno = 0; fieldno < field_count; fieldno++) {
        db_fielddef_init(&fields[fieldno]);
        db_get_field( tab, fieldno, &fields[fieldno]);
    }

    sep = ",";

    /* export header */
    if (eoptions.header_mode == USE_HEADER) {

        for (fieldno = 0; fieldno < field_count; fieldno++) {
            if (fieldno > 0) {
                fputs(sep, out_file);
            }

            fputs(fields[fieldno].field_name, out_file);
        }

        fputc('\n', out_file);
    }

    /* export data */
    if (db_seek_first( tab ) == DB_FAIL) {
        print_error_message("unable to read table");
        goto cleanup;
    }

    while (1) {
        int rc = db_eof(tab);

        if (rc < 0) {
            print_error_message("unable to read table");
            goto cleanup;
        }

        if (rc) {
            break;
        }

        if (db_fetch( tab, row, NULL ) == DB_FAIL) {
            print_error_message("unable to read table");
            goto cleanup;
        }

        if( export_options && export_options->line_prefix && export_options->line_prefix[0] ) {
            fprintf( out_file, "%s", export_options->line_prefix );
        }
        for (fieldno = 0; fieldno < field_count; fieldno++) {
            db_len_t len;


            if (db_is_null(row, fieldno)) {
                field_buf[0] = 0;
                len = DB_FIELD_NULL;
            } else {
                switch((intptr_t)fields[fieldno].field_type) {
                case (intptr_t)DB_COLTYPE_UTF8STR:
                case (intptr_t)DB_COLTYPE_UTF16STR:
                case (intptr_t)DB_COLTYPE_UTF32STR:
                    len = db_get_field_data(row, fieldno, DB_VARTYPE_UTF8STR, &field_buf, sizeof(field_buf));
                    break;
                default:
                    len = db_get_field_data(row, fieldno, DB_VARTYPE_ANSISTR, &field_buf, sizeof(field_buf));
                }

                if (len == DB_LEN_FAIL) {
                    print_error_message("unable to get field %d data", fieldno);
                    goto cleanup;
                }
            }

            if (fieldno > 0) {
                fputs(sep, out_file);
            }

            fputc(eoptions.field_quote, out_file);

            begin = field_buf;
            while(1) {
                const char * e = strchr(begin, eoptions.field_quote);

                if (e == NULL) {
                    fwrite(begin, 1, strlen(begin), out_file);
                    break;
                } else {
                    fwrite(begin, 1, e - begin + 1, out_file);
                    fputc(eoptions.field_quote, out_file);
                    begin = e + 1;
                }
            }
            fputc(eoptions.field_quote, out_file);
        }

        if( export_options && export_options->line_suffix && export_options->line_suffix[0] ) {
            fprintf( out_file, "%s", export_options->line_suffix );
        }
        fputs(EOL, out_file);

        if (db_seek_next(tab) == DB_FAIL) {
            print_error_message("unable to read table");
            goto cleanup;
        }
    }
    rc = EXIT_SUCCESS;

cleanup:

    if (fields) {
        free(fields);
    }

    if (row) {
        db_free_row( row );
    }

    if (tab) {
        db_close_cursor( tab );
    }

    if (out_file && out_file != stdout) {
        fclose(out_file);
    }

    return rc;
}
