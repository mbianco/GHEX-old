#ifndef _THREADS_HPP
#define _THREADS_HPP

#include <omp.h>

#define DO_PRAGMA(x) _Pragma(#x)

#define GET_THREAD_NUM()  omp_get_thread_num()
#define GET_NUM_THREADS() omp_get_num_threads()
#define IN_PARALLEL()     omp_in_parallel()

#define DECLARE_THREAD_PRIVATE(names) DO_PRAGMA(omp threadprivate(names))

#endif /* _THREADS_HPP */
