#include "algebra/graph/bgmgraph.h"

namespace pascinference {
namespace algebra {

double BGMGraph::compute_normsqr(const double *values, int idx1, int idx2){
	int d;
	double mynorm = 0;
	for(d=0;d<dim;d++){
		mynorm += (values[idx1+d*n] - values[idx2+d*n])*(values[idx1+d*n] - values[idx2+d*n]);
	}
	return mynorm;
}

BGMGraph::BGMGraph(std::string filename, int dim){
	coordinates = new GeneralVector<PetscVector>();
	
	/* load nodes from file */
	coordinates->load_local(filename);

	this->dim = dim;
	n = coordinates->size()/(double)dim;

	m = 0;
	m_max = 0;
	threshold = -1;
	processed = false;

	DD_decomposed = false;
}

BGMGraph::BGMGraph(const double *coordinates_array, int n, int dim){
	/* prepare vector from values */
	Vec vec_arr;
	TRYCXX( VecCreateSeqWithArray(PETSC_COMM_SELF,1,n*dim,coordinates_array,&vec_arr) );

	coordinates = new GeneralVector<PetscVector>(vec_arr);

	this->dim = dim;
	this->n = n;

	m = 0;
	m_max = 0;
	threshold = -1;
	processed = false;

	DD_decomposed = false;
}

BGMGraph::BGMGraph(){
	this->n = 0;
	this->m = 0;
	this->m_max = 0;
	threshold = -1;
	processed = false;
	DD_decomposed = false;
}

BGMGraph::~BGMGraph(){
//	TRYCXX( VecDestroy(&coordinates.get_vector()));
//	free(coordinates);

	/* if the graph was processed, then free memory */
	if(processed){
		free(neighbor_nmbs);
		int i;
		for(i=0;i<n;i++){
			free(neighbor_ids[i]);
		}
		free(neighbor_ids);

		#ifdef USE_CUDA
			gpuErrchk( cudaFree(neighbor_nmbs_gpu) );
			for(i=0;i<n;i++){
				gpuErrchk( cudaFree(neighbor_ids_cpugpu[i]) );
			}
			free(neighbor_ids_cpugpu);
			gpuErrchk( cudaFree(neighbor_ids_gpu) );
		#endif

	}
	
	if(DD_decomposed){
		free(DD_affiliation);
		free(DD_permutation);
		free(DD_invpermutation);
		free(DD_lengths);
		free(DD_ranges);
	}
}

std::string BGMGraph::get_name() const {
	return "BGMGraph";
}

int BGMGraph::get_n() const {
	return this->n;
}

int BGMGraph::get_m() const {
	return this->m;
}

int BGMGraph::get_m_max() const {
	return this->m_max;
}

int BGMGraph::get_dim() const {
	return this->dim;
}

double BGMGraph::get_threshold() const {
	return this->threshold;
}

int *BGMGraph::get_neighbor_nmbs() const {
	return this->neighbor_nmbs;
}

int **BGMGraph::get_neighbor_ids() const {
	return this->neighbor_ids;
}

int *BGMGraph::get_neighbor_nmbs_gpu() const {
	#ifdef USE_CUDA
		return this->neighbor_nmbs_gpu;
	#else
		return this->neighbor_nmbs;
	#endif
}

int **BGMGraph::get_neighbor_ids_gpu() const {
	#ifdef USE_CUDA
		return this->neighbor_ids_gpu;
	#else
		return this->neighbor_ids;
	#endif
}

GeneralVector<PetscVector> *BGMGraph::get_coordinates() const {
	return this->coordinates;
}

bool BGMGraph::get_DD_decomposed() const {
	return this->DD_decomposed;
}

int BGMGraph::get_DD_size() const {
	return this->DD_size;
}

int *BGMGraph::get_DD_affiliation() const {
	return this->DD_affiliation;
}

int *BGMGraph::get_DD_permutation() const {
	return this->DD_permutation;
}

int *BGMGraph::get_DD_invpermutation() const {
	return this->DD_invpermutation;
}

int *BGMGraph::get_DD_lengths() const {
	return this->DD_lengths;
}

int *BGMGraph::get_DD_ranges() const {
	return this->DD_ranges;
}

void BGMGraph::decompose(int nmb_domains){
	LOG_FUNC_BEGIN
	
	if(!this->DD_decomposed){ /* there wasn't decompose called yet */
		this->DD_decomposed = true;
		this->DD_size = nmb_domains;

		/* allocate arrays */
		DD_affiliation = (int*)malloc(n*sizeof(int));
		DD_permutation = (int*)malloc(n*sizeof(int));
		DD_invpermutation = (int*)malloc(n*sizeof(int));
		DD_lengths = (int*)malloc(DD_size*sizeof(int));
		DD_ranges = (int*)malloc((DD_size+1)*sizeof(int));

		if(nmb_domains > 1){
			/* ---- METIS STUFF ---- */
			int *xadj; /* Indexes of starting points in adjacent array */
			xadj = (int*)malloc((n+1)*sizeof(int));
		
			int *adjncy; /* Adjacent vertices in consecutive index order */
			adjncy = (int*)malloc(2*m*sizeof(int));

			/* fill aux metis stuff */
			int counter = 0;
			for(int i=0;i<n;i++){
				xadj[i] = counter;
				for(int j=0;j<neighbor_nmbs[i];j++){
					adjncy[counter] = neighbor_ids[i][j];
					counter++;
				}
			}	
			xadj[n] = counter;

			int objval;
			int nWeights = 1; /* something with weights of graph, I really don't know, sorry */

			/* run decomposition */
			int metis_ret = METIS_PartGraphKway(&n,&nWeights, xadj, adjncy,
							   NULL, NULL, NULL, &DD_size, NULL,
							   NULL, NULL, &objval, DD_affiliation);

			/* free aux stuff */
			free(xadj);
			free(adjncy);
			/* --------------------- */

			/* compute local lengths and permutation of global indexes */
			for(int i=0;i<DD_size;i++){ /* use DD_length as counters */
				DD_lengths[i] = 0;
			}
			for(int i=0;i<n;i++){
				DD_permutation[i] = DD_lengths[DD_affiliation[i]]; /* set index as a value of counter */
				DD_lengths[DD_affiliation[i]] += 1;
			}

			/* compute ranges */
			DD_ranges[0] = 0;
			for(int i=0;i<DD_size;i++){
				DD_ranges[i+1] = DD_ranges[i] + DD_lengths[i];
			}
			for(int i=0;i<n;i++){
				DD_permutation[i] += DD_ranges[DD_affiliation[i]]; /* shift local indexes to global */

				/* compute inverse permutation */
				DD_invpermutation[DD_permutation[i]] = i;
			}

			
		} else {
			
			/* nmb_domains <= 1 */
			/* fill arrays manually, METIS is not able to do it with number of domains equal to 1 */
			for(int i=0;i<n;i++){
				DD_affiliation[i] = 0;
			}

			for(int i=0;i<n;i++){
				DD_permutation[i] = i;
				DD_invpermutation[i] = i;
			}

			DD_lengths[0] = n;

			DD_ranges[0] = 0;
			DD_ranges[1] = n;
		}
	
	} else {
		// TODO: give error that decompose was already called, or clean stuff and make it again?
	}
	
	LOG_FUNC_END
}

void BGMGraph::process(double threshold) {
	LOG_FUNC_BEGIN
	
	this->threshold = threshold;
	
	/* prepare array for number of neighbors */
	neighbor_nmbs = (int*)malloc(n*sizeof(int));

	#pragma omp parallel for
	for(int i=0;i<n;i++){
		neighbor_nmbs[i] = 0;
	}
	
	/* get local array and work with it */
	const double *coordinates_arr;
	TRYCXX( VecGetArrayRead(coordinates->get_vector(),&coordinates_arr) );
	
	/* go throught graph - compute number of neighbors */
//	#pragma omp parallel for
	for(int i=0;i<n;i++){
		for(int j=i+1;j<n;j++){
			if(compute_normsqr(coordinates_arr, i, j) < threshold*threshold){
				neighbor_nmbs[i] += 1;
				neighbor_nmbs[j] += 1;
			}
		}
	}

	/* prepare storages for neightbors ids */
	neighbor_ids = (int**)malloc(n*sizeof(int*));

	#pragma omp parallel for
	for(int i=0;i<n;i++){
		neighbor_ids[i] = (int*)malloc(neighbor_nmbs[i]*sizeof(int));
	}

	/* go throught graph - fill indexes of neighbors */
	int *counters;
	counters = (int*)malloc(n*sizeof(int));

	#pragma omp parallel for
	for(int i=0;i<n;i++){
		counters[i] = 0;
	}

//	#pragma omp parallel for // TODO: here is a problem, cannot be used, maybe because of couter arrays?
	for(int i=0;i<n;i++){
		for(int j=i+1;j<n;j++){
			if(compute_normsqr(coordinates_arr, i, j) < threshold*threshold){
				neighbor_ids[i][counters[i]] = j;
				neighbor_ids[j][counters[j]] = i;

				counters[i] += 1;
				counters[j] += 1;
				
				this->m += 1;
			}
		}

		/* compute m_max (max degree of vertex) */
		if(neighbor_nmbs[i] > m_max){
			this->m_max = neighbor_nmbs[i];
		}
	}
	free(counters);
	
	/* restore array */
	TRYCXX( VecRestoreArrayRead(coordinates->get_vector(),&coordinates_arr) );

	#ifdef USE_CUDA
		/* copy data to gpu */
		gpuErrchk( cudaMalloc((void **)&neighbor_nmbs_gpu, n*sizeof(int)) );
		gpuErrchk( cudaMemcpy( neighbor_nmbs_gpu, neighbor_nmbs, n*sizeof(int), cudaMemcpyHostToDevice) );
		
		/* allocate pointers on CPU */
		neighbor_ids_cpugpu = (int**)malloc(n*sizeof(int*));
		
		for(int i=0;i<n;i++){
			int mysize = neighbor_nmbs[i];
		
			gpuErrchk( cudaMalloc((void **)&(neighbor_ids_cpugpu[i]), mysize*sizeof(int)) );
			gpuErrchk( cudaMemcpy( neighbor_ids_cpugpu[i], neighbor_ids[i], mysize*sizeof(int), cudaMemcpyHostToDevice) );
		}

		/* copy pointers to arrays from CPU to GPU */
		gpuErrchk( cudaMalloc((void **)&neighbor_ids_gpu, n*sizeof(int*)) );
		gpuErrchk( cudaMemcpy( neighbor_ids_gpu, neighbor_ids_cpugpu, n*sizeof(int*), cudaMemcpyHostToDevice) );

		gpuErrchk( cudaDeviceSynchronize() );
	#endif
	
	processed = true;

	LOG_FUNC_END
}

void BGMGraph::print(ConsoleOutput &output) const {
	output << this->get_name() << std::endl;
	
	output.push();
	output << " - dim:        " << this->dim << std::endl;
	output << " - vertices:   " << this->n << std::endl;
	output << " - edges:      " << this->m << std::endl;
	output << " - max_degree: " << this->m_max << std::endl;
	output << " - threshold:  " << this->threshold << std::endl;
	output << " - processed:  " << this->processed << std::endl;

	output << " - decomposed: " << this->DD_decomposed << std::endl;
	output.push();
	if(DD_decomposed){
		output << " - nmb of domains: " << this->DD_size << std::endl;
		output << " - lengths:        ";
		for(int i=0;i<DD_size;i++){ /* use DD_length as counters */
			output << DD_lengths[i];
			if(i < DD_size-1){
				output << ", ";
			}
		}
		output << std::endl;
	}
	output.pop();
	
	output.pop();
	
}

void BGMGraph::print_content(ConsoleOutput &output) const {
	LOG_FUNC_BEGIN
	
	output << "Graph" << std::endl;
	
	output.push();
	output << " - dim         : " << this->dim << std::endl;
	output << " - vertices    : " << this->n << std::endl;
	output << " - edges       : " << this->m << std::endl;
	output << " - max_degree  : " << this->m_max << std::endl;
	output << " - threshold   : " << this->threshold << std::endl;
	output << " - coordinates : " << *coordinates << std::endl;

	output << " - decomposed  : " << this->DD_decomposed << std::endl;
	output.push();
	if(DD_decomposed){
		output << " - DD_size: " << this->DD_size << std::endl;
		output << " - DD_lengths:        ";
		for(int i=0;i<DD_size;i++){
			output << DD_lengths[i];
			if(i < DD_size-1){
				output << ", ";
			}
		}
		output << std::endl;	
		output << " - DD_ranges:        ";
		for(int i=0;i<DD_size+1;i++){
			output << DD_ranges[i];
			if(i < DD_size){
				output << ", ";
			}
		}
		output << std::endl;	
		output << " - DD_affiliation: ";
		for(int i=0;i<n;i++){
			output << DD_affiliation[i];
			if(i < n-1){
				output << ", ";
			}
		}
		output << std::endl;	
		output << " - DD_permutation: ";
		for(int i=0;i<n;i++){
			output << DD_permutation[i];
			if(i < n-1){
				output << ", ";
			}
		}
		output << std::endl;	
		output << " - DD_invpermutation: ";
		for(int i=0;i<n;i++){
			output << DD_invpermutation[i];
			if(i < n-1){
				output << ", ";
			}
		}
		output << std::endl;	
			
	}
	output.pop();

	output << " - processed:  " << this->processed << std::endl;
	if(this->processed){
		output << " - arrays of neighbors: " << std::endl;
		output.push();
		for(int i=0;i<n;i++){
			output << i << ": " << "(" << neighbor_nmbs[i] << "): ";
			for(int j=0;j<neighbor_nmbs[i];j++){
				output << neighbor_ids[i][j];
				if(j < neighbor_nmbs[i]-1){
					output << ", ";
				}
			}
			output << std::endl;
		}
		output.pop();
	}
	output.pop();
	
	LOG_FUNC_END
}

void BGMGraph::saveVTK(std::string filename) const {
	LOG_FUNC_BEGIN
	
	Timer timer_saveVTK; 
	timer_saveVTK.restart();
	timer_saveVTK.start();
	
	/* to manipulate with file */
	std::ofstream myfile;	
	
	/* master writes the file */
	if(GlobalManager.get_rank() == 0){
		myfile.open(filename.c_str());

		/* write header to file */
		myfile << "# vtk DataFile Version 3.1" << std::endl;
		myfile << "PASCInference: Graph" << std::endl;
		myfile << "ASCII" << std::endl;
		myfile << "DATASET UNSTRUCTURED_GRID" << std::endl;

		/* write points - coordinates */
		myfile << "POINTS " << n << " FLOAT" << std::endl;
		const double *coordinates_arr;
		TRYCXX( VecGetArrayRead(coordinates->get_vector(),&coordinates_arr) );
		for(int i=0;i<n;i++){
			if(dim == 1){ 
				/* 1D sample */
				myfile << coordinates_arr[i] << " 0 0" << std::endl; /* x */
			}

			if(dim == 2){ 
				/* 2D sample */
				myfile << coordinates_arr[i] << " "; /* x */
				myfile << coordinates_arr[n+i] << " 0" << std::endl; /* y */
			}

			if(dim == 3){ 
				/* 3D sample */
				myfile << coordinates_arr[i] << " "; /* x */
				myfile << coordinates_arr[n+i] << " "; /* y */
				myfile << coordinates_arr[2*n+i] << std::endl; /* z */
			}

			if(dim > 3){
				//TODO ???
			}
		}
		TRYCXX( VecRestoreArrayRead(coordinates->get_vector(),&coordinates_arr) );
		
		/* write edges */
		myfile << "\nCELLS " << 2*m << " " << 2*m*3 << std::endl; /* actually, edges are here twice */
		for(int i=0;i<n;i++){
			for(int j=0;j<neighbor_nmbs[i];j++){
				myfile << "2 " << i << " " << neighbor_ids[i][j] << std::endl;
			}
		}
		myfile << "\nCELL_TYPES " << 2*m << std::endl;
		for(int i=0;i<2*m;i++){
			myfile << "3" << std::endl;
		}
		
		/* write domain affiliation */
		myfile << "\nPOINT_DATA " << n << std::endl;
		myfile << "SCALARS domain float 1" << std::endl;
		myfile << "LOOKUP_TABLE default" << std::endl;
		for(int i=0;i<n;i++){
			if(DD_decomposed){
				myfile << DD_affiliation[i] << std::endl;
			} else {
				myfile << "-1" << std::endl;
			}
		}
		
		myfile.close();
	}
	TRYCXX( PetscBarrier(NULL) );
	
	
	
	timer_saveVTK.stop();
	coutMaster <<  " - graph saved to VTK in: " << timer_saveVTK.get_value_sum() << std::endl;

	LOG_FUNC_END
}



}
} /* end of namespace */

