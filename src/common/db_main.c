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

#include <ittia/db.h>
#include <stdlib.h>
#include <stdio.h>

int example_main(int argc, char* argv[]);

int db_main(int argc, char* argv[])
{
    int exit_code;
    int status;

    /* Initialize ITTIA DB SQL library. */
    status = db_init_ex(DB_API_VER, NULL);

    if (DB_SUCCESS(status)) {
        db_api_statistics_t api_stats;
        db_lm_statistics_t lm_stats;
        db_done_t done = { NULL, &api_stats, &lm_stats };

#ifdef _DEBUG
        /* Enable collection of statistics. */
        db_api_statistics(NULL, DB_STATISTICS_ENABLE);
        db_lm_statistics(NULL, DB_STATISTICS_ENABLE);
#endif

        /* Run the example application. */
        exit_code = example_main(argc, argv);

        /* Release all resources owned by the ITTIA DB SQL library. */
        status = db_done_ex(&done);

        if (DB_SUCCESS(status)) {
            if (api_stats.have_statistics) {
                printf("ITTIA DB SQL final C API resource statistics:\n");
                printf("  db:       %ld remain open of %ld max\n", (long)api_stats.db.cur_value, (long)api_stats.db.max_value);
                printf("  row:      %ld remain open of %ld max\n", (long)api_stats.row.cur_value, (long)api_stats.row.max_value);
                printf("  cursor:   %ld remain open of %ld max\n", (long)api_stats.cursor.cur_value, (long)api_stats.cursor.max_value);
                printf("  seq:      %ld remain open of %ld max\n", (long)api_stats.seq.cur_value, (long)api_stats.seq.max_value);
                printf("  seqdef:   %ld remain open of %ld max\n", (long)api_stats.seqdef.cur_value, (long)api_stats.seqdef.max_value);
                printf("  tabledef: %ld remain open of %ld max\n", (long)api_stats.tabledef.cur_value, (long)api_stats.tabledef.max_value);
                printf("  indexdef: %ld remain open of %ld max\n", (long)api_stats.indexdef.cur_value, (long)api_stats.indexdef.max_value);
                printf("  oid:      %ld remain open of %ld max\n", (long)api_stats.oid.cur_value, (long)api_stats.oid.max_value);
            }

            if (lm_stats.have_statistics) {
                printf("ITTIA DB SQL final lock manager resource statistics:\n");
                printf("  nlocks:   %ld remain open of %ld max\n", (long)lm_stats.nlocks.cur_value, (long)lm_stats.nlocks.max_value);
                printf("  nowners:  %ld remain open of %ld max\n", (long)lm_stats.nowners.cur_value, (long)lm_stats.nowners.max_value);
                printf("  nobjects: %ld remain open of %ld max\n", (long)lm_stats.nobjects.cur_value, (long)lm_stats.nobjects.max_value);
            }
        }
    }

    if (DB_FAILED(status)) {
        printf("ITTIA DB SQL init/done error: %d\n", status);
        exit_code = EXIT_FAILURE;
    }

    return exit_code;
}
