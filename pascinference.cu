/*******************************************************************************
PASC INFERENCE library
Lukas Pospisil, Illia Horenko, Patrick Gagliardini, Will Sawyer
USI Lugano, 2016
lukas.pospisil@usi.ch

*******************************************************************************/

#include "common.h"
#include "problem.h"
#include "data.h"
#include "model.h"

#include <boost/program_options.hpp>

#define ALGORITHM_deltaL_eps 0.0001 /*stopping criteria of outer main loop */
#define ALGORITHM_max_s_steps 100 /* max number of outer steps */
#define ALGORITHM_EPSSQUARE 10.0 /* default FEM regularization parameter */

int T = 10; /* default length of generated time serie */
int K = 3; /* default number of clusters */
int dim = 2; /* default dimension of the problem */

/* PetscVector */
#ifdef USE_PETSC // TODO: add to common.h
	#include "petscvector.h"
#endif

bool load_from_console(int argc, char *argv[]){
	bool return_value = true; /* continue of not? */

	namespace po = boost::program_options;

	/* define command line options */
	po::options_description description("PASC Inference Usage");
	description.add_options()
		("help,h", "Display this help message")
		("version,v", "Display the version number")
		("debug", po::value<int>(), "Debug mode")
		("length,T", po::value<int>(), "Length of time series")
		("clusters,K", po::value<int>(), "number of clusters");	
	
	/* parse command line arguments */	
	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
	po::notify(vm);		

	/* what to do with parsed arguments? */	
	if(vm.count("help")){ // TODO: this can be included in global application
		std::cout << description;
		return_value = false;
	}

	if(vm.count("version")){// TODO: this can be included in global application
		std::cout << "not implemented yet" << std::endl;
		return_value = false;
	}

	if(vm.count("debug")){// TODO: this can be included in global application
		DEBUG_MODE = vm["debug"].as<int>(); /* set global variable */
	}

	if(vm.count("length")){
		T = vm["length"].as<int>(); /* set global variable */
	}

	if(vm.count("clusters")){
		K = vm["clusters"].as<int>(); /* set global variable */
	}

	
	return return_value;
}

/* --- MAIN FUNCTION ---- */
int main( int argc, char *argv[] )
{
	/* load parameters from console input */
	if(!load_from_console(argc, argv)){
		return 0;
	}
	
		
	Initialize(argc, argv); // TODO: load parameters of problem from console input

	PetscVector vec1(10);
	vec1.set(1.0); // TODO: vec1(all) = 2
	
	vec1(2) = 5.5;

	std::cout << "vec1(2): " << vec1(2) << std::endl;
	std::cout << "vec1: " << vec1 << std::endl;

	PetscVector vec2(10);
	vec2 = 5*vec1;

	std::cout << "vec2: " << vec2 << std::endl;

	PetscVector vec3(10);
//	PetscVectorWrapperComb test;
	vec3 = -2*vec1 + 5*vec2;
	std::cout << "vec3: " << vec3 << std::endl;


	PetscVector vec4(10);
//	PetscVectorWrapperComb test;
	vec4 = 2*vec1 + vec2 + 0*vec3;

	std::cout << "vec4: " << vec4 << std::endl;


if(false){ // TODO: temp
	Timer timer_program; /* global timer for whole application */
	Timer timer_data; /* for generating the problem */
	Timer timer_model; /* for manipulation with model */

	timer_program.restart();
	timer_data.restart();
	timer_model.restart();

	/* say hello */	
	Message("- start program");
	timer_program.start(); /* here starts the timer for whole application */
	
	/* prepare data */
	Data_kmeans data;
	data.init(dim,T); // TODO: make it more funny using input K
	timer_data.start(); 
	 data.generate();
	timer_data.stop();

	if(DEBUG_MODE >= 2) Message_info_time(" - problem generated in: ",timer_data.get_value_last());
	if(DEBUG_MODE >= 3)	data.print();

	/* prepare model */
	Model_kmeans model;
	timer_model.start(); 
	 model.init(dim,T,K);
	timer_model.stop();

	if(DEBUG_MODE >= 2) Message_info_time(" - model prepared in: ",timer_model.get_value_last());
	if(DEBUG_MODE >= 3)	model.print();

	/* prepare problem */
	Problem problem;
	problem.init();
	problem.set_data(data); /* set data to problem */
	problem.set_model(model); /* set model to problem */


	Message("- run main cycle:");
	 problem.solve(ALGORITHM_max_s_steps,ALGORITHM_deltaL_eps);
	Message("- main cycle finished");

	if(DEBUG_MODE >= 2)	problem.print();

	/* save the solution to VTK */
	if(EXPORT_SAVEVTK){
		Message("- save solution to VTK");
		problem.saveVTK("output/data.vtk");
	}

	/* here ends the application */
	problem.finalize();
	timer_program.stop();

	/* say bye */	
	Message("- end program");

	if(DEBUG_MODE >= 2) Message_info_time("- elapsed time: ",timer_program.get_value_sum());
	if(DEBUG_MODE >= 3)	problem.print();
}	
	Finalize();
	return 0;
}

