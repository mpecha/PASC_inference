#ifndef SPGQPSOLVER_H
#define	SPGQPSOLVER_H

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include <iostream>
#include "solver/qpsolver.h"
#include "data/qpdata.h"
#include "result/qpresult.h"

#define SPGQPSOLVER_DEFAULT_MAXIT 1000;
#define SPGQPSOLVER_DEFAULT_EPS 0.0001;

#define SPGQPSOLVER_DEFAULT_M 10;
#define SPGQPSOLVER_DEFAULT_GAMMA 0.9;
#define SPGQPSOLVER_DEFAULT_SIGMA2 1.0;
#define SPGQPSOLVER_DEFAULT_ALPHAINIT 1.0;

namespace pascinference {

/* settings */
class SPGQPSolverSetting : public QPSolverSetting {
	public:
		int maxit; /* max number of iterations */
		double eps; /* precision */
		int debug_mode; /* print info about the progress */
		int m; /* size of fs */
		double gamma; 
		double sigma2;
		double alphainit; /* initial step-size */

		SPGQPSolverSetting() {
			maxit = SPGQPSOLVER_DEFAULT_MAXIT;
			eps = SPGQPSOLVER_DEFAULT_EPS;
			debug_mode = DEBUG_MODE;

			m = SPGQPSOLVER_DEFAULT_M;
			gamma = SPGQPSOLVER_DEFAULT_GAMMA;
			sigma2 = SPGQPSOLVER_DEFAULT_SIGMA2;
			alphainit = SPGQPSOLVER_DEFAULT_ALPHAINIT;

		};
		~SPGQPSolverSetting() {};

		virtual void print(std::ostream &output) const {
			output << "  SPGQPSolverSettings:" << std::endl;
			output << "   - maxit:      " << maxit << std::endl;
			output << "   - eps:        " << eps << std::endl;
			output << "   - debug_mode: " << debug_mode << std::endl;

			output << "   - m:          " << m << std::endl;
			output << "   - gamma:      " << gamma << std::endl;
			output << "   - sigma2:     " << sigma2 << std::endl;
			output << "   - alphainit:  " << alphainit << std::endl;

		};
		
};


/* for manipulation with fs - function values for generalized Armijo condition */
class SPGQPSolver_fs {
	private:
		int m; /* the length of list */
		std::list<double> fs_list; /* the list with function values */
		
	public: 
		SPGQPSolver_fs(int new_m);
		void init(double fx);
		double get_max();		
		int get_size();
		void update(double new_fx);
		
		friend std::ostream &operator<<(std::ostream &output, SPGQPSolver_fs fs);
};



/* SPGQPSolver */ 
template<class VectorBase>
class SPGQPSolver: public QPSolver<VectorBase> {
	protected:
		const QPData<VectorBase> *data; /* data on which the solver operates */
		const QPResult<VectorBase> *result; /* here solver stores results */
	
		/* temporary vectors used during the solution process */
		void allocate_temp_vectors();
		void free_temp_vectors();
		GeneralVector<VectorBase> *g; /* gradient */
		GeneralVector<VectorBase> *d; /* projected gradient */
		GeneralVector<VectorBase> *Ad; /* A*d */
		GeneralVector<VectorBase> *temp; /* general temp vector */

		/* compute function values from g,x */
		double get_function_value();
	
	public:
		SPGQPSolverSetting setting;

		SPGQPSolver();
		SPGQPSolver(const QPData<VectorBase> &new_data, const QPResult<VectorBase> &new_result); 
		~SPGQPSolver();

		void solve();
		void solve(SolverType type){};
		double get_function_value(GeneralVector<VectorBase> *x);

		void print(std::ostream &output) const;
		std::string get_name() const;

};

} // end of namespace

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {


/* ----- Solver ----- */
/* constructor */
template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver(){
	if(DEBUG_MODE >= 100) std::cout << "(SPGQPSolver)CONSTRUCTOR" << std::endl;

	data = NULL;
	result = NULL;
	
	/* temp vectors */
	g = NULL;
	d = NULL;
	Ad = NULL;
	temp = NULL;

}

template<class VectorBase>
SPGQPSolver<VectorBase>::SPGQPSolver(const QPData<VectorBase> &new_data, const QPResult<VectorBase> &new_result){
	data = &new_data;
	result = &new_result;
	
	/* allocate temp vectors */
	allocate_temp_vectors();
	
}


/* destructor */
template<class VectorBase>
SPGQPSolver<VectorBase>::~SPGQPSolver(){
	if(DEBUG_MODE >= 100) std::cout << "(SPGQPSolver)DESTRUCTOR" << std::endl;

	/* free temp vectors */
	free_temp_vectors();
}

/* prepare temp_vectors */
template<class VectorBase>
void SPGQPSolver<VectorBase>::allocate_temp_vectors(){
	GeneralVector<VectorBase> *pattern = data->b; /* I will allocate temp vectors subject to linear term */

	g = new GeneralVector<VectorBase>(*pattern);
	d = new GeneralVector<VectorBase>(*pattern);
	Ad = new GeneralVector<VectorBase>(*pattern);	
	temp = new GeneralVector<VectorBase>(*pattern);	
	
}

/* destroy temp_vectors */
template<class VectorBase>
void SPGQPSolver<VectorBase>::free_temp_vectors(){
	free(g);
	free(d);
	free(Ad);
	free(temp);
	
}


/* print info about problem */
template<class VectorBase>
void SPGQPSolver<VectorBase>::print(std::ostream &output) const {
	if(DEBUG_MODE >= 100) std::cout << "(SPGQPSolver)FUNCTION: print" << std::endl;

	output << this->get_name() << std::endl;
	
	/* print settings */
	output << setting;
		
}

template<class VectorBase>
std::string SPGQPSolver<VectorBase>::get_name() const {
	return "Spectral Projected Gradient method for QP";
}

/* solve the problem */
template<class VectorBase>
void SPGQPSolver<VectorBase>::solve() {
	if(DEBUG_MODE >= 100) std::cout << "(SPGQPSolver)FUNCTION: solve" << std::endl;

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to data */
	pMatrix A = *(data->A);
	pVector b = *(data->b);
	pVector x0 = *(data->x0);

	/* pointer to result */
	pVector x = *(result->x);

	/* auxiliary vectors */
	pVector g = *(this->g); /* gradient */
	pVector d = *(this->d); /* A-conjugate vector */
	pVector Ad = *(this->Ad); /* A*p */

	int it = 0; /* number of iterations */
	int hessmult = 0; /* number of hessian multiplications */

	double fx; /* function value */
	SPGQPSolver_fs fs(setting.m); /* store function values for generalized Armijo condition */
	double fx_max; /* max(fs) */
	double xi, beta_bar, beta_hat, beta; /* for Armijo condition */
	double dd; /* dot(d,d) */
	double gd; /* dot(g,d) */
	double dAd; /* dot(Ad,d) */
	double alpha_bb; /* BB step-size */

	/* initial step-size */
	alpha_bb = setting.alphainit;

	x = x0; /* set approximation as initial */
	data->feasibleset->project(x); /* project initial approximation to feasible set */

	/* compute gradient, g = A*x-b */
	g = A*x; 
	hessmult += 1; /* there was muliplication by A */
	g -= b;

	/* compute function value */
 	fx = get_function_value();
	/* initialize fs */
	fs.init(fx);	

	/* main cycle */
	while(it < setting.maxit){

		/* d = x - alpha_bb*g, see next step, it will be d = P(x - alpha_bb*g) - x */
		d = x - alpha_bb*g;

		/* d = P(d) */
		data->feasibleset->project(d);

		/* d = d - x */
		d -= x;

		/* Ad = A*d */
		Ad = A*d;
		hessmult += 1; /* there was multiplication by A */

		/* dd = dot(d,d) */
		/* dAd = dot(Ad,d) */
		/* gd = dot(g,d) */
		dd = dot(d,d);
		dAd = dot(Ad,d);
		gd = dot(g,d);

		/* stopping criteria */
		if(dd < setting.eps){
			break;
		}
		
		/* fx_max = max(fs) */
		fx_max = fs.get_max();	
		
		/* compute step-size from A-condition */
		xi = (fx_max - fx)/dAd;
		beta_bar = -gd/dAd;
		beta_hat = setting.gamma*beta_bar + std::sqrt(setting.gamma*setting.gamma*beta_bar*beta_bar + 2*xi);

		/* beta = min(sigma2,beta_hat) */
		if(beta_hat < setting.sigma2){
			beta = beta_hat;
		} else {
			beta = setting.sigma2;
		}

		/* update approximation and gradient */
		x += beta*d; /* x = x + beta*d */
		g += beta*Ad; /* g = g + beta*Ad */
		
		/* compute new function value using gradient and update fs list */
		fx = get_function_value();
		fs.update(fx);

		/* update BB step-size */
		alpha_bb = dd/dAd;

		/* print data */
		if(DEBUG_MODE >= 10){
			std::cout << "x: " << x << std::endl;
			std::cout << "d: " << d << std::endl;
			std::cout << "g: " << g << std::endl;
			std::cout << "Ad: " << Ad << std::endl;
			
		}

		/* print progress of algorithm */
		if(DEBUG_MODE >= 4){
			std::cout << "\033[33m   it = \033[0m" << it;
			std::cout << ", \t\033[36mfx = \033[0m" << fx;
			std::cout << ", \t\033[36mdd = \033[0m" << dd << std::endl;
		}

		if(DEBUG_MODE >= 5){
			std::cout << "\033[36m    alpha_bb = \033[0m" << alpha_bb << ",";
			std::cout << "\033[36m dAd = \033[0m" << dAd << ",";
			std::cout << "\033[36m gd = \033[0m" << gd << std::endl;
			
			std::cout << "\033[36m    fx = \033[0m" << fx << ",";
			std::cout << "\033[36m fx_max = \033[0m" << fx_max << ",";
			std::cout << "\033[36m xi = \033[0m" << xi << std::endl;
			
			std::cout << "\033[36m    beta_bar = \033[0m" << beta_bar << ",";
			std::cout << "\033[36m beta_hat = \033[0m" << beta_hat << ",";
			std::cout << "\033[36m beta = \033[0m" << beta << std::endl;
			
		}
		
		/* increase iteration counter */
		it += 1;


	} /* main cycle end */

	/* very short info */
	if(DEBUG_MODE >= 3){
		Message_info_value("   - it    = ",it);
//		Message_info_time("   - time  = ",this->timer_total.get_value_last());

	}

	
}

/* compute function value in given approximation */
template<class VectorBase>
double SPGQPSolver<VectorBase>::get_function_value(GeneralVector<VectorBase> *px){
	if(DEBUG_MODE >= 11) std::cout << "(SPGQPSolver)FUNCTION: get_function_value(x)" << std::endl;
	
	double fx = std::numeric_limits<double>::max();

	/* we have nothing - compute fx using full formula fx = 0.5*dot(A*x,x) - dot(b,x) */

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);
	typedef GeneralMatrix<VectorBase> (&pMatrix);

	/* pointers to data */
	pMatrix A = *(data->A);
	pVector b = *(data->b);
	pVector x = *px;
	pVector Ax = *(this->temp);
		
	double xAx, xb;

	Ax = A*x;
		 
	xAx = dot(Ax,x);
	fx = 0.5*xAx;
		 
	xb = dot(x,b);
	fx -= xb;

	return fx;
}

/* compute function value using inner *x and already computed *g */
template<class VectorBase>
double SPGQPSolver<VectorBase>::get_function_value(){
	if(DEBUG_MODE >= 11) std::cout << "(SPGQPSolver)FUNCTION: get_function_value()" << std::endl;
	
	double fx = std::numeric_limits<double>::max();

	/* I don't want to write (*x) as a vector, therefore I define following pointer types */
	typedef GeneralVector<VectorBase> (&pVector);

	/* pointers to data */
	pVector g = *(this->g);
	pVector x = *(result->x);
	pVector b = *(data->b);
	pVector temp = *(this->temp);

	/* use computed gradient in this->g to compute function value */
	temp = g - b;
	fx = 0.5*dot(temp,x);

	return fx;	
}



/* ---------- SPGQPSolver_fs -------------- */

/* constructor */
SPGQPSolver_fs::SPGQPSolver_fs(int new_m){
	this->m = new_m;
}

/* init the list with function values using one initial fx */
void SPGQPSolver_fs::init(double fx){
	this->fs_list.resize(this->m, fx);
}

/* get the size of the list */
int SPGQPSolver_fs::get_size(){
	return this->m;
}

/* get the value of max value in the list */
double SPGQPSolver_fs::get_max(){
	std::list<double>::iterator it;
	it = std::max_element(this->fs_list.begin(), this->fs_list.end());
	return *it;
}

/* update the list by new value - pop the first and push the new value (FIFO) */
void SPGQPSolver_fs::update(double new_fx){
	this->fs_list.pop_back();
	this->fs_list.push_front(new_fx);
}

/* print the content of the list */
std::ostream &operator<<(std::ostream &output, SPGQPSolver_fs fs)
{
	int j, list_size;
	std::list<double>::iterator it; /* iterator through list */

	it = fs.fs_list.begin();
	list_size = fs.fs_list.size(); /* = m? */
	
	output << "[ ";
	/* for each component go throught the list */
	for(j=0;j<list_size;j++){
		output << *it;
		if(j < list_size-1){ 
				/* this is not the last node */
				output << ", ";
				it++;
		}
	}
	output << " ]";
			
	return output;
}



} /* end namespace */

#endif