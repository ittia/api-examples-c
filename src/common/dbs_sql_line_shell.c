/**************************************************************************/
/*                                                                        */
/*      Copyright (c) 2005-2019 by ITTIA L.L.C. All rights reserved.      */
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

#include "dbs_sql_line_shell.h"

#include <stdlib.h>
#include <string.h>

static void grow_buffer(char ** buffer, size_t * size, size_t requested_size);

/// Execute a SQL query for each line of input
void
dbs_sql_line_shell(db_t database, const char * prompt, FILE * in, FILE * out, FILE * err)
{
    char * buffer;
    size_t buffer_size = 128;

    buffer = (char *)malloc(buffer_size);

    while (!feof(in) && !ferror(in)) {
        size_t statement_length = 0;
        db_cursor_t sql_cursor;

        fprintf(out, "%s> ", prompt);
        fflush(out);

        // Read line in
        do {
            grow_buffer(&buffer, &buffer_size, buffer_size + statement_length);
            if (buffer == NULL) {
                fprintf(err, "Out of memory for SQL statement\n");
                return;
            }

            fgets(buffer + statement_length, (int)(buffer_size - statement_length), in);
            statement_length += strlen(buffer + statement_length);
        } while (statement_length > 0 && buffer[statement_length - 1] != '\n');

        // Continue until a blank line is entered
        if (buffer[0] == '\n') {
            db_commit_tx(database, 0);
            break;
        }

        // Remove trailing semicolon, if present
        if (statement_length >= 2 && buffer[statement_length - 2] == ';') {
            buffer[statement_length - 2] = '\0';
        }

        // Prepare the SQL statement
        sql_cursor = db_prepare_sql_cursor(database, buffer, DB_CURSOR_ENCODING_UTF8);

        if (db_is_prepared(sql_cursor) > 0) {
            int param_count = db_get_param_count(sql_cursor);
            db_row_t param_row;

            if (param_count > 0) {
                db_fieldno_t paramno;
                param_row = db_alloc_param_row(sql_cursor);

                for (paramno = 0; paramno < param_count; ++paramno) {
                    size_t param_length = 0;

                    fprintf(out, "param[%d]=", (int)paramno);
                    fflush(out);

                    // Read line in
                    do {
                        grow_buffer(&buffer, &buffer_size, buffer_size + param_length);
                        if (buffer == NULL) {
                            fprintf(err, "Out of memory for SQL parameter\n");
                            return;
                        }

                        fgets(buffer + param_length, (int)(buffer_size - param_length), in);
                        param_length += strlen(buffer + param_length);
                    } while (param_length > 0 && buffer[param_length - 1] != '\n');

                    if (param_length > 0) {
                        if (DB_OK != db_set_field_data(param_row, paramno, DB_VARTYPE_UTF8STR, buffer, (db_len_t)param_length - 1)) {
                            fprintf(err, "Type conversion error: %d\n", get_db_error());
                        }
                    }
                }
            }
            else {
                param_row = NULL;
            }

            // Execute the SQL statement
            if (DB_OK == db_execute(sql_cursor, param_row, NULL)) {
                if (db_is_browsable(sql_cursor) > 0) {
                    db_fieldno_t fieldno;
                    db_fieldno_t field_count;
                    db_fielddef_t fielddef;
                    db_row_t row;

                    // Print list of fields in the result set
                    field_count = db_get_field_count(sql_cursor);
                    db_fielddef_init(&fielddef);
                    for (fieldno = 0; fieldno < field_count; ++fieldno) {
                        if (fieldno > 0) {
                            fprintf(out, ", ");
                        }
                        if (DB_OK == db_get_field(sql_cursor, fieldno, &fielddef)) {
                            fprintf(out, "%s", fielddef.field_name);
                        }
                    }
                    db_fielddef_destroy(&fielddef);
                    fprintf(out, "\n");

                    // Print result set
                    row = db_alloc_cursor_row(sql_cursor);
                    if (row != NULL) {
                        for (db_seek_first(sql_cursor); !db_eof(sql_cursor); db_seek_next(sql_cursor)) {
                            // Print the current row
                            if (DB_OK == db_fetch(sql_cursor, row, NULL)) {
                                for (fieldno = 0; fieldno < field_count; ++fieldno) {
                                    if (fieldno > 0) {
                                        fprintf(out, ", ");
                                    }
                                    db_len_t len;

                                    // Grow buffer, if required
                                    len = db_get_field_data(row, fieldno, DB_VARTYPE_UTF8STR, NULL, 0);

                                    if (len > 0) {
                                        grow_buffer(&buffer, &buffer_size, (size_t)len + 1);
                                        if (buffer == NULL) {
                                            fprintf(err, "Out of memory for field\n");
                                            return;
                                        }

                                        // Print field as a string
                                        len = db_get_field_data(row, fieldno, DB_VARTYPE_UTF8STR, buffer, (db_len_t)buffer_size);
                                        if (len > 0) {
                                            fprintf(out, "%*s", (int)len, buffer);
                                        }
                                    }

                                    if (len == DB_FIELD_NULL) {
                                        fprintf(out, "null");
                                    }
                                    else if (len == DB_LEN_FAIL) {
                                        fprintf(err, "Type conversion error: %d\n", get_db_error());
                                    }
                                }
                                fprintf(out, "\n");
                            }
                            else {
                                fprintf(err, "Fetch error: %d\n", get_db_error());
                            }
                        }
                        db_free_row(row);
                    }
                }
                else {
                    int32_t modified_rows;
                    if (DB_OK == db_get_row_count_ex(sql_cursor, &modified_rows) && modified_rows >= 0) {
                        fprintf(out, "%ld rows modified\n", (long int)modified_rows);
                    }
                }
            }
            else {
                fprintf(err, "Execute error: %d\n", get_db_error());
            }

            db_free_row(param_row);
        }
        else {
            fprintf(err, "Prepare error: %d\n", get_db_error());
        }

        db_close_cursor(sql_cursor);
    }

    free(buffer);
}

void grow_buffer(char ** buffer, size_t * size, size_t requested_size)
{
    if (requested_size > *size) {
        char * buffer_grown;
        buffer_grown = realloc(*buffer, requested_size);

        if (buffer_grown != NULL) {
            *buffer = buffer_grown;
            *size = requested_size;
        }
        else {
            free(*buffer);
            *buffer = NULL;
            *size = 0;
        }
    }
}

