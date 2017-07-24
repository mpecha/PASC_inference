/** @file test_signal_fuzzykmeans.cpp
 *  @brief test the fuzzykmeans problem solver on nD signal problem
 *
 *  @author Lukas Pospisil
 */

#include "pascinference.h"

#include <vector>

#ifndef USE_PETSC
 #error 'This example is for PETSC'
#endif

#define DEFAULT_FUZZIFIER 2
#define DEFAULT_K 2
#define DEFAULT_DATA_TYPE 1
#define DEFAULT_XDIM 1
#define DEFAULT_FEM_TYPE 1
#define DEFAULT_FEM_REDUCE 1.0
#define DEFAULT_SIGNAL_IN "data/test_signal/samplesignal.bin"
#define DEFAULT_SIGNAL_OUT "samplesignal"
#define DEFAULT_SIGNAL_SOLUTION "data/test_signal/samplefilename_solution.bin"
#define DEFAULT_SCALEDATA false
#define DEFAULT_ANNEALING 1
#define DEFAULT_CUTGAMMA false
#define DEFAULT_CUTDATA false
#define DEFAULT_PRINTSTATS false
#define DEFAULT_PRINTINFO false
#define DEFAULT_SAVERESULT true
#define DEFAULT_SHORTINFO true
#define DEFAULT_SHORTINFO_FILENAME "shortinfo/samplesignal.txt"

using namespace pascinference;

int main( int argc, char *argv[] )
{
	/* add local program options */
	boost::program_options::options_description opt_problem("PROBLEM EXAMPLE", consoleArg.get_console_nmb_cols());
	opt_problem.add_options()
		("test_fuzzifier", boost::program_options::value<double>(), "fuzzy parameter of fuzzy kmeans [double]")
		("test_K", boost::program_options::value<int>(), "number of clusters [int]")
		("test_xdim", boost::program_options::value<int>(), "dimension of data points [int]")
		("test_data_type", boost::program_options::value< int >(), "type of input/output vector [0=TRn, 1=TnR, 2=nTR]")
		("test_fem_type", boost::program_options::value<int>(), "type of used FEM to reduce problem [0=FEM_SUM/1=FEM_HAT]")
		("test_fem_reduce", boost::program_options::value<double>(), "parameter of the reduction of FEM nodes [int,-1=false]")
		("test_filename_in", boost::program_options::value< std::string >(), "name of input file with signal data (vector in PETSc format) [string]")
		("test_filename_out", boost::program_options::value< std::string >(), "name of output file with filtered signal data (vector in PETSc format) [string]")
		("test_filename_solution", boost::program_options::value< std::string >(), "name of input file with original signal data without noise (vector in PETSc format) [string]")
		("test_filename_gamma0", boost::program_options::value< std::string >(), "name of input file with initial gamma approximation (vector in PETSc format) [string]")
		("test_scaledata", boost::program_options::value<bool>(), "scale to interval {-1,1} [bool]")
		("test_annealing", boost::program_options::value<int>(), "number of annealing steps [int]")
		("test_cutgamma", boost::program_options::value<bool>(), "cut gamma to set {0;1} [bool]")
		("test_cutdata", boost::program_options::value<bool>(), "cut data to interval {0,1} [bool]")
		("test_printstats", boost::program_options::value<bool>(), "print basic statistics of data [bool]")
		("test_printinfo", boost::program_options::value<bool>(), "print informations about created objects [bool]")
		("test_saveresult", boost::program_options::value<bool>(), "save the solution [bool]")
		("test_Theta", boost::program_options::value<std::vector<std::string> >()->multitoken(), "given solution Theta [K*\"xdim*double\"]")
		("test_shortinfo", boost::program_options::value<bool>(), "save shortinfo file after computation [bool]")
		("test_shortinfo_header", boost::program_options::value< std::string >(), "additional header in shortinfo [string]")
		("test_shortinfo_values", boost::program_options::value< std::string >(), "additional values in shortinfo [string]")
		("test_shortinfo_filename", boost::program_options::value< std::string >(), "name of shortinfo file [string]");
	consoleArg.get_description()->add(opt_problem);

	/* call initialize */
	if(!Initialize<PetscVector>(argc, argv)){
		return 0;
	}

	/* start to measure power consumption and time */
    Timer timer_all;
    timer_all.start();
	const int ranks_per_node = PowerCheck::get_ranks_per_node();
    double node_energy    = PowerCheck::get_node_energy()/(double)ranks_per_node;
    double device_energy  = PowerCheck::get_device_energy()/(double)ranks_per_node;

	int K, fem_type, xdim, data_type, annealing;
	bool cutgamma, scaledata, cutdata, printstats, printinfo, shortinfo_write_or_not, saveresult;
	double fem_reduce, fuzzifier;

	std::string filename_in;
	std::string filename_out;
	std::string filename_solution;
	std::string filename_gamma0;
	std::string shortinfo_filename;
	std::string shortinfo_header;
	std::string shortinfo_values;

	consoleArg.set_option_value("test_fuzzifier", &fuzzifier, DEFAULT_FUZZIFIER);
	consoleArg.set_option_value("test_K", &K, DEFAULT_K);
	consoleArg.set_option_value("test_xdim", &xdim, DEFAULT_XDIM);
	consoleArg.set_option_value("test_data_type", &data_type, DEFAULT_DATA_TYPE);
	consoleArg.set_option_value("test_fem_type", &fem_type, DEFAULT_FEM_TYPE);
	consoleArg.set_option_value("test_fem_reduce", &fem_reduce, DEFAULT_FEM_REDUCE);
	consoleArg.set_option_value("test_filename_in", &filename_in, DEFAULT_SIGNAL_IN);
	consoleArg.set_option_value("test_filename_out", &filename_out,DEFAULT_SIGNAL_OUT);
	consoleArg.set_option_value("test_scaledata", &scaledata, DEFAULT_SCALEDATA);
	consoleArg.set_option_value("test_annealing", &annealing, DEFAULT_ANNEALING);
	consoleArg.set_option_value("test_cutgamma", &cutgamma, DEFAULT_CUTGAMMA);
	consoleArg.set_option_value("test_cutdata", &cutdata, DEFAULT_CUTDATA);
	consoleArg.set_option_value("test_printstats", &printstats, DEFAULT_PRINTSTATS);
	consoleArg.set_option_value("test_printinfo", &printinfo, DEFAULT_PRINTINFO);
	consoleArg.set_option_value("test_saveresult", &saveresult, DEFAULT_SAVERESULT);
	consoleArg.set_option_value("test_shortinfo", &shortinfo_write_or_not, DEFAULT_SHORTINFO);
	consoleArg.set_option_value("test_shortinfo_header", &shortinfo_header, "");
	consoleArg.set_option_value("test_shortinfo_values", &shortinfo_values, "");
	consoleArg.set_option_value("test_shortinfo_filename", &shortinfo_filename, DEFAULT_SHORTINFO_FILENAME);

	/* maybe solution is given */
	bool given_solution;
	if(!consoleArg.set_option_value("test_filename_solution", &filename_solution)){
		given_solution=false;

		/* maybe we run program with default values */
		if(filename_in == DEFAULT_SIGNAL_IN){
			filename_solution = DEFAULT_SIGNAL_SOLUTION;
			given_solution=true;
		}
	} else {
		given_solution=true;
	}

	/* maybe gamma0 is given in console parameters */
	bool given_gamma0;
	if(consoleArg.set_option_value("test_filename_gamma0", &filename_gamma0)){
		given_gamma0 = true;
	} else {
		given_gamma0 = false;
	}

	/* maybe theta is given in console parameters */
	bool given_Theta;
	std::vector<std::string> Theta_list;
	if(consoleArg.set_option_value("test_Theta", &Theta_list)){
		given_Theta = true;

		/* control number of provided Theta */
		if(Theta_list.size() != K){
			coutMaster << "number of provided Theta solutions is different then number of clusters! (you provided " << Theta_list.size() << " parameters)" << std::endl;
			return 0;
		}
	} else {
		given_Theta = false;
	}

	/* set decomposition in space */
	int DDT_size = GlobalManager.get_size();

	coutMaster << "- PROBLEM INFO ----------------------------" << std::endl;
#ifdef USE_CUDA
	coutMaster << " computing on GPU" << std::endl;
#else
	coutMaster << " computing on CPU" << std::endl;
#endif
	coutMaster << " ranks_per_node              = " << std::setw(50) << ranks_per_node << " (number of MPI processes on one node)" << std::endl;
	coutMaster << " DDT_size                    = " << std::setw(50) << DDT_size << " (decomposition in space)" << std::endl;
	coutMaster << " test_fuzzifier              = " << std::setw(50) << fuzzifier << " (fuzzy parameter of fuzzy kmeans)" << std::endl;
	coutMaster << " test_K                      = " << std::setw(50) << K << " (number of clusters)" << std::endl;
	coutMaster << " test_xdim                   = " << std::setw(50) << xdim << " (dimension of data points)" << std::endl;
	coutMaster << " test_data_type              = " << std::setw(50) << Decomposition<PetscVector>::get_type_name(data_type) << " (type of output vector [" << Decomposition<PetscVector>::get_type_list() << "])" << std::endl;
	coutMaster << " test_Theta                  = " << std::setw(50) << print_bool(given_Theta) << " (given solution Theta)" << std::endl;
	coutMaster << " test_fem_type               = " << std::setw(50) << fem_type << " (type of used FEM to reduce problem [0=FEM_SUM/1=FEM_HAT])" << std::endl;
	coutMaster << " test_fem_reduce             = " << std::setw(50) << fem_reduce << " (parameter of the reduction of FEM node)" << std::endl;
	coutMaster << " test_filename_in            = " << std::setw(50) << filename_in << " (name of input file with signal data)" << std::endl;
	coutMaster << " test_filename_out           = " << std::setw(50) << filename_out << " (name of output file with filtered signal data)" << std::endl;
	if(given_solution){
		coutMaster << " test_filename_solution      = " << std::setw(50) << filename_solution << " (name of input file with original signal data without noise)" << std::endl;
	} else {
		coutMaster << " test_filename_solution      = " << std::setw(50) << "NO" << " (name of input file with original signal data without noise)" << std::endl;
	}
	if(given_gamma0){
		coutMaster << " test_filename_gamma0        = " << std::setw(50) << filename_gamma0 << " (name of input file with initial gamma approximation)" << std::endl;
	} else {
		coutMaster << " test_filename_gamma0        = " << std::setw(50) << "NO" << " (name of input file with initial gamma approximation)" << std::endl;
	}
	coutMaster << " test_annealing              = " << std::setw(50) << annealing << " (number of annealing steps)" << std::endl;
	coutMaster << " test_cutgamma               = " << std::setw(50) << print_bool(cutgamma) << " (cut gamma to {0;1})" << std::endl;
	coutMaster << " test_cutdata                = " << std::setw(50) << print_bool(cutdata) << " (cut data to {0,1})" << std::endl;
	coutMaster << " test_scaledata              = " << std::setw(50) << print_bool(scaledata) << " (scale data to {-1,1})" << std::endl;
	coutMaster << " test_saveresult             = " << std::setw(50) << print_bool(saveresult) << " (save reconstructed signal)" << std::endl;
	coutMaster << " test_printstats             = " << std::setw(50) << print_bool(printstats) << " (print basic statistics of data)" << std::endl;
	coutMaster << " test_printinfo              = " << std::setw(50) << print_bool(printinfo) << " (print informations about created objects)" << std::endl;
	coutMaster << " test_shortinfo              = " << std::setw(50) << print_bool(shortinfo_write_or_not) << " (save shortinfo file after computation)" << std::endl;
	coutMaster << " test_shortinfo_header       = " << std::setw(50) << shortinfo_header << " (additional header in shortinfo)" << std::endl;
	coutMaster << " test_shortinfo_values       = " << std::setw(50) << shortinfo_values << " (additional values in shortinfo)" << std::endl;
	coutMaster << " test_shortinfo_filename     = " << std::setw(50) << shortinfo_filename << " (name of shortinfo file)" << std::endl;
	coutMaster << "-------------------------------------------" << std::endl;


	/* start logging */
	std::ostringstream oss;
	oss << "log/" << filename_out << ".txt";
	logging.begin(oss.str());
	oss.str("");

	/* start shortinfo output */
	if(shortinfo_write_or_not){
		shortinfo.begin(shortinfo_filename);
	}
	std::ostringstream oss_short_output_values;
	std::ostringstream oss_short_output_header;

	/* say hello */
	coutMaster << "- start program" << std::endl;

/* 1.) prepare preliminary time-series data (to get the size of the problem T) */
	coutMaster << "--- PREPARING PRELIMINARY DATA ---" << std::endl;
	SignalData<PetscVector> mydata(filename_in);

/* 2.) prepare decomposition */
	coutMaster << "--- COMPUTING DECOMPOSITION ---" << std::endl;

	/* prepare decomposition based on preloaded data */
	Decomposition<PetscVector> decomposition(mydata.get_Tpreliminary()/xdim, 1, K, xdim, DDT_size);

	/* print info about decomposition */
	if(printinfo) decomposition.print(coutMaster);

/* 3.) prepare time-series data */
	coutMaster << "--- APPLY DECOMPOSITION TO DATA ---" << std::endl;
	mydata.set_decomposition(decomposition, data_type);

	/* print information about loaded data */
	if(printinfo) mydata.print(coutMaster);

	/* print statistics */
	if(printstats) mydata.printstats(coutMaster);

/* 4.) prepare and load solution */
	Vec solution_Vec;
	Vec solution_Vec_preload;
	GeneralVector<PetscVector> solution(solution_Vec);
	if(given_solution){
		TRYCXX( VecDuplicate(mydata.get_datavector()->get_vector(),&solution_Vec) );
		TRYCXX( VecDuplicate(mydata.get_datavector()->get_vector(),&solution_Vec_preload) );

		solution.load_global(filename_solution);
		decomposition.permute_to_pdTRb(solution.get_vector(), solution_Vec_preload, decomposition.get_xdim(), data_type, false);

		TRYCXX( VecCopy(solution_Vec_preload, solution.get_vector()));
		TRYCXX( VecDestroy(&solution_Vec_preload) );
	}

/* 5.) prepare model */
	coutMaster << "--- PREPARING MODEL ---" << std::endl;

	/* prepare FEM reduction */
	Fem<PetscVector> *fem;
	if(fem_type == 0){
		fem = new Fem1DSum<PetscVector>(fem_reduce);
	}
	if(fem_type == 1){
		fem = new Fem1DHat<PetscVector>(fem_reduce);
	}

	/* prepare model on the top of given data */
	FuzzyKmeansModel<PetscVector> mymodel(mydata, fuzzifier, fem);

	/* print info about model */
	if(printinfo) mymodel.print(coutMaster,coutAll);

/* 6.) prepare time-series solver */
	coutMaster << "--- PREPARING SOLVER ---" << std::endl;

	/* prepare time-series solver */
	TSSolver<PetscVector> mysolver(mydata, annealing);

	/* if gamma0 is provided, then load it */
	if(given_gamma0){
		coutMaster << " - loading and setting gamma0" << std::endl;
		mydata.load_gammavector(filename_gamma0);
	}

	/* print info about solver */
	if(printinfo) mysolver.print(coutMaster,coutAll);

	/* set solution if obtained from console */
	double *Theta_solution;
	if(given_Theta){
		/* parse strings to doubles */
		Theta_solution = new double[K*xdim];
		if(!parse_strings_to_doubles(K,xdim, Theta_list, Theta_solution) ){
			coutMaster << "unable to parse input Theta values!" << std::endl;
			return 0;
		}
		mysolver.set_solution_theta(Theta_solution);
	}

/* 6.) solve the problem */
	double abserr; /* actual error */
	double L; /* actual value of objective function */

	/* energy for one iteration */
	double node_energy_it;
   	double node_energy_it_sum;

	coutMaster << "--- SOLVING THE PROBLEM with epssqr ---" << std::endl;

	/* cut data */
	if(cutdata) mydata.cutdata(0,1);

	/* scale data */
	if(scaledata){
		mydata.scaledata(-1,1,0,1);
	}

	/* measure energy at begin */
	MPI_Barrier(MPI_COMM_WORLD);
	node_energy_it    = PowerCheck::get_node_energy()/(double)ranks_per_node;

	/* !!! solve the problem */
	mysolver.solve();

	/* measure energy in the end */
	MPI_Barrier(MPI_COMM_WORLD);
	node_energy_it     = PowerCheck::get_node_energy()/(double)ranks_per_node - node_energy_it;
	node_energy_it_sum = PowerCheck::mpi_sum_reduce(node_energy_it);

	/* cut gamma */
	if(cutgamma) mydata.cutgamma();

	/* unscale data before save */
	if(scaledata){
		mydata.scaledata(0,1,-1,1);
	}

	/* compute absolute error of computed solution */
	if(given_solution){
		abserr = mydata.compute_abserr_reconstructed(solution);
	} else {
		abserr = -1.0;
	}
	coutMaster << " - abserr = " << abserr << std::endl;

	/* compute value of objective function */
	L = mysolver.get_L();
	coutMaster << " - L = " << L << std::endl;

	/* store short info */
	if(shortinfo_write_or_not){
		/* add provided strings from console parameters and info about the problem */
		oss_short_output_header << shortinfo_header << "K,abserr,L,energy,";
		oss_short_output_values << shortinfo_values << K << "," << abserr << "," << L << "," << node_energy_it_sum << ",";

		/* append Theta solution */
		for(int k=0; k<K; k++) oss_short_output_header << "Theta" << k << ",";
		oss_short_output_values << mydata.print_thetavector();

		/* print info from solver */
		mysolver.printshort(oss_short_output_header, oss_short_output_values);

		/* append end of line */
		oss_short_output_header << "\n";
		oss_short_output_values << "\n";

		/* write to shortinfo file */
		shortinfo.write(oss_short_output_header.str());
		shortinfo.write(oss_short_output_values.str());

		/* clear streams for next writing */
		oss_short_output_header.str("");
		oss_short_output_values.str("");
	}

	
/* 8.) store solution */
	if(saveresult){
		coutMaster << "--- SAVING OUTPUT ---" << std::endl;
		oss << filename_out;
		mydata.save_reconstructed(oss.str(),data_type);
		mydata.save_gammavector(oss.str());
		oss.str("");
	}

	/* print solution */
	coutMaster << "--- THETA SOLUTION ---" << std::endl;
	mydata.print_thetavector(coutMaster);

	/* print timers */
	coutMaster << "--- TIMERS INFO ---" << std::endl;
	mysolver.printtimer(coutMaster);

	/* print short info */
	coutMaster << "--- FINAL SOLVER INFO ---" << std::endl;
	mysolver.printstatus(coutMaster);

	/* print info about power consumption */
	timer_all.stop();
	MPI_Barrier(MPI_COMM_WORLD);
	node_energy    = PowerCheck::get_node_energy()/(double)ranks_per_node - node_energy;
	device_energy  = PowerCheck::get_device_energy()/(double)ranks_per_node - device_energy;

	double total_node_energy = PowerCheck::mpi_sum_reduce(node_energy);
	double total_device_energy = PowerCheck::mpi_sum_reduce(device_energy);
	double time_all = timer_all.get_value_sum();

	coutMaster << "--- ENERGY INFO --------------------------------" << std::endl;
	coutMaster << "- total time: " << time_all << " s" << std::endl;
	#ifdef USE_CRAYPOWER
		coutMaster << "- node" << std::endl;
		coutMaster.push();
		coutMaster << "- " << total_node_energy << " Joules, ";
		coutMaster << total_node_energy/time_all << " Watts";
		coutMaster << std::endl;
		coutMaster.pop();
		coutMaster << "- device" << std::endl;
		coutMaster.push();
		coutMaster << "- " << total_device_energy << " Joules, ";
		coutMaster << total_device_energy/time_all << " Watts";
		coutMaster << std::endl;
		coutMaster.pop();
		coutMaster << "- node + device" << std::endl;
		coutMaster.push();
		coutMaster << "- " << (total_node_energy + total_device_energy) << " Joules, ";
		coutMaster << (total_node_energy + total_device_energy)/time_all << " Watts";
		coutMaster << std::endl;
		coutMaster.pop();
	#endif

	coutMaster << "------------------------------------------------" << std::endl;

	/* say bye */
	coutMaster << "- end program" << std::endl;

	logging.end();
	Finalize<PetscVector>();

	return 0;
}

