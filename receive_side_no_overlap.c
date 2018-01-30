/* Theory: By default, MVAPICH2 uses RDMA write for point-to-point data transfer using the rendezvous protocol.
 * This situation is optimal when considering the scenario below:
 * a. Sending side uses blocking semantics, and receiving side uses non-blocking semantics
 * b. Reciever reaches the MPI_Irecv call first, and carries on with computation as it hasn't received any RNDV_START message yet
 * c. Sender blocks on the MPI_Send call, and waits till the receiver reaches the MPI_Wait() call, and begins data transfer. Essentially,
 *    communication and computation are serialized.
 * 
 * When using RDMA read for data trasfer, we still see that there is no overlap on the receiver side. This is to be expected if MVAPICH2 does not generate interrupts on the
 * receiving side on the receipt of the RNDV_START message. TODO: Ask MVAPICH2 team why interrupts are not generated, even though a lot of their past papers suggest this to be the case.
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
		//Some dummy computation on the sending to ensure that the receiver reaches the MPI_Irecv call first
		compute(0.02);
		MPI_Send(buffer, 100000000, MPI_INT, 0, 123, MPI_COMM_WORLD);
	} else if(my_rank == 0) {
	//Recv
		MPI_Irecv(buffer, 100000000, MPI_INT, 1, 123, MPI_COMM_WORLD, &req);
		compute(0.06);
		MPI_Wait(&req, &stat);
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
