#include <transport_layer/mpi/communicator.hpp>
#include <iostream>
#include <iomanip>

#include <gtest/gtest.h>

const int SIZE = 4000000;
int mpi_rank;
mpi::communicator comm;

bool comm_ready = false;

struct send_callback {
    mpi::shared_message<> smsg;

    send_callback(mpi::shared_message<> m) : smsg{m} {}

    void operator()(int, int) {

        /** Can we change the above message argument to &?
        If yes, then use_count()==1 would indicate completion of the last comm request.
        */
        if(2 == smsg.use_count())
            comm_ready = true;
        // std::cerr << mpi_rank  << ": sent to " << rank << " use_count " << smsg.use_count() << "\n";
    }
};

void submit_sends() {

    mpi::shared_message<> smsg{SIZE};
    unsigned char *buffer;

    /* fill the buffer directly (memcpy) */
    buffer = smsg.data();
    for (int i = 0; i < SIZE; ++i) {
	buffer[i] = i%256;
    }
    smsg.set_size(SIZE);
    std::array<int, 3> dsts = {1,2,3};

    /** two options: with, or without callback on send completion */
    if(true){

	/* send message to neighbors: with completion callback */
	comm.send_multi(smsg, dsts, 42, send_callback{smsg}); // This line is the same as
//	comm.send_multi(smsg, dsts, 42, [smsg](int, int) {
//            if(2 == smsg.use_count())
//            comm_ready = true;
//            });

    } else {

	/** we don't care about the send completion: mark comm as ready */
	comm.send_multi(smsg, dsts, 42);
	comm_ready = true;
    }
    // std::cerr << "initial smsg.use_count " << smsg.use_count() << "\n" ;
}

void recv_callback(int, int) {

    /** Can we change the above message argument to &?
	If yes, then use_count()==1 would indicate completion of the last comm request.
     */
    comm_ready = true;
    // std::cerr << mpi_rank  << ": received from " << rank << " use_count " << rmsg.use_count() << "\n";
}

void submit_recvs() {
    mpi::shared_message<> rmsg{SIZE, SIZE};
    comm.recv(rmsg, 0, 42, recv_callback); // Similar trick with a callable/lambda can work on receives
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
