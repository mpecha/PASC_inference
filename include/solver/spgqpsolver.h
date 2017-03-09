/** @file spgqpsolver.h
 *  @brief Spectral Projected Gradient method for solving Quadratic Programs
 *
 *  @author Lukas Pospisil
 */

#ifndef PASC_SPGQPSOLVER_H
#define	PASC_SPGQPSOLVER_H

#include <iostream>

#include "pascinference.h"
#include "solver/qpsolver.h"
#include "data/qpdata.h"

#ifdef USE_CUDA
 #include <../src/vec/vec/impls/seq/seqcuda/cudavecimpl.h>
#endif

#include "algebra/matrix/blockgraphsparse.h"


#define SPGQPSOLVER_DEFAULT_MAXIT 1000
#define SPGQPSOLVER_DEFAULT_EPS 1e-9
#define SPGQPSOLVER_DEFAULT_DEBUGMODE 0

#define SPGQPSOLVER_DEFAULT_M 20
#define SPGQPSOLVER_DEFAULT_GAMMA 0.9
#define SPGQPSOLVER_DEFAULT_SIGMA1 0.000
#define SPGQPSOLVER_DEFAULT_SIGMA2 1.0
#define SPGQPSOLVER_DEFAULT_ALPHAINIT 2.0

#define SPGQPSOLVER_STOP_NORMGP false
#define SPGQPSOLVER_STOP_ANORMGP false
#define SPGQPSOLVER_STOP_NORMGP_NORMB false
#define SPGQPSOLVER_STOP_ANORMGP_NORMB false
#define SPGQPSOLVER_STOP_DIFFF true

#define SPGQPSOLVER_MONITOR false
#define SPGQPSOLVER_DUMP false

#ifdef USE_PETSCVECTOR
	typedef petscvector::PetscVector PetscVector;
#endif

#ifdef USE_CUDA
//    #include <../src/vec/vec/impls/seq/seqcuda/cudavecimpl.h>
#endif

namespace pascinference {
namespace solver {

/** \class SPGQPSolver
 *  \brief Spectral Projected Gradient method for Quadratic Programs
 *
 *  For solving QP on closed convex set using projections.
*/
template<class VectorBase>
class SPGQPSolver: public QPSolver<VectorBase> {
	private:
		/** \class SPGQPSolver_fs
		 *  \brief generalized Armijo condition
		 *
		 *  For manipulation with fs - function values for generalized Armijo condition used in SPGQP.
		*/
		class SPGQPSolver_fs {
			private:
				int m; /**< the length of list */
				double *fs_list; /**< the list with function values */
				int last_idx;

			public: 
				/** @brief constructor
				*
				* @param new_m length of lists
				*/
				SPGQPSolver_fs(int new_m);

				/** @brief deconstructor
				*
				*/
				~SPGQPSolver_fs();

				/** @brief set all values to given one
				*
				* At the begining of computation, all values are the same, set them using this function.
				* 
				* @param fx function value
				*/
				void init(double fx);

				/** @brief return maximum value from the list
				*
				*/
				double get_max();		

				/** @brief return length of lists
				*
				*/
				int get_size();
		
				/** @brief update list - add new value and remove oldest one
				*
				*/
				void update(double new_fx);
		
				/** @brief print content of the lists
				*
				* @param output where to print
				*/
				void print(ConsoleOutput &output);
		};

		Timer timer_solve; 			/**< total solution time of SPG algorithm */
		Timer timer_projection;		/**< the sum of time necessary to perform projections */
		Timer timer_matmult; 		/**< the sum of time necessary to perform matrix multiplication */
		Timer timer_dot; 			/**< the sum of time necessary to compute dot_products */
		Timer timer_update; 		/**< total time of vector updates */
		Timer timer_stepsize;	 	/**< total time of step-size computation */
		Timer timer_fs; 			/**< total time of manipulation with fs vector during iterations */

		bool stop_normgp; 			/**< stopping criteria based on norm of gP */
		bool stop_Anormgp; 			/**< stopping criteria based on A-norm of gP */
		bool stop_normgp_normb;		/**< stopping criteria based on norm of gP and norm of b */
		bool stop_Anormgp_normb;	/**< stopping criteria based on A-norm of gP and norm of b */
		bool stop_difff;			/**< stopping criteria based on size of decrease of f */

		bool monitor;				/**< export the descend into .m file */

		int m;						/**< size of SPGQPSolver_fs */
		double gamma;				/**< parameter of Armijo condition */
		double sigma1;				/**< to enforce progress */
		double sigma2;				/**< to enforce progress */
		double alphainit;			/** initial step-size */

		QPData<VectorBase> *qpdata; /**< data on which the solver operates */
		double gP; 					/**< norm of projected gradient */
	
		/* temporary vectors used during the solution process */
		/** @brief allocate storage for auxiliary vectors used in computation
		* 
		*/
		void allocate_temp_vectors();

		/** @brief deallocate storage for auxiliary vectors used in computation
		* 
		*/
		void free_temp_vectors();

		GeneralVector<VectorBase> *g; 		/**< gradient */
		GeneralVector<VectorBase> *d; 		/**< projected gradient */
		GeneralVector<VectorBase> *Ad; 		/**< A*d */
		GeneralVector<VectorBase> *temp;	/**< general temp vector */

		/** @brief compute multiple dot products
		* 
		* @param dd result of dot(d,d)
		* @param dAd result of dot(Ad,d)
		* @param gd result of dot(g,d)
		*/
		void compute_dots(double *dd, double *dAd, double *gd) const;

		double *Mdots_val; /**< for manipulation with mdot */

		#ifdef USE_PETSCVECTOR
			Vec *Mdots_vec; /**< for manipulation with mdot */
		#endif

		/** @brief set settings of algorithm from arguments in console
		* 
		*/
		void set_settings_from_console();
		
		int debugmode;				/**< basic debug mode schema [0/1/2] */
		bool debug_print_it;		/**< print simple info about outer iterations */
		bool debug_print_vectors;	/**< print content of vectors during iterations */
		bool debug_print_scalars;	/**< print values of computed scalars during iterations */ 
		
		void copy_to_gpu();
		
	public:
		/** @brief general constructor
		* 
		*/
		SPGQPSolver();

		/** @brief constructor based on provided data of problem
		* 
		* @param new_qpdata data of quadratic program
		*/
		SPGQPSolver(QPData<VectorBase> &new_qpdata); 

		/** @brief destructor
		* 
		*/
		~SPGQPSolver();

		void solve();

		double get_fx() const;
		double get_fx(double fx_old, double beta, double gd, double dAd) const;

		void print(ConsoleOutput &output) const;
		void print(ConsoleOutput &output_global, ConsoleOutput &output_local) const;
		void printstatus(ConsoleOutput &output) const;
		void printstatus(std::ostringstream &output) const;
		void printtimer(ConsoleOutput &output) const;
		void printshort(std::ostringstream &header, std::ostringstream &values) const;
		void printshort_sum(std::ostringstream &header, std::ostringstream &values) const;
		std::string get_name() const;

};


}
} /* end of namespace */

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {
namespace solver {

template<class VectorBase>
void SPGQPSolver<VectorBase>::copy_to_gpu() {
#ifdef USE_GPU
//	BlockGraphSparseMatrix<PetscVector> *Abgs = dynamic_cast<BlockGraphSparseMatrix<PetscVector> *>(qpdata->get_A());
//	Mat A_Mat = Abgs->get_petscmatrix();

	GeneralVector<PetscVector> *b_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_b());
	Vec b_Vec = b_p->get_vector();

	GeneralVector<PetscVector> *x_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_x());
	Vec x_Vec = x_p->get_vector();

	GeneralVector<PetscVector> *x0_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_x0());
	Vec x0_Vec = x0_p->get_vector();

	GeneralVector<PetscVector> *g_p = dynamic_cast<GeneralVector<PetscVector> *>(this->g);
	Vec g_Vec = g_p->get_vector();

	GeneralVector<PetscVector> *d_p = dynamic_cast<GeneralVector<PetscVector> *>(this->d);
	Vec d_Vec = d_p->get_vector();

	GeneralVector<PetscVector> *Ad_p = dynamic_cast<GeneralVector<PetscVector> *>(this->Ad);
	Vec Ad_Vec = Ad_p->get_vector();

//	TRYCXX( MatCUDACopyToGPU(A_Mat) );
	TRYCXX( VecCUDACopyToGPU(b_Vec) );
	TRYCXX( VecCUDACopyToGPU(x_Vec) );
	TRYCXX( VecCUDACopyToGPU(x0_Vec) );
	TRYCXX( VecCUDACopyToGPU(g_Vec) );
	TRYCXX( VecCUDACopyToGPU(d_Vec) );
	TRYCXX( VecCUDACopyToGPU(Ad_Vec) );

#endif
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::set_settings_from_console() {
	consoleArg.set_option_value("spgqpsolver_maxit", &this->maxit, SPGQPSOLVER_DEFAULT_MAXIT);
	consoleArg.set_option_value("spgqpsolver_eps", &this->eps, SPGQPSOLVER_DEFAULT_EPS);
	
	consoleArg.set_option_value("spgqpsolver_m", &this->m, SPGQPSOLVER_DEFAULT_M);	
	consoleArg.set_option_value("spgqpsolver_gamma", &this->gamma, SPGQPSOLVER_DEFAULT_GAMMA);	
	consoleArg.set_option_value("spgqpsolver_sigma1", &this->sigma1, SPGQPSOLVER_DEFAULT_SIGMA1);	
	consoleArg.set_option_value("spgqpsolver_sigma2", &this->sigma2, SPGQPSOLVER_DEFAULT_SIGMA2);	
	consoleArg.set_option_value("spgqpsolver_alphainit", &this->alphainit, SPGQPSOLVER_DEFAULT_ALPHAINIT);	

	consoleArg.set_option_value("spgqpsolver_stop_normgp", &this->stop_normgp, SPGQPSOLVER_STOP_NORMGP);
	consoleArg.set_option_value("spgqpsolver_stop_Anormgp", &this->stop_Anormgp, SPGQPSOLVER_STOP_ANORMGP);
	consoleArg.set_option_value("spgqpsolver_stop_normgp_normb", &this->stop_normgp_normb, SPGQPSOLVER_STOP_NORMGP_NORMB);
	consoleArg.set_option_value("spgqpsolver_stop_Anormgp_normb", &this->stop_Anormgp_normb, SPGQPSOLVER_STOP_ANORMGP_NORMB);
	consoleArg.set_option_value("spgqpsolver_stop_difff", &this->stop_difff, SPGQPSOLVER_STOP_DIFFF);	

	consoleArg.set_option_value("spgqpsolver_dump", &this->dump_or_not, SPGQPSOLVER_DUMP);	
	consoleArg.set_option_value("spgqpsolver_monitor", &this->monitor, SPGQPSOLVER_MONITOR);	

	/* set debug mode */
	consoleArg.set_option_value("spgqpsolver_debugmode", &this->debugmode, SPGQPSOLVER_DEFAULT_DEBUGMODE);

	
	debug_print_vectors = false;
	debug_print_scalars = false; 
	debug_print_it = false; 

	if(debugmode == 1){
		debug_print_it = true;
	}

	if(debugmode == 2){
		debug_print_it = true;
		debug_print_scalars = true;
	}

	consoleArg.set_option_value("tssolver_debug_print_it",		&debug_print_it, 		debug_print_it);
	consoleArg.set_option_value("tssolver_debug_print_vectors", &debug_print_vectors,	false);
	consoleArg.set_option_value("tssolver_debug_print_scalars", &debug_print_scalars, 	debug_print_scalars);

}


/* ----- Solver ----- */
/* constructor */
template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver(){
	LOG_FUNC_BEGIN

	qpdata = NULL;
	
	/* temp vectors */
	this->g = NULL;
	this->d = NULL;
	this->Ad = NULL;
	this->temp = NULL;

	this->it_sum = 0;
	this->hessmult_sum = 0;
	this->it_last = 0;
	this->hessmult_last = 0;
	
	this->fx = std::numeric_limits<double>::max();
	this->gP = std::numeric_limits<double>::max();

	/* settings */
	set_settings_from_console();

	/* prepare timers */
	this->timer_solve.restart();	
	this->timer_projection.restart();
	this->timer_matmult.restart();
	this->timer_dot.restart();
	this->timer_update.restart();
	this->timer_stepsize.restart();
	this->timer_fs.restart();

	LOG_FUNC_END
}

template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver(QPData<VectorBase> &new_qpdata){
	LOG_FUNC_BEGIN

	qpdata = &new_qpdata;
	
	/* allocate temp vectors */
	allocate_temp_vectors();

	this->it_sum = 0;
	this->hessmult_sum = 0;
	this->it_last = 0;
	this->hessmult_last = 0;

	this->fx = std::numeric_limits<double>::max();
	this->gP = std::numeric_limits<double>::max();

	/* settings */
	set_settings_from_console();

	/* prepare timers */
	this->timer_projection.restart();
	this->timer_matmult.restart();
	this->timer_dot.restart();
	this->timer_update.restart();
	this->timer_stepsize.restart();
	this->timer_fs.restart();
	this->timer_solve.restart();	

	LOG_FUNC_END
}


/* destructor */
template<class VectorBase>
SPGQPSolver<VectorBase>::~SPGQPSolver(){
	LOG_FUNC_BEGIN

	/* free temp vectors */
	free_temp_vectors();

	LOG_FUNC_END
}

/* prepare temp_vectors */
template<class VectorBase>
void SPGQPSolver<VectorBase>::allocate_temp_vectors(){
	LOG_FUNC_BEGIN

	GeneralVector<VectorBase> *pattern = qpdata->get_b(); /* I will allocate temp vectors subject to linear term */

	g = new GeneralVector<VectorBase>(*pattern);
	d = new GeneralVector<VectorBase>(*pattern);
	Ad = new GeneralVector<VectorBase>(*pattern);	
	temp = new GeneralVector<VectorBase>(*pattern);	

	//TODO: prepare Mdot without petsc?
	/* prepare for Mdot */
	#ifdef USE_PETSCVECTOR
		/* without cuda */
//		#ifdef USE_CUDA
//			gpuErrchk( cudaMalloc((void **)&Mdots_val,3*sizeof(double)) );
//		#else
			TRYCXX( PetscMalloc1(3,&Mdots_val) );
//		#endif

		TRYCXX( PetscMalloc1(3,&Mdots_vec) );

		Mdots_vec[0] = d->get_vector();
		Mdots_vec[1] = Ad->get_vector();
		Mdots_vec[2] = g->get_vector();

	#endif
	
	LOG_FUNC_END
}

/* destroy temp_vectors */
template<class VectorBase>
void SPGQPSolver<VectorBase>::free_temp_vectors(){
	LOG_FUNC_BEGIN

	free(g);
	free(d);
	free(Ad);
	free(temp);
	
	#ifdef USE_PETSCVECTOR
//		#ifdef USE_CUDA
//			gpuErrchk( cudaFree(&Mdots_val) );
//			gpuErrchk( cudaFree(&Mdots_vec) );
//			TRYCXX( PetscFree(Mdots_vec) );
//		#else
			TRYCXX( PetscFree(Mdots_val) );
			TRYCXX( PetscFree(Mdots_vec) );
//		#endif
	#endif
	
	LOG_FUNC_END
}


/* print info about problem */
template<class VectorBase>
void SPGQPSolver<VectorBase>::print(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN

	output <<  this->get_name() << std::endl;
	
	/* print settings */
	output <<  " - maxit:      " << this->maxit << std::endl;
	output <<  " - eps:        " << this->eps << std::endl;
	output <<  " - debugmode: " << this->debugmode << std::endl;

	output <<  " - m:          " << m << std::endl;
	output <<  " - gamma:      " << gamma << std::endl;
	output <<  " - sigma1:     " << sigma1 << std::endl;
	output <<  " - sigma2:     " << sigma2 << std::endl;
	output <<  " - alphainit:  " << alphainit << std::endl;
	
	/* print data */
	if(qpdata){
		coutMaster.push();
		qpdata->print(output);
		coutMaster.pop();
	}

	output.synchronize();

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::print(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	LOG_FUNC_BEGIN

	output_global <<  this->get_name() << std::endl;
	
	/* print settings */
	output_local <<  " - maxit:      " << this->maxit << std::endl;
	output_local <<  " - eps:        " << this->eps << std::endl;
	output_local <<  " - debugmode: " << this->debugmode << std::endl;

	output_local <<  " - m:          " << m << std::endl;
	output_local <<  " - gamma:      " << gamma << std::endl;
	output_local <<  " - sigma1:     " << sigma1 << std::endl;
	output_local <<  " - sigma2:     " << sigma2 << std::endl;
	output_local <<  " - alphainit:  " << alphainit << std::endl;

	output_local.synchronize();
	
	/* print data */
	if(qpdata){
		coutMaster.push();
		qpdata->print(output_global, output_local);
		coutMaster.pop();
	}

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::printstatus(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN

	output <<  " - it: " << std::setw(6) << this->it_last << ", ";
	output <<  "hess mult: " << std::setw(6) << this->hessmult_last << ", ";
	output <<  "fx: " << std::setw(10) << this->fx << ", ";	
	output <<  "norm(gP): " << std::setw(10) << this->gP << ", ";
	output <<  "used memory: " << std::setw(6) << MemoryCheck::get_virtual() << "%" << std::endl;

	output << " - ";
//	output <<  "t_solve = " << std::setw(10) << this->timer_solve.get_value_last() << ", ";
	output <<  "t_project = " << std::setw(10) << this->timer_projection.get_value_last() << ", ";
	output <<  "t_matmult = " << std::setw(10) << this->timer_matmult.get_value_last() << ", ";
//	output <<  "t_dot = " << std::setw(10) << this->timer_dot.get_value_last() << ", ";
//	output <<  "t_update = " << std::setw(10) << this->timer_update.get_value_last() << ", ";
//	output <<  "t_stepsize = " << std::setw(10) << this->timer_stepsize.get_value_last() << ", ";
//	output <<  "t_fs = " << std::setw(10) << this->timer_fs.get_value_last() << ", ";
	output <<  "t_other = " << std::setw(10) << this->timer_solve.get_value_last() - (this->timer_projection.get_value_last() + this->timer_matmult.get_value_last() + this->timer_dot.get_value_last() + this->timer_update.get_value_last() + this->timer_stepsize.get_value_last() + this->timer_fs.get_value_last()) << std::endl;

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::printstatus(std::ostringstream &output) const {
	LOG_FUNC_BEGIN

	double fx_linear, fx_quadratic;

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to qpdata */
	pMatrix A = *(qpdata->get_A());
	pVector b = *(qpdata->get_b());

	/* pointer to solution */
	pVector x = *(qpdata->get_x());

	/* auxiliary vectors */
	pVector Ad = *(this->Ad); /* A*p */

	Ad = A*x;
	fx_quadratic = 0.5*dot(Ad,x);
	fx_linear = -dot(b,x);

	std::streamsize ss = std::cout.precision();

	output << std::setprecision(17);
	output <<  "      - fx:           " << std::setw(25) << this->fx << std::endl;
	output <<  "      - fx_control:   " << std::setw(25) << fx_quadratic+fx_linear << ", log: " << std::setw(25) << log(fx_quadratic+fx_linear)/log(10) << std::endl;
	output <<  "      - fx_linear:    " << std::setw(25) << fx_linear << ", log: " << std::setw(25) << log(fx_linear)/log(10) << std::endl;
	output <<  "      - fx_quadratic: " << std::setw(25) << fx_quadratic << ", log: " << std::setw(25) << log(fx_quadratic)/log(10) << std::endl;
	output <<  "      - norm(gP):     " << std::setw(25) << this->gP << ", log: " << std::setw(25) << log(this->gP)/log(10) << std::endl;
	output << std::setprecision(ss);

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::printtimer(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN

	output <<  this->get_name() << std::endl;
	output <<  " - it all =        " << this->it_sum << std::endl;
	output <<  " - hessmult all =  " << this->hessmult_sum << std::endl;
	output <<  " - timers" << std::endl;
	output <<  "  - t_solve =      " << this->timer_solve.get_value_sum() << std::endl;
	output <<  "  - t_project =    " << this->timer_projection.get_value_sum() << std::endl;
	output <<  "  - t_matmult =    " << this->timer_matmult.get_value_sum() << std::endl;
	output <<  "  - t_dot =        " << this->timer_dot.get_value_sum() << std::endl;
	output <<  "  - t_update =     " << this->timer_update.get_value_sum() << std::endl;
	output <<  "  - t_stepsize =   " << this->timer_stepsize.get_value_sum() << std::endl;
	output <<  "  - t_fs =         " << this->timer_fs.get_value_sum() << std::endl;
	output <<  "  - t_other =      " << this->timer_solve.get_value_sum() - (this->timer_projection.get_value_sum() + this->timer_matmult.get_value_sum() + this->timer_dot.get_value_sum() + this->timer_update.get_value_sum() + this->timer_stepsize.get_value_sum() + this->timer_fs.get_value_sum()) << std::endl;

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::printshort(std::ostringstream &header, std::ostringstream &values) const {
	LOG_FUNC_BEGIN

	double fx_linear, fx_quadratic;

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to qpdata */
	pMatrix A = *(qpdata->get_A());

	pVector b = *(qpdata->get_b());

	/* pointer to solution */
	pVector x = *(qpdata->get_x());

	/* auxiliary vectors */
	pVector Ad = *(this->Ad); /* A*p */

	Ad = A*x;
	fx_quadratic = 0.5*dot(Ad,x);
	fx_linear = -dot(b,x);
	std::streamsize ss = std::cout.precision();

	values << std::setprecision(17);

	header << "SPGQP it, ";
	values << this->it_last << ", ";

	header << "SPGQP hessmult, ";
	values << this->hessmult_last << ", ";

	header << "SPGQP t all, ";
	values << this->timer_solve.get_value_last() << ", ";

	header << "SPGQP t project, ";
	values << this->timer_projection.get_value_last() << ", ";

	header << "SPGQP t matmult, ";
	values << this->timer_matmult.get_value_last() << ", ";

	header << "SPGQP t dot, ";
	values << this->timer_dot.get_value_last() << ", ";

	header << "SPGQP t update, ";
	values << this->timer_update.get_value_last() << ", ";

	header << "SPGQP t stepsize, ";
	values << this->timer_stepsize.get_value_last() << ", ";

	header << "SPGQP t fs, ";
	values << this->timer_fs.get_value_last() << ", ";

	header << "SPGQP t other, ";
	values << this->timer_solve.get_value_last() - (this->timer_projection.get_value_last() + this->timer_matmult.get_value_last() + this->timer_dot.get_value_last() + this->timer_update.get_value_last() + this->timer_stepsize.get_value_last() + this->timer_fs.get_value_last()) << ", ";

	header << "SPGQP fx, ";
	values << this->fx << ", ";

	header << "SPGQP fx_linear, ";
	values << fx_linear << ", ";

	header << "SPGQP fx_quadratic, ";
	values << fx_quadratic << ", ";

	values << std::setprecision(ss);

	LOG_FUNC_END
}

template<class VectorBase>
void SPGQPSolver<VectorBase>::printshort_sum(std::ostringstream &header, std::ostringstream &values) const {
	LOG_FUNC_BEGIN

	header << "SPGQP_sum it, ";
	values << this->it_sum << ", ";

	header << "SPGQP_sum hessmult, ";
	values << this->hessmult_sum << ", ";

	header << "SPGQP_sum t all, ";
	values << this->timer_solve.get_value_sum() << ", ";

	header << "SPGQP_sum t project, ";
	values << this->timer_projection.get_value_sum() << ", ";

	header << "SPGQP_sum t matmult, ";
	values << this->timer_matmult.get_value_sum() << ", ";

	header << "SPGQP_sum t dot, ";
	values << this->timer_dot.get_value_sum() << ", ";

	header << "SPGQP_sum t update, ";
	values << this->timer_update.get_value_sum() << ", ";

	header << "SPGQP_sum t stepsize, ";
	values << this->timer_stepsize.get_value_sum() << ", ";

	header << "SPGQP_sum t fs, ";
	values << this->timer_fs.get_value_sum() << ", ";

	header << "SPGQP_sum t other, ";
	values << this->timer_solve.get_value_sum() - (this->timer_projection.get_value_sum() + this->timer_matmult.get_value_sum() + this->timer_dot.get_value_sum() + this->timer_update.get_value_sum() + this->timer_stepsize.get_value_sum() + this->timer_fs.get_value_sum()) << ", ";

	LOG_FUNC_END
}

template<class VectorBase>
std::string SPGQPSolver<VectorBase>::get_name() const {
	return "SPGQP";
}

/* solve the problem */
template<class VectorBase>
void SPGQPSolver<VectorBase>::solve() {
	LOG_FUNC_BEGIN

	copy_to_gpu();

	this->timer_solve.start(); /* stop this timer in the end of solution */

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
//	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to qpdata */
//	pMatrix A = *(qpdata->get_A());
//	pVector b = *(qpdata->get_b());
//	pVector x0 = *(qpdata->get_x0());

	/* pointer to solution */
	pVector x = *(qpdata->get_x());

	/* auxiliary vectors */
//	pVector g = *(this->g); /* gradient */
	pVector d = *(this->d); /* A-conjugate vector */
//	pVector Ad = *(this->Ad); /* A*p */

	//TODO: temp!!!
		BlockGraphSparseMatrix<PetscVector> *Abgs = dynamic_cast<BlockGraphSparseMatrix<PetscVector> *>(qpdata->get_A());
		GeneralVector<PetscVector> *b_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_b());
		GeneralVector<PetscVector> *x_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_x());
		GeneralVector<PetscVector> *x0_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_x0());
		GeneralVector<PetscVector> *g_p = dynamic_cast<GeneralVector<PetscVector> *>(this->g);
		GeneralVector<PetscVector> *d_p = dynamic_cast<GeneralVector<PetscVector> *>(this->d);
		GeneralVector<PetscVector> *Ad_p = dynamic_cast<GeneralVector<PetscVector> *>(this->Ad);

		Mat A_Mat = Abgs->get_petscmatrix(); 
		Vec b_Vec = b_p->get_vector();
		Vec x_Vec = x_p->get_vector();
		Vec x0_Vec = x0_p->get_vector();
		Vec g_Vec = g_p->get_vector();
		Vec d_Vec = d_p->get_vector();
		Vec Ad_Vec = Ad_p->get_vector();


	int it = 0; /* number of iterations */
	int hessmult = 0; /* number of hessian multiplications */

	double fx; /* function value */
	double fx_old; /* f(x_{it - 1}) */
	SPGQPSolver_fs fs(this->m); /* store function values for generalized Armijo condition */
	double fx_max; /* max(fs) */
	double xi, beta_bar, beta_hat, beta; /* for Armijo condition */
	double dd; /* dot(d,d) */
	double gd; /* dot(g,d) */
	double dAd; /* dot(Ad,d) */
	double alpha_bb; /* BB step-size */
	double normb;// = norm(b); /* norm of linear term used in stopping criteria */
	//TODO: temp!!!
		TRYCXX( VecNorm(b_Vec, NORM_2, &normb) );

	/* initial step-size */
	alpha_bb = this->alphainit;

	//TODO: temp!
//	x = x0; /* set approximation as initial */
	TRYCXX( VecCopy(x0_Vec, x0_Vec) );

	this->timer_projection.start();
	 qpdata->get_feasibleset()->project(x); /* project initial approximation to feasible set */
	this->timer_projection.stop();

	/* compute gradient, g = A*x-b */
	this->timer_matmult.start();
	//TODO: temp!!!
	// g = A*x;
		 TRYCXX( MatMult(A_Mat, x_Vec, g_Vec) );
		 TRYCXX( VecScale(g_Vec, Abgs->get_coeff()) );

	 hessmult += 1; /* there was muliplication by A */
	this->timer_matmult.stop();

	//TODO: temp!!!
	//g -= b;
		 TRYCXX( VecAXPY(g_Vec, -1.0, b_Vec) );

	/* initialize fs */
	this->timer_fs.start();
	 fx = get_fx();
	 fx_old = std::numeric_limits<double>::max();
	 this->fx = fx;
	 fs.init(fx);
	this->timer_fs.stop();

	/* main cycle */
	while(it < this->maxit){
		/* increase iteration counter */
		it += 1;

		// TODO: TEMP
//		coutMaster << "x.type  : " << x.get_type() << std::endl;
//		coutMaster << "d.type  : " << d.get_type() << std::endl;
//		coutMaster << "Ad.type : " << Ad.get_type() << std::endl;
//		coutMaster << "g.type  : " << g.get_type() << std::endl;

		/* d = x - alpha_bb*g, see next step, it will be d = P(x - alpha_bb*g) - x */
		this->timer_update.start();
		//d = x - alpha_bb*g;
		 TRYCXX( VecCopy(x_Vec, d_Vec));
		 TRYCXX( VecAXPY(d_Vec, -alpha_bb, g_Vec) );
		 
		this->timer_update.stop();

		/* d = P(d) */
		this->timer_projection.start();
		 qpdata->get_feasibleset()->project(d);
		this->timer_projection.stop();

		/* d = d - x */
		this->timer_update.start();
		 //TODO: temp!!!
		 //d -= x;
		 TRYCXX( VecAXPY(d_Vec, -1.0, x_Vec) );

		this->timer_update.stop();

		/* Ad = A*d */
		this->timer_matmult.start();
		//TODO: temp!!!
		// Ad = A*d;
		 TRYCXX( MatMult(A_Mat, d_Vec, Ad_Vec) );
		 TRYCXX( VecScale(Ad_Vec, Abgs->get_coeff()) );

		 hessmult += 1;
		this->timer_matmult.stop();

		this->timer_dot.start();
//		  dd = dot(d,d);
//		  dAd = dot(Ad,d); 
//		  gd = dot(g,d);
		 compute_dots(&dd, &dAd, &gd);
		this->timer_dot.stop();

		/* fx_max = max(fs) */
		this->timer_fs.start();
		 fx_max = fs.get_max();
		this->timer_fs.stop();
		
		/* compute step-size from A-condition */
		this->timer_stepsize.start();
		 xi = (fx_max - fx)/dAd;
		 beta_bar = -gd/dAd;
		 beta_hat = this->gamma*beta_bar + PetscSqrtReal(this->gamma*this->gamma*beta_bar*beta_bar + 2*xi);

		 /* beta = max(sigma1,min(sigma2,beta_hat)) */
		 if(beta_hat < this->sigma1){
			 beta_hat = this->sigma1;
		 }
		 
		 if(beta_hat < this->sigma2){
			beta = beta_hat;
		 } else {
			beta = this->sigma2;
		 }
		this->timer_stepsize.stop();

		/* update approximation and gradient */
		this->timer_update.start();
		//TODO: temp!!!
		 TRYCXX( VecAXPY(x_Vec, beta, d_Vec) );
		 TRYCXX( VecAXPY(g_Vec, beta, Ad_Vec) );
//		 x += beta*d; /* x = x + beta*d */
//		 g += beta*Ad; /* g = g + beta*Ad */
		this->timer_update.stop();

		/* compute new function value using gradient and update fs list */
		this->timer_fs.start();
		 fx_old = fx;
//		 fx = get_fx(fx_old,beta,gd,dAd);
		 fx = get_fx();
		 fs.update(fx);
		this->timer_fs.stop();

		/* update BB step-size */
		this->timer_stepsize.start();
		 alpha_bb = dd/dAd;
		this->timer_stepsize.stop();

		this->gP = dd;

		/* print progress of algorithm */
		if(debug_print_it){
			coutMaster << "\033[33m   it = \033[0m" << it;
			
			std::streamsize ss = std::cout.precision();
			coutMaster << ", \t\033[36mfx = \033[0m" << std::setprecision(17) << fx << std::setprecision(ss);

			coutMaster << ", \t\033[36mgP = \033[0m" << this->gP;
			coutMaster << ", \t\033[36mdd = \033[0m" << dd << std::endl;

			/* log function value */
			LOG_FX(fx)

		}
		
		/* print qpdata */
		if(debug_print_vectors){
//			coutMaster << "x: " << x << std::endl;
//			coutMaster << "d: " << d << std::endl;
//			coutMaster << "g: " << g << std::endl;
//			coutMaster << "Ad: " << Ad << std::endl;
		}

		if(debug_print_scalars){
			coutMaster << "\033[36m    alpha_bb = \033[0m" << alpha_bb << ",";
			coutMaster << "\033[36m dAd = \033[0m" << dAd << ",";
			coutMaster << "\033[36m gd = \033[0m" << gd << std::endl;
			
			coutMaster << "\033[36m    fx = \033[0m" << fx << ",";
			coutMaster << "\033[36m fx_max = \033[0m" << fx_max << ",";
			coutMaster << "\033[36m xi = \033[0m" << xi << std::endl;
			
			coutMaster << "\033[36m    beta_bar = \033[0m" << beta_bar << ",";
			coutMaster << "\033[36m beta_hat = \033[0m" << beta_hat << ",";
			coutMaster << "\033[36m beta = \033[0m" << beta << std::endl;
			
		}

		/* stopping criteria */
		if( this->stop_difff && abs(fx - fx_old) < this->eps){
			break;
		}
		if(this->stop_normgp && dd < this->eps){
			break;
		}
		if(this->stop_normgp_normb && dd < this->eps*normb){
			break;
		}
		if(this->stop_Anormgp && dAd < this->eps){
			break;
		}
		if(this->stop_Anormgp_normb && dAd < this->eps*normb){
			break;
		}
		
		/* monitor - export values of stopping criteria */
		if(this->monitor && GlobalManager.get_rank() == 0){
			//TODO: this could be done in a different way
			std::ofstream myfile;
			myfile.open("log/spgqpsolver_monitor.m", std::fstream::in | std::fstream::out | std::fstream::app);

			std::streamsize ss = myfile.precision();
			myfile << std::setprecision(17);
			
			myfile << "fx(" << it << ") = " << fx << "; "; 
			myfile << "alpha_bb(" << it << ") = " << alpha_bb << "; ";
			myfile << "beta(" << it << ") = " << beta << "; ";
			myfile << "norm_difff(" << it << ") = " << abs(fx - fx_old) << "; ";
			myfile << "norm_gp(" << it << ") = " << dd << "; ";
			myfile << "norm_Agp(" << it << ") = " << dAd << "; ";
			myfile << std::endl;
			
			myfile << std::setprecision(ss);
			myfile.close();			
		}
		
	} /* main cycle end */

	this->it_sum += it;
	this->hessmult_sum += hessmult;
	this->it_last = it;
	this->hessmult_last = hessmult;

	this->fx = fx;
	this->timer_solve.stop();

	/* write info to log file */
	LOG_IT(it)
	LOG_FX(fx)

	LOG_FUNC_END
}

/* compute function value using inner *x and already computed *g */
template<class VectorBase>
double SPGQPSolver<VectorBase>::get_fx() const {
	LOG_FUNC_BEGIN
	
	double fx = std::numeric_limits<double>::max();

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
//	typedef GeneralVector<VectorBase> (&pVector);

	/* pointers to qpdata */
//	pVector g = *(this->g);
//	pVector x = *(qpdata->get_x());
//	pVector b = *(qpdata->get_b());
//	pVector temp = *(this->temp);

	/* use computed gradient in this->g to compute function value */
	//TODO: temp!!!
//	temp = g - b;
	double tempt; // = dot(temp,x);


		GeneralVector<PetscVector> *b_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_b());
		GeneralVector<PetscVector> *x_p = dynamic_cast<GeneralVector<PetscVector> *>(qpdata->get_x());
		GeneralVector<PetscVector> *g_p = dynamic_cast<GeneralVector<PetscVector> *>(this->g);
		GeneralVector<PetscVector> *temp_p = dynamic_cast<GeneralVector<PetscVector> *>(this->temp);

		Vec b_Vec = b_p->get_vector();
		Vec x_Vec = x_p->get_vector();
		Vec g_Vec = g_p->get_vector();
		Vec temp_Vec = temp_p->get_vector();

		TRYCXX( VecCopy(g_Vec,temp_Vec) );
		TRYCXX( VecAXPY(temp_Vec, -1.0, b_Vec));
		TRYCXX( VecDot(x_Vec,temp_Vec,&tempt));

	fx = 0.5*tempt;

	LOG_FUNC_END
	return fx;	
}

/* compute function value using previously computed values */
template<class VectorBase>
double SPGQPSolver<VectorBase>::get_fx(double fx_old, double beta, double gd, double dAd) const {
	LOG_FUNC_BEGIN
	
	double fx = fx_old + beta*(gd + 0.5*beta*dAd);

	LOG_FUNC_END
	return fx;	
}

/* compute dot products */
template<class VectorBase>
void SPGQPSolver<VectorBase>::compute_dots(double *dd, double *dAd, double *gd) const {
	LOG_FUNC_BEGIN

	*dd = dot(*d,*d);
	*dAd = dot(*Ad,*d);
	*gd = dot(*g,*d);

	LOG_FUNC_END
}

/* ---------- SPGQPSolver_fs -------------- */

/* constructor */
template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver_fs::SPGQPSolver_fs(int new_m){
	this->m = new_m;
	this->fs_list = (double*)malloc(this->m*sizeof(double));
}

template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver_fs::~SPGQPSolver_fs(){
	free(this->fs_list);
}

/* init the list with function values using one initial fx */
template<class VectorBase>
void SPGQPSolver<VectorBase>::SPGQPSolver_fs::init(double fx){
	LOG_FUNC_BEGIN

	for(int i=0; i<this->m;i++){
		this->fs_list[i] = fx;
	}
	this->last_idx = 0;

	LOG_FUNC_END
}

/* get the size of the list */
template<class VectorBase>
int SPGQPSolver<VectorBase>::SPGQPSolver_fs::get_size(){
	return this->m;
}

/* get the value of max value in the list */
template<class VectorBase>
double SPGQPSolver<VectorBase>::SPGQPSolver_fs::get_max(){
	LOG_FUNC_BEGIN
	
	int max_idx = 0;
	double max_value = this->fs_list[max_idx];
	for(int i=0;i<this->m;i++){
		if(this->fs_list[i] > max_value){
			max_idx = i;
			max_value = this->fs_list[max_idx];
		}
	}

	LOG_FUNC_END

	return max_value;
}

/* update the list by new value - pop the first and push the new value (FIFO) */
template<class VectorBase>
void SPGQPSolver<VectorBase>::SPGQPSolver_fs::update(double new_fx){
	LOG_FUNC_BEGIN

	this->last_idx++;
	if(this->last_idx >= this->m){
		this->last_idx = 0;
	}
	
	this->fs_list[this->last_idx] = new_fx;

	LOG_FUNC_END
}

/* print the content of the list */
template<class VectorBase>
void SPGQPSolver<VectorBase>::SPGQPSolver_fs::print(ConsoleOutput &output)
{
	output << "[ ";
	/* for each component go throught the list */
	for(int i=this->last_idx;i<this->last_idx+this->m;i++){
		if(i < this->m){
			output << this->fs_list[i];
		} else {
			output << this->fs_list[i - this->m];
		}
		
		if(i < this->last_idx+this->m-1){ /* this is not the last node */
				output << ", ";
		}
	}
	output << " ]";
}


/* ----------- PETSC special stuff ------------- */
#ifdef USE_PETSC
/* compute dot products */
template<>
void SPGQPSolver<PetscVector>::compute_dots(double *dd, double *dAd, double *gd) const {
	LOG_FUNC_BEGIN

	//TODO: temp
//	const char *mytype1;
//	const char *mytype2;
//	const char *mytype3;
//	TRYCXX( VecGetType(Mdots_vec[0], &mytype1) );
//	TRYCXX( VecGetType(Mdots_vec[1], &mytype2) );
//	TRYCXX( VecGetType(Mdots_vec[2], &mytype3) );

//	coutMaster << "typeofvector1: " << mytype1 << std::endl;
//	coutMaster << "typeofvector2: " << mytype2 << std::endl;
//	coutMaster << "typeofvector3: " << mytype3 << std::endl;

#ifdef USE_CUDA
	//TODO: hotfix
//	TRYCXX( VecCUDACopyToGPU(Mdots_vec[0]) );
//	TRYCXX( VecCUDACopyToGPU(Mdots_vec[1]) );
//	TRYCXX( VecCUDACopyToGPU(Mdots_vec[2]) );

	TRYCXX( VecMDot( Mdots_vec[0], 3, Mdots_vec, Mdots_val) );

	*dd = Mdots_val[0];
	*dAd = Mdots_val[1];
	*gd = Mdots_val[2];

//	VecDot(Mdots_vec[0],Mdots_vec[0], dd);
//	VecDot(Mdots_vec[0],Mdots_vec[1], dAd);
//	VecDot(Mdots_vec[0],Mdots_vec[2], gd);
#else
	TRYCXX( VecMDot( Mdots_vec[0], 3, Mdots_vec, Mdots_val) );

	*dd = Mdots_val[0];
	*dAd = Mdots_val[1];
	*gd = Mdots_val[2];

#endif


	LOG_FUNC_END
}
#endif


}
} /* end namespace */

#endif
