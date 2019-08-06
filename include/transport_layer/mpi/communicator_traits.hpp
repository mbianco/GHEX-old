#ifndef GHEX_COMMUNICATOR_TRAITS_HPP
#define GHEX_COMMUNICATOR_TRAITS_HPP

#include <mpi.h>

namespace gridtools {
    namespace mpi {

        class communicator_traits {
            MPI_Comm m_comm;

        public:
            communicator_traits(MPI_Comm comm)
            : m_comm{comm}
            {}

            communicator_traits()
            : m_comm{MPI_COMM_WORLD}
            {}

            MPI_Comm communicator() const { return m_comm; }
        };

    } // namespace mpi
} // namespace gridtools

#endif