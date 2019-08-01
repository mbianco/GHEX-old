#include <transport_layer/mpi/communicator.hpp>
#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

const int SIZE = 4000000;
int mpi_rank;
mpi::communicator comm;

bool comm_ready = false;

void submit_sends() {

    mpi::shared_message<> smsg{SIZE};
    unsigned char *buffer;

    /* fill the buffer directly (memcpy) */
    buffer = smsg.data();
    for (int i = 0; i < SIZE; ++i) {
	buffer[i] = i%256;
    }
    smsg.set_size(SIZE);

    /* send message to neighbors */
    std::array<int, 3> dsts = {1,2,3};
    comm.send_multi(smsg, dsts, 42);
    // std::cerr << "smsg.use_count " << smsg.use_count() << "\n" ;

    /** we don't care about the send completion: mark comm as ready */
    comm_ready = true;
}

void recv_callback(int rank, int tag, mpi::shared_message<> rmsg) {
    comm_ready = true;
    // std::cout << mpi_rank  << ": received from " << rank << " size " << rmsg.size() << "\n";
}

void submit_recvs() {
    mpi::shared_message<> rmsg{SIZE, SIZE};
    comm.recv(rmsg, 0, 42, recv_callback);
    // std::cerr << "rmsg.use_count " << rmsg.use_count() << "\n" ;
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
    if (mpi_rank == 0) {
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
