#ifndef PASC_CGQPSOLVER_H
#define	PASC_CGQPSOLVER_H

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include <iostream>
#include "solver/qpsolver.h"
#include "data/qpdata.h"

#define CGQPSOLVER_DEFAULT_MAXIT 1000;
#define CGQPSOLVER_DEFAULT_EPS 0.0001;

namespace pascinference {

/* settings */
class CGQPSolverSetting : public QPSolverSetting {
	public:
		CGQPSolverSetting() {
			this->maxit = CGQPSOLVER_DEFAULT_MAXIT;
			this->eps = CGQPSOLVER_DEFAULT_EPS;
			this->debug_mode = DEBUG_MODE;
		};
		~CGQPSolverSetting() {};

		virtual void print(std::ostream &output) const {
			output << "  CGQPSolverSettings:" << std::endl;
			output << "   - maxit: " << this->maxit << std::endl;
			output << "   - eps: " << this->eps << std::endl;
			output << "   - debug_mode: " << this->debug_mode << std::endl;

		};
		
};


/* CGQPSolver */ 
template<class VectorBase>
class CGQPSolver: public QPSolver<VectorBase> {
	protected:
		const QPData<VectorBase> *data; /* data on which the solver operates */
	
		/* temporary vectors used during the solution process */
		void allocate_temp_vectors();
		void free_temp_vectors();
		GeneralVector<VectorBase> *g; /* gradient */
		GeneralVector<VectorBase> *p; /* A-conjugate vector */
		GeneralVector<VectorBase> *Ap; /* A*p */
	
	public:
		CGQPSolverSetting setting;

		CGQPSolver();
		CGQPSolver(const QPData<VectorBase> &new_data); 
		~CGQPSolver();


		void solve();
		void solve(SolverType type){};

		void print(std::ostream &output) const;
		std::string get_name() const;


};

} // end of namespace

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {

/* constructor */
template<class VectorBase>
CGQPSolver<VectorBase>::CGQPSolver(){
	if(DEBUG_MODE >= 100) std::cout << "(CGQPSolver)CONSTRUCTOR" << std::endl;

	data = NULL;
	
	/* temp vectors */
	g = NULL;
	p = NULL;
	Ap = NULL;
	
	this->fx = std::numeric_limits<double>::max();
}

template<class VectorBase>
CGQPSolver<VectorBase>::CGQPSolver(const QPData<VectorBase> &new_data){
	data = &new_data;
	
	/* allocate temp vectors */
	allocate_temp_vectors();

	this->fx = std::numeric_limits<double>::max();
}


/* destructor */
template<class VectorBase>
CGQPSolver<VectorBase>::~CGQPSolver(){
	if(DEBUG_MODE >= 100) std::cout << "(CGQPSolver)DESTRUCTOR" << std::endl;

	/* free temp vectors */
	free_temp_vectors();
}

/* prepare temp_vectors */
template<class VectorBase>
void CGQPSolver<VectorBase>::allocate_temp_vectors(){
	GeneralVector<VectorBase> *pattern = data->get_b(); /* I will allocate temp vectors subject to linear term */

	g = new GeneralVector<VectorBase>(*pattern);
	p = new GeneralVector<VectorBase>(*pattern);
	Ap = new GeneralVector<VectorBase>(*pattern);	
	
}

/* destroy temp_vectors */
template<class VectorBase>
void CGQPSolver<VectorBase>::free_temp_vectors(){
	free(g);
	free(p);
	free(Ap);
	
}


/* print info about problem */
template<class VectorBase>
void CGQPSolver<VectorBase>::print(std::ostream &output) const {
	if(DEBUG_MODE >= 100) std::cout << "(CGQPSolver)FUNCTION: print" << std::endl;

	output << this->get_name() << std::endl;
	
	/* print settings */
	output << setting;
		
}

template<class VectorBase>
std::string CGQPSolver<VectorBase>::get_name() const {
	return "Conjugate Gradient method for QP";
}


/* solve the problem */
template<class VectorBase>
void CGQPSolver<VectorBase>::solve() {
	if(DEBUG_MODE >= 100) std::cout << "(CGQPSolver)FUNCTION: solve" << std::endl;

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to data */
	pMatrix A = *(data->get_A());
	pVector b = *(data->get_b());
	pVector x0 = *(data->get_x0());

	/* pointer to solution */
	pVector x = *(data->get_x());

	/* auxiliary vectors */
	pVector g = *(this->g); /* gradient */
	pVector p = *(this->p); /* A-conjugate vector */
	pVector Ap = *(this->Ap); /* A*p */

	x = x0; /* set approximation as initial */

	int it = 0; /* iteration counter */
	int hess_mult = 0; /* number of hessian multiplications */
	double normg, alpha, beta, pAp, gg, gg_old;
	
	g = A*x; hess_mult += 1; g -= b; /* compute gradient */
	p = g; /* initial conjugate direction */

	gg = dot(g,g);
	normg = std::sqrt(gg);

	while(normg > this->setting.eps && it < this->setting.maxit){
		/* compute new approximation */

		Ap = A*p; hess_mult += 1;

		/* compute step-size */			
		pAp = dot(Ap,p);
		alpha = gg/pAp;

		/* set new approximation, x = x - alpha*p */
		x -= alpha*p; 

		/* compute gradient recursively, g = g - alpha*Ap */
		g -= alpha*Ap; 
		gg_old = gg;
		gg = dot(g,g);
		normg = std::sqrt(gg);
			
		/* compute new A-orthogonal vector, p = g + beta*p */
		beta = gg/gg_old;
		p *= beta;
		p += g;
		
		if(this->setting.debug_mode >= 10){
			std::cout << "it " << it << ": ||g|| = " << normg << std::endl;
		}

		if(this->setting.debug_mode >= 100){
			std::cout << "x = " << x << std::endl;
			std::cout << "g = " << g << std::endl;
			std::cout << "p = " << p << std::endl;
			std::cout << "Ap = " << Ap << std::endl;
			std::cout << "pAp = " << pAp << std::endl;
			std::cout << "alpha = " << alpha << std::endl;
			std::cout << "beta = " << beta << std::endl;
			std::cout << "gg = " << gg << std::endl;
			std::cout << "gg_old = " << gg_old << std::endl;
			std::cout << "normg = " << normg << std::endl;

			std::cout << "------------------------------------" << std::endl;
		}

				
		it += 1;

	}
		
	/* print output */
	if(this->setting.debug_mode >= 10){
		std::cout << "------------------------" << std::endl;
		std::cout << " it_cg = " << it << std::endl;
		std::cout << " norm_g = " << normg << std::endl;
		std::cout << " hess_mult = " << hess_mult << std::endl;
	}

	
}


} /* end namespace */

#endif
