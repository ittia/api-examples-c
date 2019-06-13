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

#ifndef THREAD_UTILS_INCLUDED
#define THREAD_UTILS_INCLUDED

#include <ittia/os/os_thread.h>

typedef void (*thread_proc_t)(void*);

/* thread flags */
#define THREAD_JOINABLE 1  /**< #os_thread_join can be used to wait for the
                                   thread to complete. */
#define THREAD_DETACHED 0  /**< Another thread cannot wait for thread to
                                   complete. */

int thread_spawn(thread_proc_t proc, void * arg, int flags, os_thread_t ** h);
int thread_join(os_thread_t * h);

struct os_mutex_t;
typedef struct {
    struct os_mutex_t * mutex;
} mutex_t;

int mutex_init(mutex_t * mutex);
int mutex_destroy(mutex_t * mutex);
int mutex_trylock(mutex_t * mutex);
int mutex_lock(mutex_t * mutex);
int mutex_unlock(mutex_t * mutex);

#endif //THREAD_UTILS_INCLUDED
