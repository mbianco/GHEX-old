#include <transport_layer/mpi/communicator.hpp>
#include <allocator/persistent_allocator.hpp>
#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

const int SIZE = 4000000;
int mpi_rank;
gridtools::mpi::communicator comm;

/** use an allocator that keeps the allocations for future use */
typedef ghex::allocator::persistent_allocator<unsigned char, std::allocator<unsigned char>> t_allocator;

/** this needs to be thread-private, but can't be declared so?
 *  The below seems to be a hack.
 */
extern t_allocator allocator;
#pragma omp threadprivate(allocator)
t_allocator allocator;

bool comm_ready = false;

struct send_callback {
    gridtools::mpi::shared_message<t_allocator> msg;

    send_callback(gridtools::mpi::shared_message<t_allocator> m) : msg{m} {}

    void operator()(int, int) {

        /** Can we change the above message argument to &?
	    If yes, then use_count()==1 would indicate completion of the last comm request.
        */
        if(1 == msg.use_count())
            comm_ready = true;
	// std::cerr << mpi_rank  << ": sent to " << rank << " use_count " << msg.use_count() << "\n";
    }
};

void submit_sends() {

    gridtools::mpi::shared_message<t_allocator> smsg{SIZE, allocator};
    unsigned char *buffer;

    /* fill the buffer directly (memcpy) */
    buffer = smsg.data();
    for (int i = 0; i < SIZE; ++i) {
	buffer[i] = i%256;
    }
    smsg.resize(SIZE);
    std::array<int, 3> dsts = {1,2,3};

    /** two options: with, or without callback on send completion */
    if(true){

	/* send message to neighbors: with completion callback */
	comm.send_multi(smsg, dsts, 42, send_callback{smsg}); // This line is the same as
	// comm.send_multi(smsg, dsts, 42, [smsg](int, int) {
	// 	if(1 == smsg.use_count())
	// 	    comm_ready = true;
	//     });

    } else {

	/** we don't care about the send completion: mark comm as ready */
	comm.send_multi(smsg, dsts, 42);
	comm_ready = true;
    }
    // std::cerr << "initial smsg.use_count " << smsg.use_count() << "\n" ;
}

struct recv_callback {
    gridtools::mpi::shared_message<t_allocator> msg;
    
    recv_callback(gridtools::mpi::shared_message<t_allocator> m) : msg{m} {}

    void operator()(int, int) {
	comm_ready = true;
	// std::cerr << mpi_rank  << ": received from " << rank << " use_count " << msg.use_count() << "\n";
    }
};

void submit_recvs() {
    gridtools::mpi::shared_message<t_allocator> rmsg{SIZE, SIZE, allocator};
    comm.recv(rmsg, 0, 42, recv_callback{rmsg});
    // std::cerr << "initial rmsg.use_count " << rmsg.use_count() << "\n" ;
}

TEST(transport, send_multi) {

    {
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        EXPECT_EQ(size, 4);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Barrier(MPI_COMM_WORLD);

    /** General DISPATCH scheduler loop:
	1. compute on a patch (progress it in time)
	2. send the progressed patch to the neighbors that require it
	3. progress the communication (this can - in the callbacks - make new patches ready for execution)
    */

    // while(!end_of_simulation){

    /** pick a ready patch an compute on it */
    /* [...] */

    /** submit the communication requests */
    if (0 == mpi_rank) {
	submit_sends();
    } else {
	submit_recvs();
    }

    /** progress any of the pending communication requests:
	might progress any comm of any patch, not necessarily
	the recently posted requests. NOTE: Need some loop here,
	but not in the real DISPATCH scheduler.
    */
    while(!comm_ready) comm.progress();

    // } end of while(!end_of_simulation)

    /** make sure all comm has actually been completed before exiting MPI.
	Something like this needs to be done in comm destructor,
	otherwise MPI aborts with incomplete comm errors.
    */
    while(comm.progress());
    MPI_Barrier(MPI_COMM_WORLD);

    EXPECT_TRUE(comm_ready);
}
