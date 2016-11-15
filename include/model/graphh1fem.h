#ifndef PASC_GRAPHH1FEMMODEL_H
#define PASC_GRAPHH1FEMMODEL_H

#ifndef USE_PETSCVECTOR
 #error 'GRAPHH1FEMMODEL is for PETSCVECTOR'
#endif

typedef petscvector::PetscVector PetscVector;

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include "pascinference.h"

/* gamma problem */
//#include "algebra/matrix/blockgraphfree.h" // TODO: implement?
#include "algebra/matrix/blockgraphsparse.h"

#include "algebra/feasibleset/simplex_local.h"
//#include "solver/spgqpsolver.h"
#include "solver/spgqpsolver_coeff.h"
#include "data/qpdata.h"

/* theta problem */
#include "solver/simplesolver.h"
#include "data/simpledata.h"

#include "data/tsdata.h"

/* default type of matrix: 0=FREE, 1=SPARSE */
#define GRAPHH1FEMMODEL_DEFAULT_MATRIX_TYPE 1


namespace pascinference {
namespace model {

/** \class GraphH1FEMModel
 *  \brief time-series model with quadratic penalty time-space regularisation.
 *
*/
template<class VectorBase>
class GraphH1FEMModel: public TSModel<VectorBase> {
	public:
		/** @brief type of solver used to solve inner gamma problem 
		 * 
		 * The inner problem leads to QP with equality constraints and bound constraints
		 * 
		 */
		typedef enum { 
			SOLVER_AUTO=0, /**< choose automatic solver */
			SOLVER_SPGQP=1, /**< CPU/GPU implementation of Spectral Projected Gradient method */
			SOLVER_PERMON=2 /**< PERMONQP solver (augumented lagrangians combined with active-set method) */
		} GammaSolverType;

	protected:
		QPData<VectorBase> *gammadata; /**< QP with simplex, will be solved by SPG-QP */
	 	SimpleData<VectorBase> *thetadata; /**< this problem is solved during assembly  */

		double epssqr; /**< penalty coeficient */
		
		/* for theta problem */
		GeneralMatrix<VectorBase> *A_shared; /**< matrix shared by gamma and theta solver */
		GeneralVector<VectorBase> *Agamma; /**< temp vector for storing A_shared*gamma */

		bool usethetainpenalty; /**< use the value of Theta in penalty parameter to scale blocks */
		
		GammaSolverType gammasolvertype; /**< the type of used solver */
	public:

		/** @brief constructor from data and regularisation constant
		 * 
		 * @param tsdata time-series data on which model operates
		 * @param epssqr regularisation constant
		 */ 	
		GraphH1FEMModel(TSData<VectorBase> &tsdata, double epssqr, bool usethetainpenalty = false);

		/** @brief destructor 
		 */ 
		~GraphH1FEMModel();

		/** @brief print info about model
		 * 
		 * @param output where to print
		 */	
		void print(ConsoleOutput &output) const;

		/** @brief print info about model
		 * 
		 * @param output_global where to print global info
		 * @param output_local where to print local info
		 */	
		void print(ConsoleOutput &output_global, ConsoleOutput &output_local) const;

		/** @brief print solution of the model
		 * 
		 * @param output_global where to print global part
		 * @param output_local where to print local part
		 */	
		void printsolution(ConsoleOutput &output_global, ConsoleOutput &output_local) const;
		
		std::string get_name() const;
		
		void initialize_gammasolver(GeneralSolver **gamma_solver);
		void update_gammasolver(GeneralSolver *gamma_solver);
		void finalize_gammasolver(GeneralSolver **gamma_solver);

		void initialize_thetasolver(GeneralSolver **theta_solver);
		void update_thetasolver(GeneralSolver *theta_solver);
		void finalize_thetasolver(GeneralSolver **theta_solver);
	
		double get_L(GeneralSolver *gammasolver, GeneralSolver *thetasolver);
		void get_linear_quadratic(double *linearL, double *quadraticL, GeneralSolver *gammasolver, GeneralSolver *thetasolver);
		
		QPData<VectorBase> *get_gammadata() const;
		SimpleData<VectorBase> *get_thetadata() const;
		BGMGraph *get_graph() const;

		GeneralVector<VectorBase> *get_coordinatesVTK() const;
		int get_coordinatesVTK_dim() const;

		double get_aic(double L) const;
		
		void set_epssqr(double epssqr);
		bool get_usethetainpenalty() const;
		
};


}
} /* end of namespace */


/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {
namespace model {

/* constructor */
template<>
GraphH1FEMModel<PetscVector>::GraphH1FEMModel(TSData<PetscVector> &new_tsdata, double epssqr, bool usethetainpenalty) {
	LOG_FUNC_BEGIN

	// TODO: enum in boost::program_options, not only int
	int gammasolvertype_int;
	consoleArg.set_option_value("graphh1femmodel_gammasolvertype", &gammasolvertype_int, 0);
	this->gammasolvertype = static_cast<GammaSolverType>(gammasolvertype_int);

	/* set given parameters */
	this->usethetainpenalty = usethetainpenalty;
	this->tsdata = &new_tsdata;
	this->epssqr = epssqr;

	/* prepare sequential vector with Theta - yes, all procesors will have the same information */
	this->thetavectorlength_local = tsdata->get_K()*tsdata->get_xdim();
	this->thetavectorlength_global = GlobalManager.get_size()*(this->thetavectorlength_local);

	/* set this model to data - tsdata will prepare gamma vector and thetavector */
	tsdata->set_model(*this);

	/* control the existence of graph */
	/* in this model, I need to work with graph, but maybe there is no graph in decomposition
	 * (for instance in the case without spatial decomposition), however it seems that user still wants to work 
	 * with this model based on graph. Therefore we create graph of disjoint nodes without any edge. This graph
	 * represents the situation and can be used for matrix-graph-based manipulation
	*/
	if(get_graph() == NULL){
		BGMGraph *graph = this->tsdata->get_decomposition()->get_graph();
		double coordinates_array[this->tsdata->get_R()];

		for(int r=0;r<this->tsdata->get_R();r++){
			coordinates_array[r] = r;
		} 
		graph = new BGMGraph(coordinates_array, this->tsdata->get_R(), 1);
		graph->process(0.0);
		
		this->tsdata->get_decomposition()->set_graph(*graph);
	}
	
	LOG_FUNC_END
}

/* destructor */
template<class VectorBase>
GraphH1FEMModel<VectorBase>::~GraphH1FEMModel(){
	LOG_FUNC_BEGIN
	
	/* destroy auxiliary vectors */
	
	LOG_FUNC_END
}


/* print info about model */
template<class VectorBase>
void GraphH1FEMModel<VectorBase>::print(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN

	output <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output <<  " - T                 : " << this->tsdata->get_T() << std::endl;
	output <<  " - xdim              : " << this->tsdata->get_xdim() << std::endl;

	output <<  " - K                 : " << this->tsdata->get_K() << std::endl;
	output <<  " - R                 : " << this->tsdata->get_R() << std::endl;
	output <<  " - epssqr            : " << this->epssqr << std::endl;
	output <<  " - usethetainpenalty : " << printbool(this->usethetainpenalty) << std::endl; 

	output <<  " - Graph             : " << std::endl;
	output.push();
	this->get_graph()->print(output);
	output.pop();

	output <<  " - thetalength: " << this->thetavectorlength_global << std::endl;

	output <<  " - gammasolvertype   : ";
	switch(this->gammasolvertype){
		case(SOLVER_AUTO): output << "AUTO"; break;
		case(SOLVER_SPGQP): output << "SPG-QP solver"; break;
		case(SOLVER_PERMON): output << "PERMON QP solver"; break;
	}
	output << std::endl;
	
	output.synchronize();	

	LOG_FUNC_END
}

/* print info about model */
template<class VectorBase>
void GraphH1FEMModel<VectorBase>::print(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	LOG_FUNC_BEGIN

	output_global <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output_global <<  " - global info" << std::endl;
	output_global <<  "  - T                 : " << this->tsdata->get_T() << std::endl;
	output_global <<  "  - xdim              : " << this->tsdata->get_xdim() << std::endl;
	output_global <<  "  - K                 : " << this->tsdata->get_K() << std::endl;
	output_global <<  "  - R                 : " << this->tsdata->get_R() << std::endl;
	output_global <<  "  - epssqr            : " << this->epssqr << std::endl;
	output_global <<  "  - usethetainpenalty : " << printbool(this->usethetainpenalty) << std::endl; 
	output_global <<  "  - gammasolvertype   : ";
	switch(this->gammasolvertype){
		case(SOLVER_AUTO): output_global << "AUTO"; break;
		case(SOLVER_SPGQP): output_global << "SPG-QP solver"; break;
		case(SOLVER_PERMON): output_global << "PERMON QP solver"; break;
	}
	output_global << std::endl;

	output_global.push();
	this->get_graph()->print(output_global);
	output_global.pop();

	output_global <<  "  - thetalength       : " << this->thetavectorlength_global << std::endl;

	/* give local info */
	output_global <<  " - local variables" << std::endl;
	output_global.push();
	output_local << "Tlocal =" << std::setw(6) << this->tsdata->get_Tlocal() << " (" << this->tsdata->get_Tbegin() << "," << this->tsdata->get_Tend() << "), ";
	output_local << "Rlocal =" << std::setw(6) << this->tsdata->get_Rlocal() << " (" << this->tsdata->get_Rbegin() << "," << this->tsdata->get_Rend() << "), ";
	output_local << "thetalength=" << std::setw(6) << this->thetavectorlength_local << std::endl;

	output_global.pop();
	output_local.synchronize();

	output_global.synchronize();

	LOG_FUNC_END
}

/* print model solution */
template<>
void GraphH1FEMModel<PetscVector>::printsolution(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	LOG_FUNC_BEGIN

	output_global <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output_global <<  "theta:" << std::endl;

	std::ostringstream temp;
	
	double *theta;
	TRYCXX( VecGetArray(thetadata->get_x()->get_vector(),&theta) );
	
	int k,n;

	output_local.push();
	output_local << "- proc: " << GlobalManager.get_rank() << std::endl;
	output_local.push();
	for(k=0;k<tsdata->get_K();k++){
		output_local <<  "- k = " << k << std::endl;

		/* mu */
		output_local.push();
		output_local <<  "- theta = [";
		for(n=0;n<tsdata->get_xdim();n++){
			temp << theta[k*tsdata->get_xdim() + n];
			output_local << temp.str();
			if(n < tsdata->get_xdim()-1){
				output_local << ", ";
			}
			temp.str("");
		}
		output_local <<  "]" << std::endl;
		output_local.pop();

	}
	output_local.pop();
	output_local.pop();

	output_local.synchronize();
	output_global.synchronize();

	LOG_FUNC_END
}

/* get name of the model */
template<class VectorBase>
std::string GraphH1FEMModel<VectorBase>::get_name() const {
	return "Graph-H1-FEM Time-Series Model";	
}

/* set new penalty */
template<class VectorBase>
void GraphH1FEMModel<VectorBase>::set_epssqr(double epssqr) {
	this->epssqr = epssqr;

	if(this->A_shared){
		/* SPARSE */
		((BlockGraphSparseMatrix<VectorBase>*)A_shared)->set_coeff(this->epssqr);
	}
}

/* prepare gamma solver */
template<>
void GraphH1FEMModel<PetscVector>::initialize_gammasolver(GeneralSolver **gammasolver){
	LOG_FUNC_BEGIN

	/* create data */
	gammadata = new QPData<PetscVector>();
	
	/* automatic choice of solver */
	if(this->gammasolvertype == SOLVER_AUTO){
		this->gammasolvertype = SOLVER_SPGQP;
	}
	
	/* SPG-QP solver */
	if(this->gammasolvertype == SOLVER_SPGQP){
		gammadata->set_x0(tsdata->get_gammavector()); /* the initial approximation of QP problem is gammavector */
		gammadata->set_x(tsdata->get_gammavector()); /* the solution of QP problem is gamma */
		gammadata->set_b(new GeneralVector<PetscVector>(*gammadata->get_x0())); /* create new linear term of QP problem */

//		double coeff = (1.0/((double)(this->tsdata->get_R()*this->tsdata->get_T())))*this->epssqr;
//		double coeff = (1.0/(sqrt((double)(this->tsdata->get_R()*this->tsdata->get_T()))))*this->epssqr;
		double coeff = this->epssqr;
//		double coeff = sqrt((double)(this->tsdata->get_R()*this->tsdata->get_T()))*this->epssqr;
//		double coeff = this->tsdata->get_R()*this->tsdata->get_T()*this->epssqr;

		/* SPARSE */
		if(usethetainpenalty){
			/* use thetavector as a vector of coefficient for scaling blocks */
			A_shared = new BlockGraphSparseMatrix<PetscVector>(*(tsdata->get_decomposition()), coeff, tsdata->get_thetavector() );
		} else {
			/* the vector of coefficient of blocks is set to NULL, therefore Theta will be not used to scale in penalisation */
			A_shared = new BlockGraphSparseMatrix<PetscVector>(*(tsdata->get_decomposition()), coeff, NULL );
		}

		gammadata->set_A(A_shared); 
		gammadata->set_feasibleset(new SimplexFeasibleSet_Local<PetscVector>(tsdata->get_Tlocal()*tsdata->get_Rlocal(),tsdata->get_K())); /* the feasible set of QP is simplex */ 	

		/* create solver */
		*gammasolver = new SPGQPSolverC<PetscVector>(*gammadata);

		/* generate random data to gamma */
		gammadata->get_x0()->set_random();

		/* project random values to feasible set to be sure that initial approximation is feasible */
		gammadata->get_feasibleset()->project(*gammadata->get_x0());

		/* set stopping criteria based on the size of x (i.e. gamma) */
//		(*gammasolver)->set_eps((double)(this->tsdata->get_R()*this->tsdata->get_T())*(*gammasolver)->get_eps());
	}

	/* SPG-QP solver */
	if(this->gammasolvertype == SOLVER_PERMON){
		
	}



	LOG_FUNC_END
}

/* prepare theta solver */
template<>
void GraphH1FEMModel<PetscVector>::initialize_thetasolver(GeneralSolver **thetasolver){
	LOG_FUNC_BEGIN
	
	/* create data */
	thetadata = new SimpleData<PetscVector>();
	thetadata->set_x(tsdata->get_thetavector());

	/* create solver */
	*thetasolver = new SimpleSolver<PetscVector>(*thetadata);
	
	/* create aux vector for gamma^T A gamma */
	Vec Agamma_Vec;
	TRYCXX( VecDuplicate(tsdata->get_gammavector()->get_vector(),&Agamma_Vec) );
	Agamma = new GeneralVector<PetscVector>(Agamma_Vec);
	
	LOG_FUNC_END
}

/* destroy gamma solver */
template<class VectorBase>
void GraphH1FEMModel<VectorBase>::finalize_gammasolver(GeneralSolver **gammasolver){
	LOG_FUNC_BEGIN

	/* I created this objects, I should destroy them */

	/* destroy data */
	free(gammadata->get_b());
	free(gammadata->get_A());
	free(gammadata->get_feasibleset());
	free(gammadata);

	/* destroy solver */
	free(*gammasolver);
	
	LOG_FUNC_END
}

/* destroy theta solver */
template<class VectorBase>
void GraphH1FEMModel<VectorBase>::finalize_thetasolver(GeneralSolver **thetasolver){
	LOG_FUNC_BEGIN

	/* I created this objects, I should destroy them */

	/* destroy data */
	free(Agamma);
	free(thetadata);

	/* destroy solver */
	free(*thetasolver);

	LOG_FUNC_END
}

template<class VectorBase>
double GraphH1FEMModel<VectorBase>::get_L(GeneralSolver *gammasolver, GeneralSolver *thetasolver){
	// TODO: not suitable in every situation - I suppose that g was computed from actual x,b */
	return ((QPSolver<VectorBase> *)gammasolver)->get_fx();
}

template<class VectorBase> //TODO: ?
void GraphH1FEMModel<VectorBase>::get_linear_quadratic(double *linearL, double *quadraticL, GeneralSolver *gammasolver, GeneralSolver *thetasolver){
	*linearL = 0;
	*quadraticL = 0;
}

template<class VectorBase>
QPData<VectorBase>* GraphH1FEMModel<VectorBase>::get_gammadata() const {
	return gammadata;
}

template<class VectorBase>
SimpleData<VectorBase>* GraphH1FEMModel<VectorBase>::get_thetadata() const {
	return thetadata;
}

template<class VectorBase>
BGMGraph *GraphH1FEMModel<VectorBase>::get_graph() const {
	return this->tsdata->get_decomposition()->get_graph();
}

template<>
void GraphH1FEMModel<PetscVector>::update_gammasolver(GeneralSolver *gammasolver){
	LOG_FUNC_BEGIN

	int T = tsdata->get_T();
	int Tlocal = tsdata->get_Tlocal();

	int R = tsdata->get_R();
	int Rlocal = tsdata->get_Rlocal();

	int K = tsdata->get_K();

	/* update gamma_solver data - prepare new linear term */
	const double *theta_arr;
	TRYCXX( VecGetArrayRead(tsdata->get_thetavector()->get_vector(), &theta_arr) );
	
	const double *data_arr;
	TRYCXX( VecGetArrayRead(tsdata->get_datavector()->get_vector(), &data_arr) );
	
	double *b_arr;
	TRYCXX( VecGetArray(gammadata->get_b()->get_vector(), &b_arr) );

	double coeff = (-1.0)/((double)(R*T));
//	double coeff = (-1.0)/(sqrt((double)(R*T)));
	
	for(int t=0;t<Tlocal;t++){
		for(int r=0;r<Rlocal;r++){
			for(int k=0;k<K;k++){
				b_arr[t*K*Rlocal + r*K + k] = coeff*(data_arr[t*Rlocal+r] - theta_arr[k])*(data_arr[t*Rlocal+r] - theta_arr[k]);
			}
		}
	}

	/* coeffs of A_shared are updated via computation of Theta :) */

	/* restore arrays */
	TRYCXX( VecRestoreArray(gammadata->get_b()->get_vector(), &b_arr) );
	TRYCXX( VecRestoreArrayRead(tsdata->get_datavector()->get_vector(), &data_arr) );
	TRYCXX( VecRestoreArrayRead(tsdata->get_thetavector()->get_vector(), &theta_arr) );

	LOG_FUNC_END
}

/* update theta solver */
template<>
void GraphH1FEMModel<PetscVector>::update_thetasolver(GeneralSolver *thetasolver){
	LOG_FUNC_BEGIN

	// TODO: if Theta is not in penalty term, this computation is completely WRONG! However, it doesn't matter if Theta is given

	Vec gamma_Vec = tsdata->get_gammavector()->get_vector();
	Vec theta_Vec = tsdata->get_thetavector()->get_vector();
	Vec data_Vec = tsdata->get_datavector()->get_vector();

	/* I will use A_shared with coefficients equal to 1, therefore I set Theta=1 */
	TRYCXX( VecSet(tsdata->get_thetavector()->get_vector(),1.0) );
	TRYCXX( VecAssemblyBegin(tsdata->get_thetavector()->get_vector()) );
	TRYCXX( VecAssemblyEnd(tsdata->get_thetavector()->get_vector()) );

	/* now compute A*gamma */
	Vec Agamma_Vec;
	if(usethetainpenalty){
		/* only if Theta is in penalty term */
		*Agamma = (*A_shared)*(*(tsdata->get_gammavector()));
		Agamma_Vec = Agamma->get_vector();
	}

	/* subvectors */
	Vec gammak_Vec;
	Vec Agammak_Vec;
	IS gammak_is;
	
	double gammakAgammak;
	double gammakx;
	double gammaksum;
	
	/* get arrays */
	double *theta_arr;
	TRYCXX( VecGetArray(theta_Vec,&theta_arr) );

	int K = tsdata->get_K();

//	double coeff = 1.0;
	double coeff = 1.0/((double)(tsdata->get_R()*tsdata->get_T()));

	/* through clusters */
	for(int k=0;k<K;k++){
		
		/* get gammak */
		this->tsdata->get_decomposition()->createIS_gammaK(&gammak_is, k);
		TRYCXX( VecGetSubVector(gamma_Vec, gammak_is, &gammak_Vec) );

		/* compute gammakAgammak */
		if(usethetainpenalty){
			/* only if Theta is in penalty term */
			TRYCXX( VecGetSubVector(Agamma_Vec, gammak_is, &Agammak_Vec) );
			TRYCXX( VecDot(gammak_Vec, Agammak_Vec, &gammakAgammak) );
		}
		
		/* compute gammakx */
		TRYCXX( VecDot(data_Vec, gammak_Vec, &gammakx) );

		/* compute gammaksum */
		TRYCXX( VecSum(gammak_Vec, &gammaksum) );

		if(usethetainpenalty){
			/* only if Theta is in penalty term */
			if(coeff*gammaksum + 0.5*gammakAgammak != 0){
				theta_arr[k] = (coeff*gammakx)/(coeff*gammaksum + 0.5*gammakAgammak);
			} else {
				theta_arr[k] = 0.0;
			}
		} else {
			/* if Theta is not in penalty term, then the computation is based on kmeans */
			if(gammaksum != 0){
				theta_arr[k] = gammakx/gammaksum;
			} else {
				theta_arr[k] = 0.0;
			}
		}
	
		TRYCXX( VecRestoreSubVector(gamma_Vec, gammak_is, &gammak_Vec) );
		if(usethetainpenalty){
			/* only if Theta is in penalty term */
			TRYCXX( VecRestoreSubVector(Agamma_Vec, gammak_is, &Agammak_Vec) );
		}
		TRYCXX( ISDestroy(&gammak_is) );
	}	

	/* restore arrays */
	TRYCXX( VecRestoreArray(theta_Vec,&theta_arr) );

	TRYCXX( PetscBarrier(NULL));

	LOG_FUNC_END
}

template<class VectorBase>
GeneralVector<VectorBase> *GraphH1FEMModel<VectorBase>::get_coordinatesVTK() const{
	return this->get_graph()->get_coordinates();
}

template<class VectorBase>
int GraphH1FEMModel<VectorBase>::get_coordinatesVTK_dim() const{
	return this->get_graph()->get_dim();
}

template<class VectorBase>
double GraphH1FEMModel<VectorBase>::get_aic(double L) const{
	return 2*log(L) + this->tsdata->get_K();

}

template<class VectorBase>
bool GraphH1FEMModel<VectorBase>::get_usethetainpenalty() const{
	return this->usethetainpenalty;

}


}
} /* end namespace */

#endif
