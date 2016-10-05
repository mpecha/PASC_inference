#ifndef PASC_VARXH1FEMMODEL_GLOBAL_H
#define	PASC_VARXH1FEMMODEL_GLOBAL_H

#ifndef USE_PETSCVECTOR
 #error 'VARXH1FEMMODEL_GLOBAL is for PETSCVECTOR'
#endif

typedef petscvector::PetscVector PetscVector;

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include "pascinference.h"

/* gamma problem */
//#include "matrix/blockdiaglaplace_explicit.h"
#include "algebra/matrix/blockdiaglaplace_vector.h"

#include "algebra/feasibleset/simplex_local.h"
#include "solver/qpsolver_global.h"
#include "data/qpdata.h"

/* theta problem */
#include "solver/multicg_global.h"
#include "algebra/matrix/blockdiag.h"
#include "algebra/matrix/localdense.h"

#include "data/tsdata_global.h"


namespace pascinference {
namespace model {

class VarxH1FEMModel_Global: public TSModel_Global {
	protected:
		QPData<PetscVector> *gammadata; /**< QP with simplex */
	 	QPData<PetscVector> *thetadata; /**< QP with blockdiag with dim*K blocks, will be solved by multiCG  */

		/* model specific variables */
		double epssqr; /**< penalty coeficient */

//		int ulength_global;
//		int ulength_local;
		
		/* update thetasolver */
		Vec gamma_local;
		Vec xn1_vec;  /**< scattered xn_global to each processor */
		Vec xn2_vec;  /**< scattered xn_global to each processor */
		Vec xn2subgammak_vec; /**< vector with xn1sub.*gammak_vecs */

		void compute_next_step(double *data_out, int t_data_out, double *data_in, int t_data_in, int xdim, int xmem, double *theta, int k);
		void compute_x_modelk(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, int k, double gamma_value);
		void compute_x_modelk(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, int k);

	public:
	
		VarxH1FEMModel_Global(int T, int xdim, int K, int xmem, double epssqr);
		~VarxH1FEMModel_Global();

		void print(ConsoleOutput &output) const;
		void print(ConsoleOutput &output_global, ConsoleOutput &output_local) const;
		void printsolution(ConsoleOutput &output_global, ConsoleOutput &output_local) const;
		
		std::string get_name() const;
		
		void initialize_gammasolver(GeneralSolver **gamma_solver, const TSData_Global *tsdata);
		void initialize_thetasolver(GeneralSolver **theta_solver, const TSData_Global *tsdata);
		
		void finalize_gammasolver(GeneralSolver **gamma_solver, const TSData_Global *tsdata);
		void finalize_thetasolver(GeneralSolver **theta_solver, const TSData_Global *tsdata);

		void update_gammasolver(GeneralSolver *gamma_solver, const TSData_Global *tsdata);
		void update_thetasolver(GeneralSolver *theta_solver, const TSData_Global *tsdata);
	
		double get_L(GeneralSolver *gammasolver, GeneralSolver *thetasolver, const TSData_Global *tsdata);

		QPData<PetscVector> *get_gammadata() const;
		QPData<PetscVector> *get_thetadata() const;

		void generate_data(int K_solution, int xmem_solution, double *theta, double *xstart, int (*get_cluster_id)(int, int), TSData_Global *tsdata, bool scale_or_not);
		void generate_data_add_noise(TSData_Global *tsdata, double *diag_covariance);
		void generate_data_add_noise(TSData_Global *tsdata, double *diag_covariance, int (*get_cluster_id)(int, int));

		void set_solution_gamma(int K_solution, int xmem_solution, int (*get_cluster_id)(int, int), GeneralVector<PetscVector> *gammavector);

		void compute_x_model(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, const double *gamma_arr, int t_gamma_arr);

//				template<class VectorBase>
//				static void set_solution_theta(int T, int xdim, int K, int xmem, double *theta, GeneralVector<VectorBase> *thetavector);		
};


}
} /* end of namespace */


/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {
namespace model {

/* constructor */
VarxH1FEMModel_Global::VarxH1FEMModel_Global(int new_T, int new_xdim, int new_K, int new_xmem, double new_epssqr) {
	LOG_FUNC_BEGIN

	/* set initial content */
	this->T = new_T;
	this->xdim = new_xdim;

	this->K = new_K;
	this->xmem = new_xmem;
	this->epssqr = new_epssqr;

	int my_rank = GlobalManager.get_rank();

	/* compute vector lengths */
	/* for data prepare vector of length T and spit it into processors */
	Vec layout;

	/* try to make a global vector of length T and then get size of local portion */
	TRY( VecCreate(PETSC_COMM_WORLD,&layout) );
	TRY( VecSetSizes(layout,PETSC_DECIDE,this->T) );
	TRY( VecSetFromOptions(layout) );
	/* get the ownership range - now I know how much I will calculate from the time-series */
	TRY( VecGetLocalSize(layout,&(this->Tlocal)) );
	TRY( VecDestroy(&layout) ); /* destroy testing vector - it is useless now */
	
	this->datavectorlength_global = xdim*this->T;
	this->datavectorlength_local = xdim*this->Tlocal;

	/* prepage global vectors with variables - theta and gamma */
	this->gammavectorlength_local = K*(this->T - this->xmem);
	this->gammavectorlength_global = GlobalManager.get_size()*(this->gammavectorlength_local);

	this->thetavectorlength_local = xdim*(1 + this->xdim*this->xmem)*K; 
	this->thetavectorlength_global = GlobalManager.get_size()*(this->thetavectorlength_local);
	
	/* temp vectors for update theta solver */
	int gammak_size = T-xmem;	
	TRY( VecCreateSeq(PETSC_COMM_SELF, gammak_size, &xn2subgammak_vec) );
	TRY( VecCreateSeq(PETSC_COMM_SELF, T, &xn1_vec) );
	TRY( VecCreateSeq(PETSC_COMM_SELF, T, &xn2_vec) );
	TRY( VecCreateSeq(PETSC_COMM_SELF, K*gammak_size, &gamma_local) );
	
	/* set other variables */
	consoleArg.set_option_value("tsmodel_global_t_scatter", &this->t_scatter, DEFAULT_T_SCATTER); /* > xmem, how long time-series to scatter to all processors */
	TRY( VecCreateSeq(PETSC_COMM_SELF, t_scatter*xdim, &x_scatter) );	
	
	LOG_FUNC_END
}

/* destructor */
VarxH1FEMModel_Global::~VarxH1FEMModel_Global(){
	LOG_FUNC_BEGIN
	
	/* destroy auxiliary vectors */

	/* update gamma solver */
//	TRY( VecDestroy(&x_scatter) );
	
	/* update theta solver */
//	TRY( VecDestroy(&xn2subgammak_vec) );
//	TRY( VecDestroy(&xn1_vec) );
//	TRY( VecDestroy(&xn2_vec) );
//	TRY( VecDestroy(&gamma_local) );
	
	LOG_FUNC_END
}


/* print info about model */
void VarxH1FEMModel_Global::print(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN

	output <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output <<  " - T:       " << this->T << std::endl;
	output <<  " - xdim:    " << this->xdim << std::endl;

	output <<  " - K:       " << this->K << std::endl;
	output <<  " - xmem:    " << this->xmem << std::endl;
	output <<  " - epssqr:  " << this->epssqr << std::endl;

	output <<  " - datalength:  " << this->datavectorlength_global << std::endl;
	output <<  " - gammalength: " << this->gammavectorlength_global << std::endl;
	output <<  " - thetalength: " << this->thetavectorlength_global << std::endl;
	
	output.synchronize();	

	LOG_FUNC_END
}

/* print info about model */
void VarxH1FEMModel_Global::print(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	LOG_FUNC_BEGIN

	output_global <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output_global <<  " - global info:  " << std::endl;
	output_global <<  "  - T:       " << this->T << std::endl;
	output_global <<  "  - xdim:    " << this->xdim << std::endl;

	output_global <<  " - K:       " << this->K << std::endl;
	output_global <<  " - xmem:    " << this->xmem << std::endl;
	output_global <<  " - epssqr:  " << this->epssqr << std::endl;

	output_global <<  " - datalength:  " << this->datavectorlength_global << std::endl;
	output_global <<  " - gammalength: " << this->gammavectorlength_global << std::endl;
	output_global <<  " - thetalength: " << this->thetavectorlength_global << std::endl;

	/* give local info */
	output_global <<  " - local variables:  " << std::endl;
	output_global.push();
	output_local << "Tlocal =" << std::setw(6) << this->Tlocal << ", ";
	output_local << "datalength=" << std::setw(6) << this->datavectorlength_local << ", ";
	output_local << "gammalength=" << std::setw(6) << this->gammavectorlength_local << ", ";
	output_local << "thetalength=" << std::setw(6) << this->thetavectorlength_local << std::endl;

	output_global.pop();
	output_local.synchronize();

	output_global.synchronize();

	LOG_FUNC_END
}

/* print model solution */
void VarxH1FEMModel_Global::printsolution(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	LOG_FUNC_BEGIN

	output_global <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output_global <<  "theta:" << std::endl;

	std::ostringstream temp;
	
	double *theta;
	TRY( VecGetArray(thetadata->get_x()->get_vector(),&theta) );
	
	int k,i,n,n2;
	int blocksize = 1 + this->xdim*this->xmem;

	output_local.push();
	output_local << "- proc: " << GlobalManager.get_rank() << std::endl;
	output_local.push();
	for(k=0;k<this->K;k++){
		output_local <<  "- k = " << k << std::endl;

		/* mu */
		output_local.push();
		output_local <<  "- mu = [";
		for(n=0;n<xdim;n++){
			temp << theta[k*xdim*blocksize + n*blocksize];
			output_local << temp.str();
			if(n < xdim-1){
				output_local << ", ";
			}
			temp.str("");
		}
		output_local <<  "]" << std::endl;
		output_local.pop();

		/* A */
		output_local.push();
		for(i=0;i<this->xmem;i++){
			output_local <<  "- A" << i << " = [" <<  std::endl;
			output_local.push();
			for(n=0;n<xdim;n++){
				for(n2=0;n2<xdim;n2++){
					temp << theta[k*xdim*blocksize + i*xdim + n*blocksize + 1 + n2];
					output_local << temp.str() << ", ";
					temp.str("");
				}
				output_local << std::endl;
			}
			output_local.pop();
			output_local <<  "  ]" << std::endl;
		}
		output_local.pop();

	}
	output_local.pop();
	output_local.pop();

	output_local.synchronize();
	output_global.synchronize();

	LOG_FUNC_END
}

/* get name of the model */
std::string VarxH1FEMModel_Global::get_name() const {
	return "VARX-H1-FEM Global Time-Series Model";	
}

/* prepare gamma solver */
void VarxH1FEMModel_Global::initialize_gammasolver(GeneralSolver **gammasolver, const TSData_Global *tsdata){
	LOG_FUNC_BEGIN

	/* in this case, gamma problem is QP with simplex feasible set */
	
	/* create data */
	gammadata = new QPData<PetscVector>();
	
	gammadata->set_x0(tsdata->get_gammavector()); /* the initial approximation of QP problem is gammavector */
	gammadata->set_x(tsdata->get_gammavector()); /* the solution of QP problem is gamma */
	gammadata->set_b(new GeneralVector<PetscVector>(*gammadata->get_x0())); /* create new linear term of QP problem */

	gammadata->set_A(new BlockDiagLaplaceVectorMatrix<PetscVector>(*gammadata->get_x0(),this->K, this->T - this->xmem,this->epssqr*this->epssqr)); /* create new blockdiagonal matrix */
	gammadata->set_feasibleset(new SimplexFeasibleSet_Local(this->T - this->xmem,this->K)); /* the feasible set of QP is simplex */ 	

	/* create solver */
	*gammasolver = new QPSolver_Global(*gammadata);

	/* generate random data to gamma */
	gammadata->get_x0()->set_random();

	/* project random values to feasible set to be sure that initial approximation is feasible */
	gammadata->get_feasibleset()->project(*gammadata->get_x0());

	LOG_FUNC_END
}

/* prepare theta solver */
void VarxH1FEMModel_Global::initialize_thetasolver(GeneralSolver **thetasolver, const TSData_Global *tsdata){
	LOG_FUNC_BEGIN

	/* in this case, theta problem is a sequence of unconstrained QP problems */

	/* prepare array with matrices */
	int nmb_blocks = this->K*this->xdim;
	LocalDenseMatrix<PetscVector> **blocks = new LocalDenseMatrix<PetscVector>*[nmb_blocks];

	int blocksize = 1 + this->xdim*this->xmem; /* see get_thetavectorlength() */

	int k,n; /* through clusters, through dimensions */
	for(k=0;k<this->K;k++){ // TODO: parallel
		for(n=0;n<this->xdim;n++){
			if(n==0){ 
				/* if this is a first dimension, then create matrix */
				blocks[k*this->xdim + n] = new LocalDenseMatrix<PetscVector>(blocksize, blocksize);
			} else {
				/* else get matrix from first dimension */
				blocks[k*this->xdim + n] = blocks[k*this->xdim];
			}
		}
	}
	
	/* create data */
	thetadata = new QPData<PetscVector>();
	thetadata->set_x0(tsdata->get_thetavector()); /* the initial approximation of QP problem is gammavector */
	thetadata->set_x(tsdata->get_thetavector()); /* the solution of QP problem is gamma */
	thetadata->set_b(new GeneralVector<PetscVector>(*thetadata->get_x0())); /* create new linear term of QP problem */

	thetadata->set_A(new BlockDiagMatrix<PetscVector,LocalDenseMatrix<PetscVector> >(nmb_blocks, blocks, blocksize));

	/* create solver */
	*thetasolver = new MultiCGSolver_Global(*thetadata);
	
	LOG_FUNC_END
}

/* destroy gamma solver */
void VarxH1FEMModel_Global::finalize_gammasolver(GeneralSolver **gammasolver, const TSData_Global *tsdata){
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
void VarxH1FEMModel_Global::finalize_thetasolver(GeneralSolver **thetasolver, const TSData_Global *tsdata){
	LOG_FUNC_BEGIN
	
	/* prepare pointer to child, I need to change pointer from general matrix to block diag (to be able to call get_block() ) */
	BlockDiagMatrix<PetscVector,LocalDenseMatrix<PetscVector> > *A = dynamic_cast<BlockDiagMatrix<PetscVector,LocalDenseMatrix<PetscVector> > *>(thetadata->get_A());
	LocalDenseMatrix<PetscVector> **blocks = A->get_blocks();

	/* destroy blocks */
	int k; /* through clusters, through dimensions */
	for(k=0;k<this->K;k++){ // TODO: parallel
		/* if this is a first dimension, then destroy matrix */
		free(blocks[k*this->xdim]);
	}
	
	free(thetadata->get_A());
	
	/* destroy other data */
	free(thetadata->get_b());
	free(thetadata);

	/* destroy solver */
	free(*thetasolver);

	LOG_FUNC_END
}

double VarxH1FEMModel_Global::get_L(GeneralSolver *gammasolver, GeneralSolver *thetasolver, const TSData_Global *tsdata){
	
	// TODO: not suitable in every situation - I suppose that g was computed from actual x,b */
	return ((QPSolver_Global *)gammasolver)->get_fx();
}

QPData<PetscVector>* VarxH1FEMModel_Global::get_gammadata() const {
	return gammadata;
}

QPData<PetscVector>* VarxH1FEMModel_Global::get_thetadata() const {
	return thetadata;
}

void VarxH1FEMModel_Global::update_gammasolver(GeneralSolver *gammasolver, const TSData_Global *tsdata){
	LOG_FUNC_BEGIN
	
	/* update gamma_solver data - prepare new linear term */

	/* global data */
	Vec M_global = tsdata->get_thetavector()->get_vector(); /* parameters of the model */
	Vec x_global = tsdata->get_datavector()->get_vector(); /* original data */
	Vec b_global = gammadata->get_b()->get_vector(); /* rhs of QP gamma problem */

	/* local data */
	const double *theta_arr;
	double *b_local_arr;

	/* get local arrays */
	TRY( VecGetArrayRead(M_global,&theta_arr) );
	TRY( VecGetArray(b_global,&b_local_arr) );	

	/* get constants of the problem */
	int gammak_size = T-xmem;
	int k,t,n;
	double value;
	double x_model[xdim];

	int t_in_scatter;
	int is_begin = 0;
	int is_end = min(is_begin + t_scatter,T);

	const double *x_scatter_arr;

	while(is_end <= T && is_begin < is_end){ /* while there is something to scatter */
		/* scatter part of time serie */
		scatter_part(x_global, is_begin, is_end, xdim);
				
		/* write begin of time-serie */
		TRY( VecGetArrayRead(x_scatter, &x_scatter_arr) );
		for(t=is_begin+xmem;t<is_end;t++){
			t_in_scatter = t - is_begin;

			/* x_scatter_arr[t_in_scatter*xdim] is a start */
			for(k=0;k<K;k++){
				compute_x_modelk(x_model, x_scatter_arr, t_in_scatter, theta_arr, k);

				/* now I have computed x_model(t), therefore I just compute || x_model(t)-x_data(t) ||^2 */
				value = 0;
				for(n=0;n<xdim;n++){
					value += (x_model[n] - x_scatter_arr[t_in_scatter*xdim+n])*(x_model[n] - x_scatter_arr[t_in_scatter*xdim+n]);
				}

				/* write the error into linear term */
				value = (-1)*value;
				b_local_arr[k*gammak_size+t-xmem] = value;
			}

		}
		TRY( VecRestoreArrayRead(x_scatter, &x_scatter_arr) );
				
		/* update upper and lower index of scatter_is */
		is_begin = is_begin + t_scatter - xmem;
		is_end = min(is_begin + t_scatter,T);

	}
	TRY( VecRestoreArrayRead(M_global,&theta_arr) );
	TRY( VecRestoreArray(b_global,&b_local_arr) );	

	LOG_FUNC_END
}

/* update theta solver */
void VarxH1FEMModel_Global::update_thetasolver(GeneralSolver *thetasolver, const TSData_Global *tsdata){
	LOG_FUNC_BEGIN
	
	/* update theta solver - prepare new BlockDiag matrix data and right-hand side vector */	

	/* pointers to global data */
	Vec x_global = tsdata->get_datavector()->get_vector();
	
	/* get constants of the problem */
//	int umem = this->umem;
	int blocksize = 1 + xdim*xmem; /* see get_thetavectorlength() */

	/* blocks of the matrix */
	BlockDiagMatrix<PetscVector,LocalDenseMatrix<PetscVector> > *A = dynamic_cast<BlockDiagMatrix<PetscVector,LocalDenseMatrix<PetscVector> > *>(thetadata->get_A());
	LocalDenseMatrix<PetscVector> **blocks = A->get_blocks();

	int gammak_size = T-xmem;

	/* prepare local gammas */
	TRY( VecGetLocalVector(gammadata->get_x()->get_vector(),gamma_local) );
	
	Vec gammak_vecs[K];
	IS gammak_is[K];
	int k;
	for(k=0;k<K;k++){
		TRY( ISCreateStride(PETSC_COMM_SELF, gammak_size, k*gammak_size, 1, &gammak_is[k]) );
		TRY( VecGetSubVector(gamma_local, gammak_is[k], &(gammak_vecs[k])) );
	}
	
	TRY( PetscBarrier(NULL) );
	
	/* prepare local b */
	double *b_arr;
	//TODO: temp
	TRY( VecSet(thetadata->get_b()->get_vector(), 0.0) );
	
	TRY( VecGetArray(thetadata->get_b()->get_vector(), &b_arr) );
		
	/* index set for manipulating with local parts of xn according to xmem */
	IS xn1sub_is;
	IS xn2sub_is;
	Vec xn1sub_vec; 
	Vec xn2sub_vec;
	int xn1sub_nmb;
	int xn2sub_nmb;
	
	/* go through clusters and fill matrices */
	int col = -1;
	int row = -1;
	int xdim1,xdim2, i,j;
	double value;

	double coeff = 1;

	/* matrix */
	for(xdim1 = 0; xdim1<xdim+1; xdim1++){
		/* constant */
		if(xdim1 == 0){
			/* the first row is 1 */
			TRY( VecSet(xn1_vec, coeff) );
			VecAssemblyBegin(xn1_vec);
			VecAssemblyEnd(xn1_vec);
			
			xn1sub_nmb = 1;
		}
		
		/* x rows */
		if(xdim1 >= 1 && xdim1 < 1+xdim){
			/* scatter xn with n=xdim1 */
			scatter_xn(x_global, xn1_vec, xdim, xdim1-1);
			xn1sub_nmb = xmem;

		}

		TRY( PetscBarrier(NULL) );

		for(xdim2 = 0; xdim2<xdim+1;xdim2++){

			/* constant */
			if(xdim2 == 0){
				/* the first row is 1 */
				TRY( VecSet(xn2_vec, coeff) );
				VecAssemblyBegin(xn2_vec);
				VecAssemblyEnd(xn2_vec);

				xn2sub_nmb = 1;
			}
		
			/* x rows */
			if(xdim2 >= 1 && xdim2 < 1+xdim){
				/* scatter xn with n=xdim2 */
				scatter_xn(x_global, xn2_vec, xdim, xdim2-1);
				xn2sub_nmb = xmem;
			}
			
			/* now I have two vectors with components corresponding with one dimension, what to do next? - local computations */
			
			for(i=0;i<xn1sub_nmb;i++){
				/* prepare subvector from xn_vec which corresponds to row of Z */
				TRY( ISCreateStride(PETSC_COMM_SELF, gammak_size, i, 1, &xn1sub_is) );
				TRY( VecGetSubVector(xn1_vec, xn1sub_is, &xn1sub_vec) );				
				
				/* ----- MATRIX --- */
				for(j=0;j<xn2sub_nmb;j++){
					/* prepare subvector from xn_vec which corresponds to column of Z */
					TRY( ISCreateStride(PETSC_COMM_SELF, gammak_size, j, 1, &xn2sub_is) );
					TRY( VecGetSubVector(xn2_vec, xn2sub_is, &xn2sub_vec) );				
					
					/* go through clusters and fill blocks */
					for(k=0;k<K;k++){
						TRY( VecPointwiseMult(xn2subgammak_vec, xn2sub_vec, gammak_vecs[k]) );

						/* compute dot product xn1sub_vec*xn2sub_vec */
						TRY( VecDot(xn2subgammak_vec,xn1sub_vec,&value) );

						/* write values to symmetric matrix */
						if(xdim1 == 0){
							row = xdim1 + (xn1sub_nmb-i-1)*xdim;
						}
						if(xdim2 == 0){
							col = xdim2 + (xn2sub_nmb-j-1)*xdim;
						}
						
						if(xdim1 >= 1 && xdim1 < 1+xdim){
							row = xdim1 + (xn1sub_nmb-i-1)*xdim;
						}						
						if(xdim2 >= 1 && xdim2 < 1+xdim){
							col = xdim2 + (xn2sub_nmb-j-1)*xdim;
						}						


						if(row == col){
							/* diagonal entry */
							blocks[k*xdim]->set_value(row,col,value);
						} else {
							/* nondiagonal entry */
							blocks[k*xdim]->set_value(row,col,value);
						}
					}
					
					/* restore, clear and prepare for next row */
					TRY( VecRestoreSubVector(xn2_vec, xn2sub_is, &xn2sub_vec) );
					TRY( ISDestroy(&xn2sub_is) );

				}
				
				/* ----- VECTOR ----- */
				if(xdim2 >= 1 && xdim2 < 1+xdim){
					TRY( ISCreateStride(PETSC_COMM_SELF, gammak_size, xmem, 1, &xn2sub_is) );
					TRY( VecGetSubVector(xn2_vec, xn2sub_is, &xn2sub_vec) );
					/* go through clusters and fill RHS vector */
					for(k=0;k<K;k++){
						TRY( VecPointwiseMult(xn2subgammak_vec, xn2sub_vec, gammak_vecs[k]) );

						/* compute dot product xn1sub_vec*xn2sub_vec */
						TRY( VecDot(xn2subgammak_vec,xn1sub_vec,&value) );

						if(xdim1 == 0){
							row = k*blocksize*xdim + (xdim2-1)*blocksize;
						}

						/* x rows */
						if(xdim1 >= 1 && xdim1 < 1+xdim){
							row = k*blocksize*xdim + (xdim2-1)*blocksize + xdim1 + (xn1sub_nmb-i-1)*xdim;
						}

						b_arr[row] = value;

					}
					TRY( VecRestoreSubVector(xn2_vec, xn2sub_is, &xn2sub_vec) );
					TRY( ISDestroy(&xn2sub_is) );
				}
				
				/* restore, clear and prepare for next row */
				TRY( VecRestoreSubVector(xn1_vec, xn1sub_is, &xn1sub_vec) );
				TRY( ISDestroy(&xn1sub_is) );
			}
			
			
			
		}
		
	}

	TRY( VecRestoreArray(thetadata->get_b()->get_vector(), &b_arr) );
//	TRY( VecRestoreLocalVector(gammadata->get_x()->get_vector(), gamma_local));

	/* restore gammas */
	for(k=0;k<K;k++){
		TRY( VecRestoreSubVector(gamma_local, gammak_is[k], &(gammak_vecs[k])) );
		TRY( ISDestroy(&gammak_is[k]) );
	}
	TRY( VecRestoreLocalVector(gammadata->get_x()->get_vector(),gamma_local) );

//	TRY( VecDestroy(&gamma_local) );
//	TRY( VecDestroy(&xn1_vec) );
//	TRY( VecDestroy(&xn2_vec) );

	/* assemble blocks of matrix */
	for(k=0;k<K;k++){
		blocks[k*xdim]->assemble();
	}

	LOG_FUNC_END
}

void VarxH1FEMModel_Global::generate_data(int K_solution, int xmem_solution, double *theta, double *xstart, int (*get_cluster_id)(int, int), TSData_Global *tsdata, bool scale_or_not){
	LOG_FUNC_STATIC_BEGIN
			
	/* size of input */
	int xstart_size = xdim*xmem_solution;

	int nproc = GlobalManager.get_size();
	int my_rank = GlobalManager.get_rank();

	/* get ownership range */
	int t_begin, t_end, t_length; 
	TRY( VecGetOwnershipRange(tsdata->get_datavector()->get_vector(), &t_begin, &t_end) );
	t_begin = ((double)t_begin)/((double)xdim);
	t_end = ((double)t_end)/((double)xdim);			
	t_length = t_end - t_begin;
			
	/* scattering the tails */
	VecScatter ctx; /* for scattering xn_global to xn_local */
	IS scatter_is;
	IS scatter_is_to;
	TRY( ISCreateStride(PETSC_COMM_SELF, xstart_size, 0, 1, &scatter_is_to) );
			
	Vec xtail_global;
		TRY( VecCreate(PETSC_COMM_WORLD,&xtail_global) );
		TRY( VecSetSizes(xtail_global,xstart_size, PETSC_DECIDE) );
		TRY( VecSetFromOptions(xtail_global) );
	Vec xtail_local;
		TRY( VecCreateSeq(PETSC_COMM_SELF, xstart_size, &xtail_local) );
	double *xtail_arr;

	/* get local data array */
	double *x_arr;
	TRY( VecGetArray(tsdata->get_datavector()->get_vector(),&x_arr) );

	/* I suppose that t_length >= xmem */
	int rank, t, n, k;
	for(rank = 0; rank < nproc; rank++){ /* through processors - this is sequential */
		/* xstart */
		if(rank == my_rank){
			/* the start */
			for(t = 0; t < xmem_solution; t++){
				if(rank == 0){
					/* given start */
					for(n = 0; n < xdim; n++){
						x_arr[t*xdim+n] = xstart[t*xdim+n];
					}	
				} else {
					/* compute start from obtained tail from previous rank */
					TRY( VecGetArray(xtail_local,&xtail_arr) );
					for(t = 0; t < xstart_size;t++){
						x_arr[t] = xtail_arr[t];
					}
					TRY( VecRestoreArray(xtail_global,&xtail_arr) );
				}
			}
					
			for(t = xmem_solution; t < t_length; t++){ /* through local time */
				k = (*get_cluster_id)(t_begin + t, T);
				compute_next_step(x_arr, t, x_arr, t, xdim, xmem_solution, theta, k);

			}
		}

		/* fill the tail vector */
		TRY( VecGetArray(xtail_global,&xtail_arr) );
		for(t = 0; t < xstart_size;t++){
			xtail_arr[t] = x_arr[(t_length-xmem_solution)*xdim+t];
		}
		TRY( VecRestoreArray(xtail_global,&xtail_arr) );

		/* now tails are stored in global vector, scatter them to local vector */
		TRY( ISCreateStride(PETSC_COMM_WORLD, xstart_size, rank*xstart_size, 1, &scatter_is) );

		/* scatter xtail_global to xtail_local */
		TRY( VecScatterCreate(xtail_global,scatter_is,xtail_local,scatter_is_to,&ctx) );
		TRY( VecScatterBegin(ctx,xtail_global,xtail_local,INSERT_VALUES,SCATTER_FORWARD) );
		TRY( VecScatterEnd(ctx,xtail_global,xtail_local,INSERT_VALUES,SCATTER_FORWARD) );

		TRY( ISDestroy(&scatter_is) );
		TRY( VecScatterDestroy(&ctx) );

		TRY( PetscBarrier(NULL) );
	}

	/* restore local data array */
	TRY( VecRestoreArray(tsdata->get_datavector()->get_vector(),&x_arr) );

	TRY( VecDestroy(&xtail_global) );
	TRY( VecDestroy(&xtail_local) );


	/* scale data */
	if(scale_or_not){
		double max_value;
		TRY( VecMax(tsdata->get_datavector()->get_vector(), NULL, &max_value) );
		TRY( VecScale(tsdata->get_datavector()->get_vector(), 1.0/max_value) );
				
		coutAll << "--- scaling data with max value of x: " << max_value << std::endl;
		coutAll.synchronize();
	}

	LOG_FUNC_STATIC_END
}

void VarxH1FEMModel_Global::generate_data_add_noise(TSData_Global *tsdata, double *diag_covariance){
	int n,t;
	
	/* prepare mu */
	double mu[xdim];
	double values[xdim];

	for(n = 0; n <xdim; n++){
		mu[n] = 0.0;
	}

	/* get local data array */
	double *x_arr;
	TRY( VecGetArray(tsdata->get_datavector()->get_vector(),&x_arr) );
	
	/* add noise to generated data */
	for(t = 0; t < Tlocal; t++){
		my_mvnrnd_Dn(xdim, mu, diag_covariance, values);
		for(n = 0; n <xdim; n++){
			x_arr[t*xdim+n] += values[n];
		}
	}
	TRY( VecRestoreArray(tsdata->get_datavector()->get_vector(),&x_arr) );
	
}

void VarxH1FEMModel_Global::generate_data_add_noise(TSData_Global *tsdata, double *diag_covariance, int (*get_cluster_id)(int, int)){
	int n,t,k,i;
	
	/* prepare mu */
	double mu[xdim];
	double values[xdim];
	double Kdiag_covariance[xdim];

	for(n = 0; n <xdim; n++){
		mu[n] = 0.0;
	}

	/* get ownership range */
	int t_begin; 
	TRY( VecGetOwnershipRange(tsdata->get_datavector()->get_vector(), &t_begin, NULL) );
	t_begin = ((double)t_begin)/((double)xdim);

	/* get local data array */
	double *x_arr;
	TRY( VecGetArray(tsdata->get_datavector()->get_vector(),&x_arr) );
	
	/* add noise to generated data */
	for(t = 0; t < Tlocal; t++){
		k = (*get_cluster_id)(t_begin + t, T);
		for(i = 0; i < xdim; i++){
			Kdiag_covariance[i] = diag_covariance[k*xdim + i];
		}
		
		my_mvnrnd_Dn(xdim, mu, Kdiag_covariance, values);
		for(n = 0; n <xdim; n++){
			x_arr[t*xdim+n] += values[n];
		}
	}
	TRY( VecRestoreArray(tsdata->get_datavector()->get_vector(),&x_arr) );
	
}

void VarxH1FEMModel_Global::compute_next_step(double *data_out, int t_data_out, double *data_in, int t_data_in, int xdim, int xmem, double *theta, int k){
	int theta_length_n = 1+xdim*xmem; /* the size of theta for one dimension, one cluster */
	int theta_start = k*theta_length_n*xdim; /* where in theta start actual coefficients */

	double Ax;

	int n,i;
	for(n = 0; n < xdim; n++){
		/* mu */
		data_out[t_data_out*xdim+n] = theta[theta_start + n*theta_length_n]; 
				
		/* A (xmem) */
		for(int t_mem = 1; t_mem <= xmem; t_mem++){
			/* add multiplication with A_{t_mem} */
			Ax = 0;
			for(i = 0; i < xdim; i++){
				Ax += theta[theta_start + n*theta_length_n + 1 + (t_mem-1)*xdim + i]*data_in[(t_data_in-t_mem)*xdim+i]; 
			}
			data_out[t_data_out*xdim+n] += Ax;
	
		}
	
	}
}

void VarxH1FEMModel_Global::compute_x_modelk(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, int k, double gamma_value){
	int n, i;
	int blocksize = 1 + this->xdim*this->xmem;

	double Ax;
	int	theta_start = k*blocksize*this->xdim;

	for(n=0;n<xdim;n++){
		x_model[n] = 0;

		/* mu */
		x_model[n] += gamma_value*theta_arr[theta_start + n*blocksize]; 
		
		/* A */
		for(int t_mem = 1; t_mem <= this->xmem; t_mem++){
			/* add multiplication with A_{t_mem} */
			Ax = 0;
			for(i = 0; i < xdim; i++){
					Ax += gamma_value*theta_arr[theta_start + n*blocksize + 1 + (t_mem-1)*xdim + i]*x_arr[(t_x_arr-t_mem)*xdim+i]; 
			}
			x_model[n] += Ax;
		}

	}

}

void VarxH1FEMModel_Global::compute_x_modelk(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, int k){
	compute_x_modelk(x_model, x_arr, t_x_arr, theta_arr, k, 1.0);
}

void VarxH1FEMModel_Global::compute_x_model(double *x_model, const double *x_arr, int t_x_arr, const double *theta_arr, const double *gamma_arr, int t_gamma_arr){
	double x_modelk[xdim];
	int k, n;

	/* initialize */
	for(n=0;n<xdim;n++){
		x_model[n] = 0; 
	}
	
	/* go throught clusters */
	for(k=0;k<K;k++){
		compute_x_modelk(x_modelk, x_arr, t_x_arr, theta_arr, k, gamma_arr[k*(this->T-xmem)+t_gamma_arr]);
		for(n=0;n<xdim;n++){
			x_model[n] += x_modelk[n]; 
		}
	}

}

void VarxH1FEMModel_Global::set_solution_gamma(int K_solution, int xmem_solution, int (*get_cluster_id)(int, int), GeneralVector<PetscVector> *gammavector){
	//TODO: K_solution == K ? xmem_solution == xmem ?

	LOG_FUNC_STATIC_BEGIN

	double *gamma_arr;
	TRY( VecGetArray(gammavector->get_vector(),&gamma_arr) );
	
	int t,k,k_sol;
	int gamma_length = T-xmem_solution;
	for(t = 0; t < gamma_length; t++){
		for(k = 0; k < K_solution; k++){
			gamma_arr[k*gamma_length + t] = 0;
		}

		k_sol = (*get_cluster_id)(xmem_solution + t, T);
		gamma_arr[k_sol*gamma_length + t] = 1.0;
	}

	TRY( VecRestoreArray(gammavector->get_vector(),&gamma_arr) );

	LOG_FUNC_STATIC_END
}


}
} /* end namespace */

#endif