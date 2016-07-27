/** @file test_image_ts.cu
 *  @brief test the kmeans problem solver on image processing problem, take image as 1D time-series problem
 *
 *  Load image file, graph file and solve the problem.
 *
 *  @author Lukas Pospisil
 */

#include "pascinference.h"
#include "solver/tssolver.h"
#include "data/imagedata.h"
#include "model/graphh1fem.h"

#ifndef USE_PETSCVECTOR
 #error 'This example is for PETSCVECTOR'
#endif
 
using namespace pascinference;

typedef petscvector::PetscVector PetscVector;

extern int pascinference::DEBUG_MODE;

int main( int argc, char *argv[] )
{
	/* add local program options */
	boost::program_options::options_description opt_problem("PROBLEM EXAMPLE", consoleArg.get_console_nmb_cols());
	opt_problem.add_options()
		("test_image_filename", boost::program_options::value< std::string >(), "name of input file with image data (vector in PETSc format) [string]")
		("test_image_out", boost::program_options::value< std::string >(), "name of output file with image data (vector in PETSc format) [string]")
		("test_K", boost::program_options::value<int>(), "number of clusters [int]")
		("test_width", boost::program_options::value<int>(), "width of image [int]")
		("test_height", boost::program_options::value<int>(), "height of image [int]")
		("test_epssqr", boost::program_options::value<double>(), "penalty parameter [double]")
		("test_annealing", boost::program_options::value<int>(), "number of annealing steps [int]")
		("test_cutgamma", boost::program_options::value<bool>(), "cut gamma to {0,1} [bool]");
	consoleArg.get_description()->add(opt_problem);

	/* call initialize */
	if(!Initialize(argc, argv)){
		return 0;
	} 

	int K, annealing, width, height; 
	double epssqr;
	bool cutgamma;

	std::string image_filename;
	std::string image_out;

	consoleArg.set_option_value("test_K", &K, 2);
	consoleArg.set_option_value("test_width", &width, 100);
	consoleArg.set_option_value("test_height", &height, 100);
	consoleArg.set_option_value("test_epssqr", &epssqr, 10);
	consoleArg.set_option_value("test_cutgamma", &cutgamma, false);
	consoleArg.set_option_value("test_annealing", &annealing, 1);
	consoleArg.set_option_value("test_image_filename", &image_filename, "data/image1.bin");
	consoleArg.set_option_value("test_image_out", &image_out, "image1");

	coutMaster << "- PROBLEM INFO ----------------------------" << std::endl;
	coutMaster << " test_image_filename  = " << std::setw(30) << image_filename << " (name of input file with image data)" << std::endl;
	coutMaster << " test_image_out       = " << std::setw(30) << image_out << " (part of name of output file)" << std::endl;
	coutMaster << " test_K               = " << K << " (number of clusters)" << std::endl;
	coutMaster << " test_width           = " << width << " (width of image)" << std::endl;
	coutMaster << " test_height          = " << height << " (height of image)" << std::endl;
	coutMaster << " test_epssqr          = " << epssqr << " (penalty)" << std::endl;
	coutMaster << " test_annealing       = " << annealing << " (number of annealing steps)" << std::endl;
	coutMaster << " test_cutgamma        = " << cutgamma << " (cut gamma to {0,1})" << std::endl;
	coutMaster << "-------------------------------------------" << std::endl << std::endl;

	/* start logging */
	std::ostringstream oss_name_of_file_log;
	oss_name_of_file_log << "results/image_ts_p" << GlobalManager.get_rank() << ".txt";
	logging.begin(oss_name_of_file_log.str());
		
	/* say hello */
	coutMaster << "- start program" << std::endl;

	/* prepare time-series data */
	coutMaster << "--- PREPARING DATA ---" << std::endl;
	ImageData<PetscVector> mydata(image_filename, width, height, height);
	mydata.print(coutMaster, coutAll);

	/* prepare model */
	coutMaster << "--- PREPARING MODEL ---" << std::endl;

	BGMGraphGrid1D mygraph(width);
	mygraph.process_grid();
	mygraph.print(coutMaster);

	GraphH1FEMModel<PetscVector> mymodel(mydata, mygraph, K, epssqr);
	mymodel.print(coutMaster,coutAll);

	/* prepare time-series solver */
	coutMaster << "--- PREPARING SOLVER ---" << std::endl;
	TSSolver<PetscVector> mysolver(mydata, annealing);

	/* solve the problem */
	coutMaster << "--- SOLVING THE PROBLEM ---" << std::endl;
	mysolver.solve();

	/* cut gamma */
	if(cutgamma){
		mydata.cut_gamma();
	}

	/* save results into CSV file */
	coutMaster << "--- SAVING OUTPUT ---" << std::endl;
	mydata.saveImage(image_out);

	/* print timers */
	coutMaster << "--- TIMERS INFO ---" << std::endl;
	mysolver.printtimer(coutAll);
	coutAll.synchronize();

	/* print short info */
	coutMaster << "--- FINAL SOLVER INFO ---" << std::endl;
	mysolver.printstatus(coutMaster);

	/* say bye */	
	coutMaster << "- end program" << std::endl;

	logging.end();
	Finalize();

	return 0;
}
