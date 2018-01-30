/* Theory: By default, MVAPICH2 uses RDMA write for point-to-point data transfer using the rendezvous protocol.
 * However, when it happens that the sending side uses non-blocking semantics and the receiving side uses 
 * blocking semantics, this can be sub-optimal overall. Consider the following scenario:
 * a. Sending side sends and RNDV_START and moves ahead with computation
 * b. Receiving side is blocking, so it recieves the RNDV_START, and sends an RNDV_REPLY okaying the data transfer
 * c. Since this is an RDMA_WRITE for the sending side, actual data transfer may not begin until the sending side reaches
 *    MPI_Wait(), since the sending side needs to be "inside" MPI in order for communications to progress in the absence of a separate thread
 * d. Thus, data transfer starts only when the sending side reaches MPI_Wait(), and this is serializing communication and computation from the sending
 *    viewpoint.
 * e. In such a situation of non-blocking sender and blocking receiver without a separate thread, it is easy to show that using an RDMA_READ from the receiving side is
 *    optimal or near-optimal. Essentially we take advantage of the fact that the receiver has "nothing" to do when blocking on a receive, and might as well progress communication
 *
 *    In order to see this behaviour, set MV2_RNDV_PROTOCOL=RGET (default is RPUT), and see the reduction in overall runtime. This is attributed to better sender side overlap.
 */
#include<stdio.h>
#include<mpi.h>

#define DIM 25
static float **a = NULL, *x = NULL, *y = NULL;

void
init_arrays()
{
    int i = 0, j = 0;

    if (a == NULL) {
        a = malloc(DIM * sizeof(float *));
        for (i = 0; i < DIM; i++) {
            a[i] = malloc(DIM * sizeof(float));
        }
    }

    if (x == NULL) {
        x = malloc(DIM * sizeof(float));
    }
    if (y == NULL) {
        y = malloc(DIM * sizeof(float));
    }

    for (i = 0; i < DIM; i++) {
        x[i] = y[i] = 1.0f;
        for (j = 0; j < DIM; j++) {
            a[i][j] = 2.0f;
        }
    }
}

void
compute_on_host()
{
    int i = 0, j = 0;
    for (i = 0; i < DIM; i++)
        for (j = 0; j < DIM; j++)
            x[i] = x[i] + a[i][j]*a[j][i] + y[j];
}

static inline void
compute(double target_seconds)
{
    double t1 = 0.0, t2 = 0.0;
    double time_elapsed = 0.0;
    while (time_elapsed < target_seconds) {
        t1 = MPI_Wtime();
        compute_on_host();
        t2 = MPI_Wtime();
        time_elapsed += (t2-t1);
    }
}

main(int argc, char **argv) {

	int my_rank, num, i;
	int *buffer = NULL;
	MPI_Request req;
	MPI_Status stat;
	int number = 10;
	double starttime, endtime;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num);
	if(num != 2) {
		printf("Example must be run with 2 processes only.\n");
		return 0;
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	buffer = (int*)malloc(100000000*sizeof(int));
	for(i=0; i < 1000; i++)
		buffer[i] = 0;

	init_arrays();

	starttime = MPI_Wtime();

	for(i=0; i < 30; i++) {

	if(my_rank == 1) {
	//Send
		printf("Starting send...\n");
		MPI_Isend(buffer, 100000000, MPI_INT, 0, 123, MPI_COMM_WORLD, &req);
		compute(0.06);
		MPI_Wait(&req, &stat);
	} else if(my_rank == 0) {
	//Recv
		MPI_Recv(buffer, 100000000, MPI_INT, 1, 123, MPI_COMM_WORLD, &stat);
		printf("Recieved message in iteration: %d\n", i);
	}

		MPI_Barrier(MPI_COMM_WORLD);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	endtime = MPI_Wtime();

	if(my_rank == 0) printf("Done in %f seconds.\n", endtime - starttime);

	free(buffer);

	MPI_Finalize(); 
}
