/** @file simplex_local_vec.h
 *  @brief simplex feasible set for VEC with local problems
 *
 *  @author Lukas Pospisil
 */

#ifndef PASC_SIMPLEXFEASIBLESET_LOCAL_VEC_H
#define	PASC_SIMPLEXFEASIBLESET_LOCAL_VEC_H

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include <iostream>
#include "generalfeasibleset.h"

#ifndef USE_PETSC
 #error 'SIMPLEXFEASIBLESET_LOCAL_VEC is for PETSC'
#endif

#include <../src/vec/vec/impls/seq/seqcuda/cudavecimpl.h>

namespace pascinference {


/** \class SimplexFeasibleSet_Local
 *  \brief class for manipulation with simplex set
 *
 *  Provides structure for manipulation with simplex feasible set, i.e.
 * \f[
 * \Omega = 
 *  \left\lbrace x \in R^{KT}: 
 *    \forall t = 0,\dots,T-1: \sum\limits_{k=0}^{K-1} x_{t+kT} = 1,  
 *    x \geq 0
 *  \right\rbrace
 *	\f]
 * 
*/
class SimplexFeasibleSet_Local: public GeneralFeasibleSet<PetscVector> {
	private:
		
		/** @brief sort array using bubble sort
		 * 
		 * @param x array with values
		 * @param n size of array
		*/ 		
		void sort_bubble(double *x, int n);

		/** @brief compute projection to one simplex subset
		 * 
		 * @param x_sub values of subvector in array
		 * @param n size of subvector
		*/ 		
		void project_sub(int t, double *x, int T, int K);

	public:
		/** @brief default constructor
		*/ 	
		SimplexFeasibleSet_Local(int T, int K_local);
		
		/** @brief default destructor
		 */ 
		~SimplexFeasibleSet_Local();

		/** @brief print properties of this feasible set
		 * 
		 * @param output where to print
		 */ 
		void print(ConsoleOutput &output) const;

		/** @brief get name of this feasible set
		 */
		virtual std::string get_name() const;

		/* variables */
		int T; /**< number of disjoint simplex subsets */
		int K_local; /**< size of each simplex subset */
		
		/** @brief compute projection onto feasible set
		 * 
		 * @param x point which will be projected
		 */		
		void project(Vec x);


		
		
};

/* cuda kernels cannot be a member of class */
#ifdef USE_CUDA
__device__ void device_sort_bubble(double *x, int n);
__global__ void project_kernel(double *x, int T, int K);
__global__ void project_kernel_test(double *x, int T, int K);
#endif

} // end of namespace

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {

/* constructor */
SimplexFeasibleSet_Local::SimplexFeasibleSet_Local(int Tnew, int Knew){
	LOG_FUNC_BEGIN

	/* set initial content */
	this->T = Tnew;
	this->K_local = Knew;

	LOG_FUNC_END
}

/* general destructor */
SimplexFeasibleSet_Local::~SimplexFeasibleSet_Local(){
	LOG_FUNC_BEGIN
	
	LOG_FUNC_END	
}

/* print info about feasible set */
void SimplexFeasibleSet_Local::print(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN
	
	output <<  this->get_name() << std::endl;
	
	/* give information about presence of the data */
	output <<  " - T:       " << T << std::endl;
	output <<  " - K_local: " << K_local << std::endl;

	LOG_FUNC_END
}

std::string SimplexFeasibleSet_Local::get_name() const {
	return "SimplexFeasibleSet_Local";
}

void SimplexFeasibleSet_Local::project(Vec x) {
	LOG_FUNC_BEGIN
	
	/* get local array */
	double *x_arr;
	

#ifdef USE_CUDA
/* use kernel to compute projection */
	TRY( VecCUDAGetArrayReadWrite(x,&x_arr) );

	//TODO: here should be actually the comparison of Vec type! not simple use_gpu
	gpuErrchk(cudaDeviceSynchronize());
	project_kernel<<<T, 1>>>(x_arr,T,K_local);
	gpuErrchk(cudaDeviceSynchronize());

	TRY( VecCUDARestoreArrayReadWrite(x,&x_arr) );
#else
	TRY( VecGetArray(x,&x_arr) );

	/* use openmp */
	int t;
	#pragma omp parallel for private(t)	
	for(t=0;t<T;t++){
		project_sub(t,x_arr,T,K_local);
	}
	
	TRY( VecRestoreArray(x,&x_arr) );
	
#endif

	LOG_FUNC_END
}


void SimplexFeasibleSet_Local::sort_bubble(double *x, int n){
	int i;
	int m = n;
	int mnew;
	double swap;

	while(m > 0){
		/* Iterate through x */
		mnew = 0;
		for(i=1;i<m;i++){
			/* Swap elements in wrong order */
			if (x[i] < x[i - 1]){
				swap = x[i];
				x[i] = x[i-1];
				x[i-1] = swap;
				mnew = i;
			}
        }
		m = mnew;
    }
}

/* this will be a kernel function which computes a projection of a point onto simplex in nD
 *
 * just for curiosity more details: 
 * take K-dimensional vector x[t,t+T,t+2T,...t+(K-1)T] =: p
 * and compute projection
 * P(p) = arg min || p - y ||_2
 * subject to constraints (which define simplex)
 * y_0 + ... y_{K-1} = 1
 * y_i >= 0 for all i=0,...,K-1
 *
 * in practical applications K is much more lower number than T
 * K - number of clusters (2 - 10^2)
 * T - length of time-series (10^5 - 10^9) 
 */ 
void SimplexFeasibleSet_Local::project_sub(int t, double *x, int T, int K){
	if(t<T){ /* maybe we call more than T kernels */
		int k;

		bool is_inside = true;
		double sum = 0.0;
	
		/* control inequality constraints */
		for(k = 0; k < K; k++){ // TODO: could be performed parallely  
			if(x[k*T+t] < 0.0){
				is_inside = false;
			}
			sum += x[k*T+t];
		}

		/* control equality constraints */
		if(sum != 1){ 
			is_inside = false;
		}

		/* if given point is not inside the feasible domain, then do projection */
		if(!is_inside){
			int j,i;
			/* compute sorted x_sub */
			double *y = new double[K];
			double sum_y;
			for(k=0;k<K;k++){
				y[k] = x[k*T+t]; 
			}
			sort_bubble(y,K);

			/* now perform analytical solution of projection problem */	
			double t_hat = 0.0;
			i = K - 1;
			double ti;

			while(i >= 1){
				/* compute sum(y) */
				sum_y = 0.0;
				for(j=i;j<K;j++){ /* sum(y(i,n-1)) */
					sum_y += y[j];
				}
				
				ti = (sum_y - 1.0)/(double)(K-i);
				if(ti >= y[i-1]){
					t_hat = ti;
					i = -1; /* break */
				} else {
					i = i - 1;
				}
			}

			if(i == 0){
				t_hat = (sum-1.0)/(double)K; /* uses sum=sum(x_sub) */
			}
    
			for(k = 0; k < K; k++){ // TODO: could be performed parallely  
				/* (*x_sub)(i) = max(*x_sub-t_hat,0); */
				ti = x[k*T+t] - t_hat;	
				if(ti > 0.0){
					x[k*T+t] = ti;
				} else {
					x[k*T+t] = 0.0;
				}
			}
			
			delete y;
		}
		
	}

	/* if t >= T then relax and do nothing */	
}




/* kernels in cuda */
#ifdef USE_CUDA

__device__ void device_sort_bubble(double *x, int n){
	int i;
	int m=n;
	int mnew;
	double swap;

	while(m > 0){
		/* Iterate through x */
		mnew = 0;
		for(i=1;i<m;i++){
			/* Swap elements in wrong order */
			if (x[i] < x[i - 1]){
				swap = x[i];
				x[i] = x[i-1];
				x[i-1] = swap;
				mnew = i;
			}
	        }
		m = mnew;
	}
}

__global__ void project_kernel_test(double *x, int T, int K){
    int t = blockIdx.x*blockDim.x + threadIdx.x;
    
    if(t<T){
	x[t] = 1.5;
    }

}
/* this is a kernel function which computes a projection of a point onto simplex in nD
 *
 * take K-dimensional vector x[t,t+T,t+2T,...t+(K-1)T] =: p
 * and compute projection
 * P(p) = arg min || p - y ||_2
 * subject to constraints (which define simplex)
 * y_0 + ... y_{K-1} = 1
 * y_i >= 0 for all i=0,...,K-1
 *
 * in practical applications K is much more lower number than T
 * K - number of clusters (2 - 10^2)
 * T - length of time-series (10^5 - 10^9) 
 */ 
__global__ void project_kernel(double *x, int T, int K){
	int t = blockIdx.x*blockDim.x + threadIdx.x; /* compute my id */

	if(t<T){ /* maybe we call more than T kernels */
		int k;

		bool is_inside = true;
		double sum = 0.0;
	
		/* control inequality constraints */
		for(k = 0; k < K; k++){ // TODO: could be performed parallely  
			if(x[k*T+t] < 0.0){
				is_inside = false;
			}
			sum += x[k*T+t];
		}

		/* control equality constraints */
		if(sum != 1){ 
			is_inside = false;
		}

		/* if given point is not inside the feasible domain, then do projection */
		if(!is_inside){
			int j,i;
			/* compute sorted x_sub */
			double *y = new double[K];
			
			double sum_y;
			for(k=0;k<K;k++){
				y[k] = x[k*T+t]; 
			}
			device_sort_bubble(y,K);

			/* now perform analytical solution of projection problem */	
			double t_hat = 0.0;
			i = K - 1;
			double ti;

			while(i >= 1){
				/* compute sum(y) */
				sum_y = 0.0;
				for(j=i;j<K;j++){ /* sum(y(i,n-1)) */
					sum_y += y[j];
				}
				
				ti = (sum_y - 1.0)/(double)(K-i);
				if(ti >= y[i-1]){
					t_hat = ti;
					i = -1; /* break */
				} else {
					i = i - 1;
				}
			}

			if(i == 0){
				t_hat = (sum-1.0)/(double)K; /* uses sum=sum(x_sub) */
			}
    
			for(k = 0; k < K; k++){ // TODO: could be performed parallely  
				/* (*x_sub)(i) = max(*x_sub-t_hat,0); */
				ti = x[k*T+t] - t_hat;	
				if(ti > 0.0){
					x[k*T+t] = ti;
				} else {
					x[k*T+t] = 0.0;
				}
			}
			
			delete[] y;
			//free(y);
		}
		
	}

	/* if t >= T then relax and do nothing */	


}

#endif



} /* end namespace */

#endif