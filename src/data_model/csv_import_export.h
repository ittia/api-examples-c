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

#ifndef CSV_IMPORT_EXPORT_H_INCLUDED
#define CSV_IMPORT_EXPORT_H_INCLUDED

#include <ittia/db.h>

typedef enum {
    LINE_COMMIT,
    FILE_COMMIT,
} csv_commit_mode_t;

typedef enum {
    NO_HEADER, IGNORE_HEADER, USE_HEADER
} csv_header_mode_t;

typedef struct {
    csv_commit_mode_t commit_mode;
    csv_header_mode_t header_mode;
} csv_import_options_t;

int csv_import( db_t hdb, const char * table_name, const char * buffer, size_t buffer_size, csv_import_options_t * import_options );

typedef enum {
    SQL_SOURCE, TABLE_SOURCE
} csv_source_mode_t;

typedef struct {
    csv_header_mode_t header_mode;
    csv_source_mode_t source_mode;
    char field_quote;
    char * line_prefix;
    char * line_suffix;
} csv_export_options_t;

int export_data(db_t hdb, const char *table_name, const char * file_name, const csv_export_options_t * export_options );

#endif
