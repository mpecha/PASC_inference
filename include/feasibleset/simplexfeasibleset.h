#ifndef SIMPLEXFEASIBLESET_H
#define	SIMPLEXFEASIBLESET_H

/* for debugging, if >= 100, then print info about ach called function */
extern int DEBUG_MODE;

#include <iostream>
#include "generalfeasibleset.h"
#include "algebra.h"

typedef petscvector::PetscVector GlobalPetscVector;
typedef minlin::threx::HostVector<double> HostMinLinVector;
typedef minlin::threx::DeviceVector<double> DeviceMinLinVector;


namespace pascinference {


/* Simplex Feasible Set */
/* \Omega = 
 *  \lbrace x \in \mathbb{R}^{KT}: 
 *    \sum\limits_{k=0}^{K-1} x_{t+kT} = 1, \forall t = 0,\dots,T-1 
 *    x \geq 0
 *  \rbrace
 */  
template<class VectorBase>
class SimplexFeasibleSet: public GeneralFeasibleSet<VectorBase> {
	private:
		void sort_bubble(double *x, int n);
		void get_projection_sub(double *x_sub, int n);
		
		#ifdef USE_PETSC
		bool petsc_projection_init; /* if the initialization of projection was not performed, then = false */
		IS petsc_projection_is; 
		int petsc_projection_Townership_low, petsc_projection_Townership_high;
		#endif

	public:
		SimplexFeasibleSet(int T, int K);
		~SimplexFeasibleSet();

		void print(std::ostream &output) const;

		/* variables */
		int T; 
		int K; 
		
		void project(GeneralVector<VectorBase> &x);


};

/* sorry, __device__ and __global__ functions cannot be members of template class */
#ifdef USE_GPU
__device__ void SimplexFeasibleSet_device_sort_bubble(double *x, int n);
__global__ void SimplexFeasibleSet_kernel_get_projection_sub(double *x, int T, int K);
#endif

} // end of namespace

/* ------------- implementation ----------- */
//TODO: move to impls

namespace pascinference {

/* constructor */
template<class VectorBase>
SimplexFeasibleSet<VectorBase>::SimplexFeasibleSet(int Tnew, int Knew){
	if(DEBUG_MODE >= 100) std::cout << "(SimplexFeasibleSet)CONSTRUCTOR" << std::endl;

	/* set initial content */
	this->T = Tnew;
	this->K = Knew;
	
	this->petsc_projection_init = false;
}

/* destructor */
template<class VectorBase>
SimplexFeasibleSet<VectorBase>::~SimplexFeasibleSet(){
	if(DEBUG_MODE >= 100) std::cout << "(SimplexFeasibleSet)DESTRUCTOR" << std::endl;
	
}

/* print info about feasible set */
template<class VectorBase>
void SimplexFeasibleSet<VectorBase>::print(std::ostream &output) const {
	output << " SimplexFeasibleSet" << std::endl;
	
	/* give information about presence of the data */
	output << "  - T:     " << T << std::endl;
	output << "  - K:     " << K << std::endl;
		
}


/* -------- minlin::threx::HostVector ---------- */
template<>
void SimplexFeasibleSet<HostMinLinVector>::project(GeneralVector<HostMinLinVector> &x) {
	if(DEBUG_MODE >= 100) std::cout << "(SimplexFeasibleSet)FUNCTION: project HostMinLinVector" << std::endl;

	int t,k;
	double x_sub[K];  /* GammaVector x_sub(K); */

	for(t=0;t<T;t++){
		/* cut x_sub from x */
		for(k=0;k<K;k++){
			x_sub[k] = x(k*T+t);
		}
		
		/* compute subprojection */
		get_projection_sub(x_sub, this->K);

		/* add x_sub back to x */
		for(k=0;k<K;k++){
			x(k*T+t) = x_sub[k];
		}
	}
}

/* project x_sub to feasible set defined by equality and inequality constraints
 * sum(x_sub) = 1
 * x_sub >= 0
 */
template<class VectorBase>
void SimplexFeasibleSet<VectorBase>::get_projection_sub(double *x_sub, int n){
	int i;

	bool is_inside = true;
	double sum = 0.0;
	
	/* control inequality constraints */
	for(i = 0; i < n; i++){ // TODO: could be performed parallely  
		if(x_sub[i] < 0.0){
			is_inside = false;
		}
		sum += x_sub[i];
	}

	/* control equality constraints */
	if(sum != 1){ 
		is_inside = false;
	}


	/* if given point is not inside the feasible domain, then do projection */
	if(!is_inside){
		int j;
		/* compute sorted x_sub */
		double y[n], sum_y;
		for(i=0;i<n;i++){
			y[i] = x_sub[i]; 
		}
		sort_bubble(y,n);

		/* now perform analytical solution of projection problem */	
		double t_hat = 0.0;
		i = n - 1;
		double ti;

		while(i >= 1){
			/* compute sum(y) */
			sum_y = 0.0;
			for(j=i;j<n;j++){ /* sum(y(i,n-1)) */
				sum_y += y[j];
			}
				
			ti = (sum_y - 1.0)/(double)(n-i);
			if(ti >= y[i-1]){
				t_hat = ti;
				i = -1; /* break */
			} else {
				i = i - 1;
			}
		}

		if(i == 0){
			t_hat = (sum-1.0)/(double)n; /* uses sum=sum(x_sub) */
		}
    
		for(i = 0; i < n; i++){ // TODO: could be performed parallely  
			/* (*x_sub)(i) = max(*x_sub-t_hat,0); */
			ti = x_sub[i] - t_hat;	
			if(ti > 0.0){
				x_sub[i] = ti;
			} else {
				x_sub[i] = 0.0;
			}
		}
	}
}

/* sort values of scalar vector */
template<class VectorBase>
void SimplexFeasibleSet<VectorBase>::sort_bubble(double *x, int n){
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



/* -------- minlin::threx::DeviceVector ---------- */
#ifdef USE_GPU

template<>
void SimplexFeasibleSet<DeviceMinLinVector>::project(GeneralVector<DeviceMinLinVector> &x) {

	/* call projection using kernel */
	double *xp = x.pointer();
	
	// TODO: compute optimal nmb of threads/kernels
	SimplexFeasibleSet_kernel_get_projection_sub<<<this->T, 1>>>(xp,this->T,this->K);
	
	/* synchronize kernels, if there is an error with cuda, then it will appear here */ 
	gpuErrchk( cudaDeviceSynchronize() );	
	

}

__device__
void SimplexFeasibleSet_device_sort_bubble(double *x, int n){
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

__global__
void SimplexFeasibleSet_kernel_get_projection_sub(double *x, int T, int K){
	/* compute my id */
	int t = blockIdx.x*blockDim.x + threadIdx.x;

	if(t<T){
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
			SimplexFeasibleSet_device_sort_bubble(y,K);

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

	/* if t >= N then relax and do nothing */	

}

#endif



/* -------- petscvector::PetscVector ---------- */
#ifdef USE_PETSC

template<>
void SimplexFeasibleSet<GlobalPetscVector>::project(GeneralVector<GlobalPetscVector> &x) {

	/* initialization - how much I will compute? */
	if(!this->petsc_projection_init){
		if(DEBUG_MODE >= 100){
			Message("  - initialization of projection");
		}
		
		this->petsc_projection_init = true;
		
		/* try to make a global vector of length T and then get the indexes of begin and end of local portion */
		GlobalPetscVector TVector(T);

		/* get the ownership range - now I know how much I will calculate from the time-series */
		TVector.get_ownership(&(this->petsc_projection_Townership_low), &(this->petsc_projection_Townership_high));

		/* destroy testing vector - it is useless now */
//		~TVector();

	}

	if(DEBUG_MODE >= 100){
		std::cout << "     my ownership: [" << this->petsc_projection_Townership_low << ", " << this->petsc_projection_Townership_high << "]" << std::endl;
	}

	int t;
	GlobalPetscVector x_sub;
	double *x_sub_arr;
	/* go throught local portion of time-serie and perform the projection */
	for(t = this->petsc_projection_Townership_low; t < this->petsc_projection_Townership_high; t++){
		/* prepare index set [low, low + T, ... , low + (K-1)*T ] */
		ISCreateStride(PETSC_COMM_SELF, this->K, this->petsc_projection_Townership_low + t, this->T, &(this->petsc_projection_is));

		/* get the subvector from global vector */
		x_sub = x(petsc_projection_is);

		/* get the array */
		x_sub.get_array(&x_sub_arr);

		/* perform the projection on this subvector */
		get_projection_sub(x_sub_arr, this->K);

		/* print the array of subvector */
		if(DEBUG_MODE >= 100){
			int i;
			std::cout << "      xsub_" << t << " = [ ";
			for(i=0;i<this->K;i++){
				std::cout << x_sub_arr[i];
				if(i < this->K-1) std::cout << ", ";
			}
			std::cout << " ]" << std::endl;
		}

		/* restore the array */
		x_sub.restore_array(&x_sub_arr);

		x(petsc_projection_is) = x_sub;

		//TODO: deal with x_sub
	}

}

#endif



} /* end namespace */

#endif