#ifndef PASC_MULTICGSOLVER_GLOBAL_H
#define	PASC_MULTICGSOLVER_GLOBAL_H

#ifndef USE_PETSCVECTOR
 #error 'MULTICGSOLVER_GLOBAL_GLOBAL is for PETSCVECTOR'
#endif

typedef petscvector::PetscVector PetscVector;

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include <iostream>

#include "solver/multicg.h"
#include "solver/qpsolver.h"
#include "solver/cgqpsolver.h"
#include "data/qpdata.h"

#include "matrix/blockdiag.h"
#include "matrix/localdense.h"

#define MULTICGSOLVER_GLOBAL_DEFAULT_MAXIT 1000;
#define MULTICGSOLVER_GLOBAL_DEFAULT_EPS 0.0001;
#define MULTICGSOLVER_GLOBAL_DEFAULT_DEBUG_MODE 0;

namespace pascinference {

/* settings */
class MultiCGSolver_GlobalSetting : public QPSolverSetting {
	public:
		MultiCGSolver_GlobalSetting() {
			this->maxit = MULTICGSOLVER_GLOBAL_DEFAULT_MAXIT;
			this->eps = MULTICGSOLVER_GLOBAL_DEFAULT_EPS;
			this->debug_mode = MULTICGSOLVER_GLOBAL_DEFAULT_DEBUG_MODE;
		};
		~MultiCGSolver_GlobalSetting() {};

		virtual void print(ConsoleOutput &output) const {
			output <<  this->get_name() << std::endl;
			output <<  " - maxit:      " << this->maxit << std::endl;
			output <<  " - eps:        " << this->eps << std::endl;
			output <<  " - debug_mode: " << this->debug_mode << std::endl;

		};

		std::string get_name() const {
			return "MultiCG_Global SolverSetting";
		};
		
};


/* MultiCGSolver_Global */ 
class MultiCGSolver_Global: public QPSolver<PetscVector> {
	protected:
		const QPData<PetscVector> *qpdata; /* data on which the solver operates, matrix has to be blogdiag */
	
		QPData<PetscVector> *data_local;
		MultiCGSolver<PetscVector> *solver_local; /* local solver */	

		/* local data */
		GeneralVector<PetscVector> *x;
		GeneralVector<PetscVector> *x0;
		GeneralVector<PetscVector> *b;

		void GetLocalData();
		void RestoreLocalData();

	public:
		MultiCGSolver_GlobalSetting setting;

		MultiCGSolver_Global(const QPData<PetscVector> &new_qpdata); 
		~MultiCGSolver_Global();

		void solve();
		double get_fx() const;
		int get_it() const;
		int get_hessmult() const;

		void print(ConsoleOutput &output) const;
		void print(ConsoleOutput &output_global, ConsoleOutput &output_local) const;
		
		void printstatus(ConsoleOutput &output) const;
		void printcontent(ConsoleOutput &output) const;
		std::string get_name() const;

};

} // end of namespace

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {

/* constructor */
MultiCGSolver_Global::MultiCGSolver_Global(const QPData<PetscVector> &new_qpdata){
	qpdata = &new_qpdata;

	this->fx = -1; /* max(norm(g)) */
	this->it_last = 0; /* max(it_block) */
	this->hessmult_last = 0; /* max(hessmult_block) */

	/* initialize local QP solver */
	this->data_local = new QPData<PetscVector>();

	int local_size;
	Vec x_local;
	Vec x0_local;
	Vec b_local;
	
	/* get local size */
	TRY( VecGetLocalSize(qpdata->get_x()->get_vector(), &local_size) );

	/* allocate local data */
	TRY( VecCreateSeq(PETSC_COMM_SELF, local_size, &x_local) );	
	TRY( VecCreateSeq(PETSC_COMM_SELF, local_size, &x0_local) );	
	TRY( VecCreateSeq(PETSC_COMM_SELF, local_size, &b_local) );	

	x = new GeneralVector<PetscVector>(x_local);
	x0 = new GeneralVector<PetscVector>(x0_local);
	b = new GeneralVector<PetscVector>(b_local);

	/* get local vectors and prepare local data */
	data_local->set_A(qpdata->get_A());
	data_local->set_b(b);
	data_local->set_x(x);
	data_local->set_x0(x0);
	
	/* create new instance of local solver */
	solver_local = new MultiCGSolver<PetscVector>(*data_local);
}


/* destructor */
MultiCGSolver_Global::~MultiCGSolver_Global(){
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)DESTRUCTOR" << std::endl;

	free(this->data_local);
	free(this->solver_local);
	
}

void MultiCGSolver_Global::GetLocalData(){
	TRY( VecGetLocalVector(qpdata->get_x()->get_vector(),data_local->get_x()->get_vector()) );
	TRY( VecGetLocalVector(qpdata->get_x0()->get_vector(),data_local->get_x0()->get_vector()) );
	TRY( VecGetLocalVector(qpdata->get_b()->get_vector(),data_local->get_b()->get_vector()) );
}

void MultiCGSolver_Global::RestoreLocalData(){
	TRY( VecRestoreLocalVector(qpdata->get_x()->get_vector(),data_local->get_x()->get_vector()) );
	TRY( VecRestoreLocalVector(qpdata->get_x0()->get_vector(),data_local->get_x0()->get_vector()) );
	TRY( VecRestoreLocalVector(qpdata->get_b()->get_vector(),data_local->get_b()->get_vector()) );
}

/* print info about problem */
void MultiCGSolver_Global::print(ConsoleOutput &output) const {
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)FUNCTION: print" << std::endl;

	output << this->get_name() << std::endl;
	
	/* print settings */
	coutMaster.push();
	setting.print(output);
	coutMaster.pop();

	/* print data */
	if(qpdata){
		output << "- data:" << std::endl;
		coutMaster.push();
		qpdata->print(output);
		coutMaster.pop();
	}
		
}

/* print info about problem */
void MultiCGSolver_Global::print(ConsoleOutput &output_global, ConsoleOutput &output_local) const {
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)FUNCTION: print" << std::endl;

	output_global << this->get_name() << std::endl;
	
	/* print settings */
	output_global.push();
	setting.print(output_global);
	output_global.pop();

	/* print data */
	if(qpdata){
		output_global << "- data:" << std::endl;
		output_global.push();
		qpdata->print(output_global,output_local);
		output_global.pop();
	}
		
}

void MultiCGSolver_Global::printstatus(ConsoleOutput &output) const {
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)FUNCTION: printstatus" << std::endl;

	output <<  " - max(it): " << std::setw(6) << this->it_last << ", ";
	output <<  "max(hessmult): " << std::setw(6) << this->hessmult_last << ", ";
	output <<  "max(norm(g)): " << std::setw(10) << this->fx << ", ";
	output <<  "used memory: " << std::setw(6) << MemoryCheck::get_virtual() << "%" << std::endl;

}

/* print content of solver */
void MultiCGSolver_Global::printcontent(ConsoleOutput &output) const {
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)FUNCTION: printcontent" << std::endl;

	output << this->get_name() << std::endl;
	
	/* print content of data */
	if(qpdata){
		output << "- data:" << std::endl;
		coutMaster.push();
		qpdata->printcontent(output);
		coutMaster.pop();
	}
		
}

std::string MultiCGSolver_Global::get_name() const {
	return "MultiCG_Global method for QP with BlockDiag system matrix";
}

double MultiCGSolver_Global::get_fx() const {
	if(setting.debug_mode >= 11) coutMaster << "(MultiCGSolver_Global)FUNCTION: get_fx()" << std::endl;
	
	return this->fx;	
}

int MultiCGSolver_Global::get_it() const {
	return this->it_last;
}

int MultiCGSolver_Global::get_hessmult() const {
	return this->hessmult_last;
}


/* solve with Petscvector on every processor */
void MultiCGSolver_Global::solve() {
	if(setting.debug_mode >= 100) coutMaster << "(MultiCGSolver_Global)FUNCTION: solve" << std::endl;

	/* for each block prepare CG solver and solve the problem */

	GetLocalData();
		
	/* solve local problem */
	solver_local->solve();

	/* copy results */
	this->it_last = solver_local->get_it();
	this->it_sum += this->it_last;

	/* destroy local storage */
	RestoreLocalData();	

}



} /* end namespace */

#endif