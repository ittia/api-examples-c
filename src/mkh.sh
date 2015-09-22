#!/bin/bash

exit 1

ROOT_BKL=console_examples.bkl
> $ROOT_BKL

function tune_app()
{
	app=$1
	subj=$2
	echo "Tuning APP: $subj/$app"
	cat >> $subj/${subj}.bkl <<EOF 
program ${app}_c
	: api_ittia_db_c
{
	sources { ${app}.c }
}


EOF

	cat > $subj/$app.c <<EOF

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

/** @file ${app}.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

//#include "dbs_schema.h"
//#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv) 
{
    return 0;
}

EOF
	
}

subj=""
app=""

echo "
 1. file_storage
 a) atomic_file_storage
 b) bulk_import
 c) background_commit
 2. memory_storage 
 a) full_memory_storage
 b) memory_disk_hybrid
 3. error_handling 
 a) transaction_rollback
 b) savepoint_rollback
 4. data_model 
 a) schema_upgrade
 b) foreign_key_constraints
 c) unicode_character_strings
 d) datetime_intervals
 e) text_import
 f) text_export
 g) sql_export
 5. security 
 a) storage_encryption
 b) connection_authentication
 6. shared_access 
 a) concurrent_insert
 b) concurrent_read_write
 c) embedded_database_server
 d) online_backup
 e) data_change_notification
 7. sql 
 a) sql_select_query
 b) sql_parameters
 c) sql_explain_query_plan
" | grep -E '.+' |\
while read l; do
	[ -z "$l" ] && continue
#	echo $l
	subj_tmp=`echo $l | awk '/[0-9]+\. / { print $2 }'`
	app=`echo $l | awk '/[a-z]+\) / { print $2 }'`
	if [ -z "$subj_tmp" ] ; then
		tune_app $app $subj
	else
		echo "Subject: $subj_tmp"
		subj=$subj_tmp
		mkdir -p $subj
		echo "submodule $subj/${subj}.bkl;" >> $ROOT_BKL
		> ${subj}/${subj}.bkl
	fi
done