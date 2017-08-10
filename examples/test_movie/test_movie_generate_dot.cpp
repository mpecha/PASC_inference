/** @file test_movie_generate_dot.cpp
 *  @brief generate testing movie sample with dot
 *
 *  @author Lukas Pospisil
 */

#include "pascinference.h"

#include <random> /* random number generator */

#ifndef USE_PETSC
 #error 'This example is for PETSC'
#endif

#define DEFAULT_WIDTH 30
#define DEFAULT_HEIGHT 20
#define DEFAULT_T 10
#define DEFAULT_DATA_TYPE 1
#define DEFAULT_K 2
#define DEFAULT_XDIM 1
#define DEFAULT_NOISE 0.1

#define DEFAULT_SPEED_X 2.0
#define DEFAULT_SPEED_Y 1.5

#define DEFAULT_SAMPLE_TYPE 0

#define DEFAULT_FILENAME_DATA "data/test_movie/dotmovie.bin"
#define DEFAULT_FILENAME_SOLUTION "data/test_movie/dotmovie_solution.bin"
#define DEFAULT_FILENAME_GAMMA0 "data/test_movie/dotmovie_gamma0.bin"
#define DEFAULT_GENERATE_DATA true
#define DEFAULT_GENERATE_GAMMA0 true

#define DEFAULT_GENERATE_MU0_XDIM1 "0.6"
#define DEFAULT_GENERATE_MU1_XDIM1 "0.5"

#define DEFAULT_GENERATE_MU0_XDIM3 "1.0,1.0,1.0"
#define DEFAULT_GENERATE_MU1_XDIM3 "1.0.0.0,0.0"


using namespace pascinference;

int main( int argc, char *argv[] )
{
	/* add local program options */
	boost::program_options::options_description opt_problem("PROBLEM EXAMPLE", consoleArg.get_console_nmb_cols());
	opt_problem.add_options()
		("test_sample_type", boost::program_options::value< int >(), "type of generated sample [0=stationary, 1=moving, 2=bouncing]")
		("test_filename_data", boost::program_options::value< std::string >(), "name of output file with movie data (vector in PETSc format) [string]")
		("test_filename_solution", boost::program_options::value< std::string >(), "name of output file with original movie data without noise (vector in PETSc format) [string]")
		("test_filename_gamma0", boost::program_options::value< std::string >(), "name of output file with initial gamma approximation (vector in PETSc format) [string]")
		("test_width", boost::program_options::value< int >(), "width of movie [int]")
		("test_height", boost::program_options::value< int >(), "height of movie [int]")
		("test_T", boost::program_options::value< int >(), "number of frames in movie [int]")
		("test_K", boost::program_options::value< int >(), "number of clusters for gamma0 [int]")
		("test_xdim", boost::program_options::value< int >(), "number of pixel values [int]")
		("test_data_type", boost::program_options::value< int >(), "type of output vector [0=TRn, 1=TnR, 2=nTR]")
		("test_noise", boost::program_options::value< double >(), "parameter of noise [double]")
		("test_mu0", boost::program_options::value< std::string >(), "color of background [string]")
		("test_mu1", boost::program_options::value< std::string >(), "color of dot [string]")
		("test_speed_x", boost::program_options::value< double >(), "speed of the dot in x axis [double]")
		("test_speed_y", boost::program_options::value< double >(), "speed of the dot in y axis [double]")
		("test_generate_data", boost::program_options::value< bool >(), "generate solution and data with noise [bool]")
		("test_generate_gamma0", boost::program_options::value< bool >(), "generate gamma0 [bool]");
	consoleArg.get_description()->add(opt_problem);

	/* call initialize */
	if(!Initialize<PetscVector>(argc, argv)){
		return 0;
	}

	/* start to measure time */
    Timer timer_all;
    timer_all.start();

	int width, height, T, sample_type;
	int K, xdim;
	double noise, init_speed_x, init_speed_y;
	int data_type;
	std::string filename_data;
	std::string filename_solution;
	std::string filename_gamma0;
	bool generate_data, generate_gamma0;

	consoleArg.set_option_value("test_sample_type", &sample_type, DEFAULT_SAMPLE_TYPE);
	consoleArg.set_option_value("test_filename_data", &filename_data, DEFAULT_FILENAME_DATA);
	consoleArg.set_option_value("test_filename_solution", &filename_solution, DEFAULT_FILENAME_SOLUTION);
	consoleArg.set_option_value("test_filename_gamma0", &filename_gamma0, DEFAULT_FILENAME_GAMMA0);
	consoleArg.set_option_value("test_width", &width, DEFAULT_WIDTH);
	consoleArg.set_option_value("test_height", &height, DEFAULT_HEIGHT);
	consoleArg.set_option_value("test_T", &T, DEFAULT_T);
	consoleArg.set_option_value("test_K", &K, DEFAULT_K);
	consoleArg.set_option_value("test_xdim", &xdim, DEFAULT_XDIM);
	consoleArg.set_option_value("test_data_type", &data_type, DEFAULT_DATA_TYPE);
	consoleArg.set_option_value("test_noise", &noise, DEFAULT_NOISE);
	consoleArg.set_option_value("test_generate_data", &generate_data, DEFAULT_GENERATE_DATA);
	consoleArg.set_option_value("test_generate_gamma0", &generate_gamma0, DEFAULT_GENERATE_GAMMA0);
	consoleArg.set_option_value("test_speed_x", &init_speed_x, DEFAULT_SPEED_X);
	consoleArg.set_option_value("test_speed_y", &init_speed_y, DEFAULT_SPEED_Y);

    /* read problem parameters - the color of dot and background */
	std::string mu0_string;
	std::string mu1_string;
	if(!consoleArg.set_option_value("test_mu0", &mu0_string)){
        if(xdim==1) mu0_string = DEFAULT_GENERATE_MU0_XDIM1;
        if(xdim==3) mu0_string = DEFAULT_GENERATE_MU0_XDIM3;
	}
	if(!consoleArg.set_option_value("test_mu1", &mu1_string)){
        if(xdim==1) mu1_string = DEFAULT_GENERATE_MU1_XDIM1;
        if(xdim==3) mu1_string = DEFAULT_GENERATE_MU1_XDIM3;
	}

	coutMaster << "mu0_string=" << mu0_string << std::endl;
	coutMaster << "mu1_string=" << mu1_string << std::endl;
	coutMaster << "xdim=" << xdim << std::endl;

    /* parse input strings to arrays */
	double mu0[xdim];
	double mu1[xdim];
    if(!parse_strings_to_doubles(xdim, mu0_string, mu0) ){
        coutMaster << "unable to parse input mu0 values!" << std::endl;
		return 0;
	}
    if(!parse_strings_to_doubles(xdim, mu1_string, mu1) ){
		coutMaster << "unable to parse input mu1 values!" << std::endl;
		return 0;
	}


	coutMaster << "- PROBLEM INFO ----------------------------" << std::endl;
	coutMaster << " test_sample_type            = " << std::setw(50) << sample_type << " type of generated sample [0=stationary, 1=moving, 2=bouncing]" << std::endl;
	coutMaster << " test_width                  = " << std::setw(50) << width << " (width of movie)" << std::endl;
	coutMaster << " test_height                 = " << std::setw(50) << height << " (height of movie)" << std::endl;
	coutMaster << " test_T                      = " << std::setw(50) << T << " (number of frames in movie)" << std::endl;
	coutMaster << " test_xdim                   = " << std::setw(50) << xdim << " (number of pixel values)" << std::endl;
	coutMaster << " test_K                      = " << std::setw(50) << K << " (number of clusters for gamma0)" << std::endl;
	coutMaster << " test_data_type              = " << std::setw(50) << Decomposition<PetscVector>::get_type_name(data_type) << " (type of output vector [" << Decomposition<PetscVector>::get_type_list() << "])" << std::endl;
	coutMaster << " test_noise                  = " << std::setw(50) << noise << " (parameter of noise)" << std::endl;
	coutMaster << " test_mu0                    = " << std::setw(50) << print_array(mu0,xdim) << " (color of background)" << std::endl;
	coutMaster << " test_mu1                    = " << std::setw(50) << print_array(mu1,xdim) << " (color of dot)" << std::endl;
	coutMaster << " test_filename_data          = " << std::setw(50) << filename_data << " (name of output file with movie data)" << std::endl;
	coutMaster << " test_filename_solution      = " << std::setw(50) << filename_solution << " (name of output file with original movie data without noise)" << std::endl;
	coutMaster << " test_filename_gamma0        = " << std::setw(50) << filename_gamma0 << " (name of output file with initial gamma approximation)" << std::endl;
	coutMaster << " test_generate_data          = " << std::setw(50) << print_bool(generate_data) << " (generate solution and data with noise)" << std::endl;
	coutMaster << " test_generate_gamma0        = " << std::setw(50) << print_bool(generate_gamma0) << " (generate gamma0)" << std::endl;
    if(sample_type == 1 | sample_type == 2){
        coutMaster << " test_speed_x                = " << std::setw(50) << init_speed_x << " (speed of the dot in x axis)" << std::endl;
        coutMaster << " test_speed_y                = " << std::setw(50) << init_speed_y << " (speed of the dot in y axis)" << std::endl;
    }
	coutMaster << "-------------------------------------------" << std::endl;

	/* say hello */
	coutMaster << "- start program" << std::endl;

    double r = width*0.2;   /* radius of sphere */

	/* allocate vector of data */
	Vec x_Vec; /* solution in TR format*/
	TRYCXX( VecCreate(PETSC_COMM_WORLD,&x_Vec) );
	TRYCXX( VecSetSizes(x_Vec,PETSC_DECIDE,T*width*height*xdim) );
	TRYCXX( VecSetType(x_Vec, VECSEQ) ); //TODO: MPI generator?
	TRYCXX( VecSetFromOptions(x_Vec) );

    Vec xdata_Vec; /* data with noise in TR format */
    TRYCXX( VecDuplicate(x_Vec, &xdata_Vec) );

    double speed_x = init_speed_x;
    double speed_y = init_speed_y;

    /* generate data */
    if(generate_data){
        double *x_arr;
        TRYCXX( VecGetArray(x_Vec, &x_arr) );

        double *xdata_arr;
        TRYCXX( VecGetArray(xdata_Vec, &xdata_arr) );

        /* Gaussian random number generator */
		std::default_random_engine generator(time(0));
		std::normal_distribution<double> distribution(0.0,noise);
//		std::uniform_real_distribution<double> distribution(0.0,noise);

        double c[2];            /* center of sphere */
		c[0] = width*0.5;
		c[1] = height*0.5;

        double value[xdim];

        for(int t=0; t < T; t++){
            /* compute new center of sphere */
			if(sample_type == 0){
				/* stationary */
			}
			if(sample_type == 1){
				/* moving */
				c[0] = width*0.1 + t*speed_x;
				c[1] = height*0.1 + t*speed_y;
			}
			if(sample_type == 2){
                double x_step = 1*speed_x; /* s=t*v (time step is one) */
                double y_step = 1*speed_y;

                c[0] += x_step;
                c[1] += y_step;

                if(c[0] >= width-r){
                    x_step = c[0] - (width - 1 - r);
                    c[0] = (width - 1 - r) - x_step;
                    speed_x = -speed_x;
                }
                if(c[1] >= height-r){
                    y_step = c[1] - (height - 1 - r);
                    c[1] = (height - 1 - r) - y_step;
                    speed_y = -speed_y;
                }
                if(c[0] < r){
                    x_step = r-c[0];
                    c[0] = x_step + r;
                    speed_x = -speed_x;
                }
                if(c[1] < r){
                    x_step = r-c[1];
                    c[1] = y_step + r;
                    speed_y = -speed_y;
                }
			}

            for(int i=0;i<width;i++){
                for(int j=0;j<height;j++){
                    if( (i-c[0])*(i-c[0]) + (j-c[1])*(j-c[1]) <= r*r ){
                        /* this point is sphere */
                        for(int n=0; n < xdim; n++){
                            value[n] = mu1[n];
                        }
                    } else {
                        /* this point is background */
                        for(int n=0; n < xdim; n++){
                            value[n] = mu0[n];
                        }
                    }

					for(int n=0; n < xdim; n++){
						int idx = 0;

						/* 0=TRn */
						if(data_type == 0){
							idx = t*xdim*width*height + j*width*xdim + i*xdim + n;
						}

						/* 1=TnR */
						if(data_type == 1){
							idx = t*xdim*width*height + n*width*height + j*width + i;
						}

						/* 2=nTR] */
						if(data_type == 2){
							idx = n*T*width*height + t*width*height + j*width + i;
						}

						x_arr[idx] = value[n];

						/* add noise */
						double noised_value = value[n] + distribution(generator);
						if(noised_value < 0.0) noised_value = 0.0;
						if(noised_value > 1.0) noised_value = 1.0;
						xdata_arr[idx] = noised_value;
					}
                }
            }
        }

        TRYCXX( VecRestoreArray(xdata_Vec, &xdata_arr) );
        TRYCXX( VecRestoreArray(x_Vec, &x_arr) );

		/* save generated vectors */
		PetscViewer mviewer;
		TRYCXX( PetscViewerCreate(PETSC_COMM_WORLD, &mviewer) );
		TRYCXX( PetscViewerBinaryOpen(PETSC_COMM_WORLD, filename_solution.c_str(), FILE_MODE_WRITE, &mviewer) );
		TRYCXX( VecView(x_Vec, mviewer) );
		TRYCXX( PetscViewerDestroy(&mviewer) );
		coutMaster << " - new solution vector saved" << std::endl;

		PetscViewer mviewer2;
		TRYCXX( PetscViewerCreate(PETSC_COMM_WORLD, &mviewer2) );
		TRYCXX( PetscViewerBinaryOpen(PETSC_COMM_WORLD, filename_data.c_str(), FILE_MODE_WRITE, &mviewer2) );
		TRYCXX( VecView(xdata_Vec, mviewer2) );
		TRYCXX( PetscViewerDestroy(&mviewer2) );
		coutMaster << " - new data vector saved" << std::endl;

    }


	if(generate_gamma0){
		/* vector for gamma0 */
		Vec gamma0_Vec;
		TRYCXX( VecCreate(PETSC_COMM_WORLD,&gamma0_Vec) );
        TRYCXX( VecSetSizes(gamma0_Vec,PETSC_DECIDE,K*T*width*height) );
        TRYCXX( VecSetType(gamma0_Vec, VECSEQ) ); //TODO: MPI generator?
        TRYCXX( VecSetFromOptions(gamma0_Vec) );

		/* generate random gamma0 */
		PetscRandom rctx2;
		TRYCXX( PetscRandomCreate(PETSC_COMM_WORLD,&rctx2) );
		TRYCXX( PetscRandomSetFromOptions(rctx2) );
		TRYCXX( VecSetRandom(gamma0_Vec,rctx2) );
		TRYCXX( PetscRandomDestroy(&rctx2) );

		/* save generated vector */
		PetscViewer mviewer3;
		TRYCXX( PetscViewerCreate(PETSC_COMM_WORLD, &mviewer3) );
		TRYCXX( PetscViewerBinaryOpen(PETSC_COMM_WORLD, filename_gamma0.c_str(), FILE_MODE_WRITE, &mviewer3) );
		TRYCXX( VecView(gamma0_Vec, mviewer3) );
		TRYCXX( PetscViewerDestroy(&mviewer3) );
		coutMaster << " - new random gamma0 vector saved" << std::endl;

	}

	/* say bye */
	coutMaster << "- end program" << std::endl;

	Finalize<PetscVector>();

	return 0;
}

