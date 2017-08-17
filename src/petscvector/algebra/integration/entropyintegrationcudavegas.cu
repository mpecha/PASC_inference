#include "external/petscvector/petscvector.cuh"
#include "external/petscvector/algebra/integration/entropyintegrationcudavegas.h"

#define Norum 0.0000000001*2.32830643653869628906

namespace pascinference {
namespace algebra {

const int xdim_max = 10;
const int nd_max = 50;

__device__ __constant__ double g_xi[xdim_max][nd_max];

__global__ void gVegasCallFunc(int g_ng, int g_npg, int g_nd, double g_xjac, double device_dxg, int g_nCubes, double *device_xl, double *device_dx, double* device_Fval, int* device_IAval, int xdim, int number_of_integrals, int n, double *device_lambda, int *device_matrix_D_arr, int id_integral);
__device__ void func_entropy(double *cvalues_out, double *cx, int xdim, int number_of_integrals, int n, double *device_lambda, int *device_matrix_D_arr, int id_integral);
__device__ __host__ __forceinline__ void fxorshift128(unsigned int seed, int n, double* a);

__global__ void gVegasFill(double *device_d, double *device_ti, double *device_tsi, int nCubes, double *device_Fval, int *device_IAval, int npg, int nd, int mds, int xdim);


__global__ void print_kernel(int xdim, int number_of_moments, int number_of_integrals, double *device_lambda, int *device_matrix_D_arr){
	printf("============ from GPU =========== \n");
	printf("xdim                : %d \n", xdim);
	printf("number_of_moments   : %d \n", number_of_moments);
	printf("number_of_integrals : %d \n", number_of_integrals);
	printf("device_lambda            : [");
	for(int i=0; i < number_of_moments-1; i++){
		printf("%f", device_lambda[i]);
		if(i<number_of_moments-2){
			printf(", ");
		}
	}
	printf("] \n");
	printf("matrix D: \n");
	for(int i_moment=0;i_moment < number_of_moments; i_moment++){
		for(int i_xdim=0;i_xdim < xdim; i_xdim++){
			printf("%d", device_matrix_D_arr[ i_moment*xdim + i_xdim]);
			
			if(i_xdim < xdim-1) printf(", ");
		}
		printf("\n");
	}
}

__global__ void kernel_set_zero(double *arr1, int size){
	
	//TODO: make it in a different way!!!
	for(int i=0;i<size;i++){
		arr1[i] = 0.0;
	}

}

EntropyIntegrationCudaVegas<PetscVector>::ExternalContent::ExternalContent(int xdim, int number_of_moments, int number_of_integrals, int *matrix_D_arr) {
	LOG_FUNC_BEGIN

	/* restart timers */
	this->timerVegasCall.restart();
	this->timerVegasMove.restart();
	this->timerVegasFill.restart();
	this->timerVegasRefine.restart();

	/* store host variables */
	this->xdim = xdim;
	this->number_of_moments = number_of_moments;
	this->number_of_integrals = number_of_integrals;
	this->matrix_D_arr = matrix_D_arr;

	/* allocate host arrays */
	this->sd = new double[this->get_number_of_integrals()];
	this->chi2a = new double[this->get_number_of_integrals()];
	this->xl = new double[xdim_max];
	this->xu = new double[xdim_max];
	this->dx = new double[xdim_max];

	/* allocate variables on cuda */
	gpuErrchk(cudaMalloc((void**)&(this->device_xl), xdim_max*sizeof(double)));
	gpuErrchk(cudaMalloc((void**)&(this->device_dx), xdim_max*sizeof(double)));
	gpuErrchk(cudaMalloc((void**)&(this->device_lambda), (number_of_moments-1)*sizeof(double)));
	gpuErrchk(cudaMalloc((void**)&(this->device_matrix_D_arr), number_of_moments*xdim*sizeof(int)));

	/* copy variables to CUDA */
	gpuErrchk( cudaMemcpy(this->device_matrix_D_arr, matrix_D_arr, number_of_moments*xdim*sizeof(int), cudaMemcpyHostToDevice ) );
	cudaThreadSynchronize(); /* wait for synchronize */


	LOG_FUNC_END
}

EntropyIntegrationCudaVegas<PetscVector>::ExternalContent::~ExternalContent(){
	LOG_FUNC_BEGIN

	/* free host arrays */
	free(this->sd);
	free(this->chi2a);
	free(this->xl);
	free(this->xu);
	free(this->dx);

	/* free cuda variables */
	gpuErrchk(cudaFree(this->device_xl));
	gpuErrchk(cudaFree(this->device_dx));
	gpuErrchk(cudaFree(this->device_lambda));
	gpuErrchk(cudaFree(this->device_matrix_D_arr));

	LOG_FUNC_END
}


void EntropyIntegrationCudaVegas<PetscVector>::ExternalContent::cuda_gVegas(double *avgi, double *lambda_arr) {
	LOG_FUNC_BEGIN

	/* copy given lambda to GPU */
	gpuErrchk( cudaMemcpy(this->device_lambda, lambda_arr, (number_of_moments-1)*sizeof(double), cudaMemcpyHostToDevice ) );
	cudaThreadSynchronize(); /* wait for synchronize */

	//TODO: temp
//	print_kernel<<<1, 1>>>(xdim, number_of_moments, number_of_integrals, this->device_lambda, this->device_matrix_D_arr);
//	cudaThreadSynchronize(); /* wait for synchronize */

	/* through all integrals which has to be computed */
	for(int id_integral=0;id_integral<number_of_integrals;id_integral++){

		int mds = 1;
		int nprn = 1;
		const double alph = 1.5;

		int it;
		int nd;
		int ng;
		int ndo;
		int npg;
		int nCubes;
		double calls;
		double dxg;
		double dnpg;
		double dv2g;
		double ti;
		double tsi;
	
		int nGridSizeX, nGridSizeY;
		int nBlockTot;
	
		double xi[xdim_max][nd_max];
		double xin[nd_max];

		double xnd;
		double xjac;
	
		double si;
		double si2;
		double swgt;
		double schi;
	
		for (int i=0;i< this->xdim;i++) {
			this->xl[i] = 0.;
			this->xu[i] = 1.;
		}

		for (int j=0; j < this->xdim; j++) {
			xi[j][0] = 1.;
		}

		/* entry vegas1 */
		it = 0;

		/* entry vegas2 */
		nd = nd_max;
		ng = 1;
   
		npg = 0;
		if (mds!=0) {
			ng = (int)pow((0.5*(double)(this->ncall)),1./(double)(this->xdim));
			mds = 1;
			if (2*ng>=nd_max) {
				mds = -1;
				npg = ng/(double)nd_max+1;
				nd = ng/(double)npg;
				ng = npg*nd;
			}
		}

		nCubes = (unsigned)(pow(ng,this->xdim));

		npg = ncall/(double)nCubes;
		if(npg < 2){
			npg = 2;
		}
		calls = (double)(npg*nCubes);

		unsigned nCubeNpg = nCubes*npg;

		if (debug_print_integration_inner) {
			coutMaster << std::endl;
			coutMaster << " << vegas internal parameters >> " << std::endl;
			coutMaster << "            ng: " << std::setw(12) << ng << std::endl;
			coutMaster << "            nd: " << std::setw(12) << nd << std::endl;
			coutMaster << "           npg: " << std::setw(12) << npg << std::endl;
			coutMaster << "        nCubes: " << std::setw(12) << nCubes << std::endl;
			coutMaster << "    nCubes*npg: " << std::setw(12) << nCubeNpg << std::endl;
		}

		dxg = 1./(double)ng;
		dnpg = (double)npg;
		dv2g = calls*calls*pow(dxg,this->xdim)*pow(dxg,this->xdim)/(dnpg*dnpg*(dnpg-1.));
		xnd = (double)nd;
		dxg *= xnd;
		xjac = 1./(double)calls;
		for (int j=0;j<this->xdim;j++) {
			dx[j] = this->xu[j]-this->xl[j];
			xjac *= dx[j];
		}

		/* tranfer data to GPU */
		ndo = 1;

		if (nd!=ndo) {
			double rc = (double)ndo/xnd;

			for (int j=0;j<this->xdim;j++) {
				int k = -1;
				double xn = 0.;
				double dr = 0.;
				int i = k;
				k++;
				dr += 1.;
				double xo = xn;
				xn = xi[j][k];

				while (i<nd-1) {
					while (dr<=rc) {
						k++;
						dr += 1.;
						xo = xn;
						xn = xi[j][k];
					}
					i++;
					dr -= rc;
					xin[i] = xn - (xn-xo)*dr;
				}

				for (int i=0;i<nd-1;i++) {
					xi[j][i] = (double)xin[i];
				}
				xi[j][nd-1] = 1.;
			}
			ndo = nd;
		}

		/* transfer data to GPU */
		gpuErrchk( cudaMemcpy(this->device_xl, this->xl, xdim_max*sizeof(double), cudaMemcpyHostToDevice ) );
		gpuErrchk( cudaMemcpy(this->device_dx, this->dx, xdim_max*sizeof(double), cudaMemcpyHostToDevice ) );
		gpuErrchk(cudaMemcpyToSymbol(g_xi, xi, sizeof(xi)));
		cudaThreadSynchronize();
	
		if (debug_print_integration_inner) {
			coutMaster << std::endl;
			coutMaster << " << input parameters for vegas >>" << std::endl;
			coutMaster << "     xdim =" << std::setw(3) << this->xdim
						<< "   ncall = " << std::setw(10) << this->ncall <<std::endl;
			coutMaster << "     it   =  0"
						<< "   itmx = " << std::setw(5) << this->itmx << std::endl;
			coutMaster << "     acc  = " << std::fixed
						<< std::setw(9) << std::setprecision(3) << this->acc << std::endl;
			coutMaster << "     mds  = " << std::setw(3) << mds
						<< "   nd = " << std::setw(4) << nd <<std::endl;
			for(int j=0; j < this->xdim; j++){
				coutMaster << "    (xl,xu)= ( " << std::setw(6) << std::fixed
						<< this->xl[j] << ", " << this->xu[j] << " )" << std::endl;
			}
		}	

		/* entry vegas3 */
		it = 0;
		si = 0.;
		si2 = 0.;
		swgt = 0.;
		schi = 0.;

		/* --------------------------
		* Set up kernel variables
		* --------------------------
		*/
		const int nGridSizeMax =  65535;
   
		dim3 ThBk(nBlockSize);
	
		nBlockTot = (nCubeNpg-1)/nBlockSize+1;
		nGridSizeY = (nBlockTot-1)/nGridSizeMax+1;
		nGridSizeX = (nBlockTot-1)/nGridSizeY+1;
		dim3 BkGd(nGridSizeX, nGridSizeY);

		if (debug_print_integration_inner) {
			coutMaster << std::endl;
			coutMaster << " << kernel parameters for CUDA >> " << std::endl;
			coutMaster << "       Block size           = " << std::setw(12) << ThBk.x << std::endl;
			coutMaster << "       Grid size            = " << std::setw(12) << BkGd.x
						<< " x " << BkGd.y << std::endl;
			int nThreadsTot = ThBk.x*BkGd.x*BkGd.y;
			coutMaster << "     Actual Number of calls = " << std::setw(12)
						<< nThreadsTot << std::endl;
			coutMaster << "   Required Number of calls = " << std::setw(12)
						<< nCubeNpg << " ( " << std::setw(6) << std::setprecision(2)
						<< 100.*(double)nCubeNpg/(double)nThreadsTot << "%)" <<std::endl;
			coutMaster << std::endl;
		}

		int sizeFval;
		int sizeIAval;
		int sized;
		
		double* host_Fval;
		int* host_IAval;
		double* host_d;

		double* device_Fval;
		int* device_IAval;
		double* device_d;
		double *device_ti;
		double *device_tsi;

		/* set sizes */
		sizeFval = nCubeNpg*sizeof(double);
		sizeIAval = nCubeNpg*xdim*sizeof(int);
		sized = xdim*nd*sizeof(double);

		/* CPU */
		gpuErrchk(cudaMallocHost((void**)&host_Fval, sizeFval));
		gpuErrchk(cudaMallocHost((void**)&host_IAval, sizeIAval));
		gpuErrchk(cudaMallocHost((void**)&host_d, sized));
		memset(host_Fval, '\0', sizeFval);
		memset(host_IAval, '\0', sizeIAval);

		/* GPU */
		gpuErrchk(cudaMalloc((void**)&device_Fval, sizeFval));
		gpuErrchk(cudaMalloc((void**)&device_IAval, sizeIAval));
		gpuErrchk(cudaMalloc((void**)&device_d, sized));
		gpuErrchk(cudaMalloc((void**)&device_ti, sizeof(double)));
		gpuErrchk(cudaMalloc((void**)&device_tsi, sizeof(double)));

		/* perform main iterations */
		do {
			it++;
			
			/* call integral function */
			timerVegasCall.start();
			 /* double* device_Fval, int* device_IAval, int xdim, int number_of_integrals, int number_of_moments, int *device_lambda, int *device_matrix_D_arr */
			gVegasCallFunc<<<BkGd, ThBk>>>(ng, npg, nd, xjac, dxg, nCubes, device_xl, device_dx, device_Fval, device_IAval, this->xdim, this->number_of_integrals, this->number_of_moments-1, this->device_lambda, this->device_matrix_D_arr, id_integral);
			cudaThreadSynchronize();
			gpuErrchk( cudaDeviceSynchronize() );
			timerVegasCall.stop();
	
			/* move computed results */
			timerVegasMove.start();
			gpuErrchk(cudaMemcpy(host_Fval, device_Fval,  sizeFval,
	                               cudaMemcpyDeviceToHost));
			gpuErrchk(cudaMemcpy(host_IAval, device_IAval,  sizeIAval,
	                               cudaMemcpyDeviceToHost));
			timerVegasMove.stop();
	
			/* fill */
			timerVegasFill.start();
	
			ti = 0.;
			tsi = 0.;

			/* zero device_d */
//			kernel_set_zero<<<1,1>>>(device_d, xdim*nd);

			/* call fill kernel */
			int nBlockTot_Fill = (nCubes-1)/nBlockSize+1;
			int nGridSizeY_Fill = (nBlockTot_Fill-1)/nGridSizeMax+1;
			int nGridSizeX_Fill = (nBlockTot_Fill-1)/nGridSizeY+1;
			dim3 BkGd_Fill(nGridSizeX_Fill, nGridSizeY_Fill);
/*
			coutMaster << "shared memory size: " << nBlockSize*xdim*nd << std::endl;
			coutMaster << "nBlockTot_Fill  : " << nBlockTot_Fill << std::endl;
			coutMaster << "nGridSizeX_Fill : " << nGridSizeX_Fill << std::endl;
			coutMaster << "nGridSizeY_Fill : " << nGridSizeY_Fill << std::endl;
*/			
			gVegasFill<<<BkGd_Fill, ThBk, nBlockSize*xdim*nd*sizeof(double) + 2*nBlockSize*sizeof(double) >>>(device_d, device_ti, device_tsi, nCubes, device_Fval, device_IAval, npg, nd, mds, xdim);
//			gVegasFill<<<BkGd_Fill, ThBk >>>(device_d, nCubes, device_Fval, device_IAval, npg, nd, mds, xdim);
			gpuErrchk( cudaDeviceSynchronize() );

			/* transfer data from device to host */
			gpuErrchk(cudaMemcpy(host_d, device_d,  sized, cudaMemcpyDeviceToHost));
			gpuErrchk(cudaMemcpy(&ti, device_ti,  sizeof(double), cudaMemcpyDeviceToHost));
			gpuErrchk(cudaMemcpy(&tsi, device_tsi,  sizeof(double), cudaMemcpyDeviceToHost));
			gpuErrchk( cudaDeviceSynchronize() );
			
			coutMaster << "ti = " << ti << std::endl;
			coutMaster << "tsi = " << tsi << std::endl;
			
			
			if(mds>0){
				for (int idim=0;idim<xdim;idim++) {
					int idimCube = idim*nCubeNpg;
					for(int idx=0;idx<nCubeNpg;idx++) {
						double f = host_Fval[idx];
						int iaj = host_IAval[idimCube+idx];
						host_d[idim*nd + iaj] += f*f;
					}
				}
			}

			
			coutMaster << "host_d: " << print_array(host_d,xdim*nd) << std::endl;
	
			timerVegasFill.stop();
	
			tsi *= dv2g;
			double ti2 = ti*ti;
			double wgt = ti2/tsi;
			si += ti*wgt;
			si2 += ti2;
			swgt += wgt;
			schi += ti2*wgt;
			avgi[id_integral] = si/swgt;
			sd[id_integral] = swgt*it/si2;
			chi2a[id_integral] = 0.;
			if(it>1) chi2a[id_integral] = sd[id_integral]*(schi/swgt-avgi[id_integral]*avgi[id_integral])/((double)it-1.);
			sd[id_integral] = sqrt(1./sd[id_integral]);

			/* check Nan */
			if(avgi[id_integral] != avgi[id_integral]){
				coutMaster << "dv2g=" << dv2g << std::endl;
				coutMaster << "ti=" << ti << std::endl;
				coutMaster << "ti2=" << ti2 << std::endl;
				coutMaster << "wgt=" << wgt << std::endl;
				coutMaster << "si=" << si << std::endl;
				coutMaster << "swgt=" << swgt << std::endl;
				coutMaster << "it=" << it << std::endl;
				coutMaster << "chi2a=" << chi2a << std::endl;
				
				avgi[id_integral] = 0.0;
			}

			if(debug_print_integration_inner) {
							
				tsi = sqrt(tsi);
				coutMaster << std::endl;
				coutMaster << " << integration by vegas >>" << std::endl;
				coutMaster << "     iteration no. " << std::setw(4) << it
							<< std::setw(10) << std::setprecision(6)
							<< "   integral=  " << ti << std::endl;
				coutMaster << "                          std dev  = " << tsi << std::endl;
				coutMaster << "     accumulated results: integral = " << avgi[id_integral] << std::endl;
				coutMaster << "                          std dev  = " << sd[id_integral] << std::endl;
				if(it > 1){
					coutMaster << "                          chi**2 per it'n = "
								<< std::setw(10) << std::setprecision(4) << chi2a[id_integral] << std::endl;
				}
				if(nprn<0){
					for (int j=0;j<xdim;j++) {
						coutMaster << "   == data for axis "
									<< std::setw(2) << j << " --" << std::endl;
						coutMaster << "    x    delt i   convce";
						coutMaster << "    x    delt i   convce";
						coutMaster << "    x    delt i   convce"<<std::endl;
					}
				}
			}
	
			/* refine grid */
			timerVegasRefine.start();
	
			double r[nd_max];
			double dt[xdim_max];
			for(int j=0;j<xdim;j++) {
				double xo = host_d[j*nd + 0];
				double xn = host_d[j*nd + 1];
				host_d[j*nd + 0] = 0.5*(xo+xn);
				dt[j] = host_d[j*nd + 0];
				for (int i=1;i<nd-1;i++) {
					host_d[j*nd + i] = xo+xn;
					xo = xn;
					xn = host_d[j*nd + (i+1)];
					host_d[j*nd + i] = (host_d[j*nd + i]+xn)/3.;
					dt[j] += host_d[j*nd + i];
				}
				host_d[j*nd + (nd-1)] = 0.5*(xn+xo);
				dt[j] += host_d[j*nd + (nd-1)];
			}
	      
			for(int j=0;j<xdim;j++) {
				double rc = 0.;
				for(int i=0;i<nd;i++) {
					r[i] = 0.;
					if(host_d[j*nd + i]>0.) {
						double xo = dt[j]/host_d[j*nd + i];
						if(!isinf(xo)){
							r[i] = pow(((xo-1.)/xo/log(xo)),alph);
						}
					}
					rc += r[i];
				}
				rc /= xnd;
				int k = -1;
				double xn = 0.;
				double dr = xn;
				int i = k;
				k++;
				dr += r[k];
				double xo = xn;
				xn = xi[j][k];
	
				do{
					while (dr<=rc) {
						k++;
						dr += r[k];
						xo = xn;
						xn = xi[j][k];
					}
					i++;
					dr -= rc;
					xin[i] = xn-(xn-xo)*dr/r[k];
				} while (i<nd-2);
	
				for (int i=0;i<nd-1;i++) {
					xi[j][i] = (double)xin[i];
				}
				
				xi[j][nd-1] = 1.;
			}
	
			gpuErrchk(cudaMemcpyToSymbol(g_xi, xi, sizeof(xi)));
			cudaThreadSynchronize();
	
			timerVegasRefine.stop();
	   } while (it<itmx && acc*fabs(avgi[id_integral])<sd[id_integral]);
	
		gpuErrchk(cudaFreeHost(host_Fval));
		gpuErrchk(cudaFree(device_Fval));
	
		gpuErrchk(cudaFreeHost(host_IAval));
		gpuErrchk(cudaFree(device_IAval));


	} /* for id_integral=0:number_of_integrals-1 */

	LOG_FUNC_END
}

__global__
void gVegasCallFunc(int g_ng, int g_npg, int g_nd, double g_xjac, double device_dxg, int g_nCubes, double *device_xl, double *device_dx, double* device_Fval, int* device_IAval, int xdim, int number_of_integrals, int n, double *device_lambda, int *device_matrix_D_arr, int id_integral){
	
	/* --------------------
	 * Check the thread ID
	 * -------------------- */
	 
	const unsigned int tIdx  = threadIdx.x;
	const unsigned int bDimx = blockDim.x;

	const unsigned int bIdx  = blockIdx.x;
	const unsigned int gDimx = gridDim.x;
	const unsigned int bIdy  = blockIdx.y;

	unsigned int bid  = bIdy*gDimx+bIdx;
	const unsigned int tid = bid*bDimx+tIdx;

	int ig = tid/g_npg;

	unsigned nCubeNpg = g_nCubes*g_npg;

	if(tid<nCubeNpg) {
		unsigned ia[xdim_max];
      
		unsigned int tidRndm = tid;
      
		int kg[xdim_max];
      
		unsigned igg = ig;
		for(int j=0;j<xdim;j++) {
			kg[j] = igg%g_ng+1;
			igg /= g_ng;
		}
      
		double randm[xdim_max];
		fxorshift128(tidRndm, xdim, randm);
      
		double x[xdim_max];
      
		double wgt = g_xjac;
		for(int j=0;j<xdim;j++){
			double xo,xn,rc;
			xn = (kg[j]-randm[j])*device_dxg+1.;
			ia[j] = (int)xn-1;
			if (ia[j]<=0) {
				xo = g_xi[j][ia[j]];
				rc = (xn-(double)(ia[j]+1))*xo;
			} else {
				xo = g_xi[j][ia[j]]-g_xi[j][ia[j]-1];
				rc = g_xi[j][ia[j]-1]+(xn-(double)(ia[j]+1))*xo;
			}
			x[j] = device_xl[j]+rc*device_dx[j];
			wgt *= xo*(double)g_nd;
		}
      
		/* compute function value for this x */
		double fs;
		//func_entropy(double *device_values_out, double *xx, int xdim, int number_of_integrals, int number_of_moments, double *device_lambda, int *device_matrix_D_arr)
		func_entropy(&fs,x, xdim, number_of_integrals, n, device_lambda, device_matrix_D_arr, id_integral);
		fs = wgt*fs;

		device_Fval[tid] = fs;
		for(int idim=0;idim<xdim;idim++) {
			device_IAval[idim*nCubeNpg+tid] = ia[idim];
		}
	}
}

__device__ __host__ __forceinline__
void fxorshift128(unsigned int seed, int n, double* a){
	unsigned int x,y,z,w,t;

	seed=seed*2357+123456789U;
    
	//1812433253 = 0x6C078965h;
	x=seed=1812433253U*(seed^(seed>>30))+1;
	y=seed=1812433253U*(seed^(seed>>30))+2;
	z=seed=1812433253U*(seed^(seed>>30))+3;
	w=seed=1812433253U*(seed^(seed>>30))+4;

	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;
	t=x^x<<11;x=y;y=z;z=w;w^=w>>19^t^t>>8;

	for(int i=0;i<n;++i){
		t=x^x<<11;
		x=y;
		y=z;
		z=w;
		w^=(w>>19)^(t^(t>>8));
		a[i]=w*Norum;
	}
	return;
}


__global__ void gVegasFill( double *device_d, double *device_ti, double *device_tsi, int nCubes, double *device_Fval, int *device_IAval, int npg, int nd, int mds, int xdim) {
	
	/* compute thread id */
	const unsigned int tIdx  = threadIdx.x;
	const unsigned int bDimx = blockDim.x;

	const unsigned int bIdx  = blockIdx.x;
	const unsigned int gDimx = gridDim.x;
	const unsigned int bIdy  = blockIdx.y;

	unsigned int bid  = bIdy*gDimx+bIdx; /* block id */
	const unsigned int tid = bid*bDimx+tIdx; /* thread id */

	int ig = tid; /* index of cube */
	
	/* shared memory for reduction */
	extern __shared__ double shared_d[];
	extern __shared__ double shared_ti[];
	extern __shared__ double shared_tsi[];

	/* zero my part of shared memory */
	for (int i=0;i<xdim*nd;i++) {
		shared_d[(tIdx*xdim*nd) + i] = 0.;
	}
	shared_ti[tIdx] = 0.0;
	shared_tsi[tIdx] = 0.0;

	if(ig < nCubes){ /* maybe we call more threads then nCubes */
		unsigned nCubeNpg = nCubes*npg;

		double ti=0.;
		double tsi=0.;

		double fb = 0.;
		double f2b = 0.;
		for(int ipg=0;ipg<npg;ipg++) {
			int idx = npg*ig+ipg;
			double f = device_Fval[idx];
			double f2 = f*f;
			fb += f;
			f2b += f2;
		}
		f2b = sqrt(f2b*npg);
		f2b = (f2b-fb)*(f2b+fb);
		ti += fb;
		tsi += f2b;

		if(mds<0){
			int idx = npg*ig;
			for(int idim=0;idim<xdim;idim++) {
				int iaj = device_IAval[idim*nCubeNpg+idx];
				shared_d[(tIdx * xdim*nd) + idim*nd + iaj] += f2b;
			}
		}
		
		shared_ti[tIdx] = ti;
		shared_tsi[tIdx] = tsi;
	}

	/* do reduction in shared mem */
	for (int s=1; s < blockDim.x; s *=2){
		int index = 2 * s * threadIdx.x;;

		if (index < blockDim.x){
			for (int i=0;i<xdim*nd;i++) {
				shared_d[(index * xdim*nd) + i] += shared_d[((index + s) * xdim*nd) + i];
			}

			shared_ti[index] += shared_ti[index + s];
			shared_tsi[index] += shared_tsi[index + s];
		}
		__syncthreads();
	}

    /* write result for this block to global mem */
    if (tIdx == 0){
		for (int i=0;i<xdim*nd;i++) {
			atomicAdd(&(device_d[i]), shared_d[i]);
		}
		atomicAdd(device_ti, shared_ti[0]);
		atomicAdd(device_tsi, shared_tsi[0]);

	}

}


__device__
void func_entropy(double *device_values_out, double *xx, int xdim, int number_of_integrals, int n, double *device_lambda, int *device_matrix_D_arr, int id_integral){
    double V = 0.0;
    double p = 0.0;
    
    for (int i = 0; i < n; i++){
        p = 1.0;
        for (int j = 0; j < xdim; j++){
            p = p*pow(xx[j], device_matrix_D_arr[(i+1)*xdim+j]);
		}

        V = V - p*device_lambda[i];
    }

	/* ff2[0] - type = 0 */
    double FF= exp(V);

	/* function value */
	if(id_integral == 0){
		device_values_out[0] = FF;		
	}

	/* gradient */
	if(id_integral >= 1 && id_integral< 1+n){
		for(int order = 0; order < n; order++){
			if(1+order == id_integral){
				p = 1.0;
				for (int j = 0; j < xdim; j++){
					p = p*pow(xx[j], device_matrix_D_arr[(order+1)*xdim+j]);
				}
				device_values_out[0] = p*FF;
			}
		}
	}
	
	/* hessian */
	if(id_integral >= 1+n && id_integral < number_of_integrals){
		int counter = 1+n;
		for(int order = 0; order < n; order++){
			for(int order2 = order; order2 < n; order2++){
				if(counter == id_integral){
					p = 1.0;
					for(int j=0; j<xdim;j++){
						p = p*pow(xx[j], device_matrix_D_arr[(order+1)*xdim+j] + device_matrix_D_arr[(order2+1)*xdim+j]);
					}
					device_values_out[0] = p*FF;
				}
				counter++;
			}
		}
	}
		
}




}
}

