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

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num);
	if(num != 2) {
		printf("Example must be run with 2 processes only.\n");
		return 0;
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	buffer = (int*)malloc(1000*sizeof(int));
	for(i=0; i < 1000; i++)
		buffer[i] = 0;

	init_arrays();

	if(my_rank == 0) {
	//Send
		printf("Starting send...\n");
		MPI_Isend(buffer, 1000, MPI_INT, 1, 123, MPI_COMM_WORLD, &req);
		compute(5.0);
		MPI_Wait(&req, &stat);
	} else if(my_rank == 1) {
	//Recv
		MPI_Recv(buffer, 1000, MPI_INT, 0, 123, MPI_COMM_WORLD, &stat);
		printf("Recieved message.\n");
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(my_rank == 0) printf("Done.\n");

	free(buffer);

	MPI_Finalize(); 
}
