#include "external/petscvector/common/decomposition.h"

namespace pascinference {
namespace algebra {

template<>
Decomposition<PetscVector>::Decomposition(int T, int R, int K, int xdim, int DDT_size){
	LOG_FUNC_BEGIN

	this->T = T;
	this->R = R;
	this->K = K;
	this->xdim = xdim;
		
	this->DDT_size = DDT_size;
	this->DDR_size = 1;
	
	/* prepare new layout for T */
	destroy_DDT_arrays = true;	
	DDT_ranges = (int *)malloc((this->DDT_size+1)*sizeof(int));
	
	Vec DDT_layout;
	TRYCXX( VecCreate(PETSC_COMM_WORLD,&DDT_layout) );
	#ifdef USE_CUDA
		TRYCXX(VecSetType(DDT_layout, VECMPICUDA));
	#endif
	
	TRYCXX( VecSetSizes(DDT_layout, PETSC_DECIDE, T ));
	TRYCXX( VecSetFromOptions(DDT_layout) );
	
	/* get properties of this layout */
	const int *DDT_ranges_const;
	TRYCXX( VecGetOwnershipRanges(DDT_layout, &DDT_ranges_const) );
	for(int i=0;i<DDT_size+1;i++){
		DDT_ranges[i] = DDT_ranges_const[i]; // TODO: how to deal with const int in ranges form PETSc?
	}
	
	/* destroy temp vector */
	TRYCXX( VecDestroy(&DDT_layout) );

	/* prepare new layout for R */
	/* no graph provided - allocate arrays */
	destroy_DDR_arrays = true;

	DDR_affiliation = (int *)malloc(R*sizeof(int));
	DDR_permutation = (int *)malloc(R*sizeof(int));
	DDR_invpermutation = (int *)malloc(R*sizeof(int));
	for(int i=0;i<R;i++){
		DDR_affiliation[i] = 0;
		DDR_permutation[i] = i;
		DDR_invpermutation[i] = i;
	}

	DDR_lengths = (int *)malloc((this->DDR_size)*sizeof(int));
	DDR_lengths[0] = R;

	DDR_ranges = (int *)malloc((this->DDR_size+1)*sizeof(int));
	DDR_ranges[0] = 0;
	DDR_ranges[1] = R;

	/* no graph provided */
	graph = NULL;

	compute_rank();

	LOG_FUNC_END
}

template<>
Decomposition<PetscVector>::Decomposition(int T, BGMGraph<PetscVector> &new_graph, int K, int xdim, int DDR_size){
	LOG_FUNC_BEGIN

	this->T = T;
	this->R = new_graph.get_n();
	this->K = K;
	this->xdim = xdim;

	/* prepare new layout for R */
	destroy_DDR_arrays = false;
	set_graph(new_graph, DDR_size);

	this->DDT_size = 1;
	this->DDR_size = new_graph.get_DD_size();

	/* prepare new layout for T */
	destroy_DDT_arrays = true;	
	DDT_ranges = (int *)malloc((this->DDT_size+1)*sizeof(int));
	DDT_ranges[0] = 0;
	DDT_ranges[1] = T;

	compute_rank();

	LOG_FUNC_END
}

template<>
Decomposition<PetscVector>::Decomposition(int T, BGMGraph<PetscVector> &new_graph, int K, int xdim, int DDT_size, int DDR_size){
	LOG_FUNC_BEGIN

	this->T = T;
	this->R = new_graph.get_n();
	this->K = K;
	this->xdim = xdim;

	/* prepare new layout for R */
	destroy_DDR_arrays = false;
	set_graph(new_graph, DDR_size);

	this->DDT_size = DDT_size;
	this->DDR_size = graph->get_DD_size();
	
	/* prepare new layout for T */
	destroy_DDT_arrays = true;	
	/* unfortunatelly, we have to compute distribution of T manually */
	int DDT_optimal_local_size = T/(double)DDT_size;
	int DDT_optimal_local_size_residue = T - DDT_optimal_local_size*DDT_size;
	DDT_ranges = (int *)malloc((this->DDT_size+1)*sizeof(int));
	DDT_ranges[0] = 0;
	for(int i=0;i<DDT_size;i++){
		DDT_ranges[i+1] = DDT_ranges[i] + DDT_optimal_local_size;
		if(i < DDT_optimal_local_size_residue){
			DDT_ranges[i+1] += 1;
		}
	}

	compute_rank();

	LOG_FUNC_END
}

template<>
Decomposition<PetscVector>::~Decomposition(){
	LOG_FUNC_BEGIN

	if(destroy_DDT_arrays){
		free(DDT_ranges);
	}

	if(destroy_DDR_arrays){
		free(DDR_affiliation);
		free(DDR_permutation);
		free(DDR_invpermutation);
		free(DDR_lengths);
		free(DDR_ranges);
	}

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::compute_rank(){
	LOG_FUNC_BEGIN

	/* get rank of this processor */
	int rank = GlobalManager.get_rank();

	this->DDT_rank = rank/(double)this->DDR_size;
	this->DDR_rank = rank - (this->DDT_rank)*(this->DDR_size);

	/* control the decomposition */
//	if(this->DDT_size*this->DDR_size != GlobalManager.get_size()){
//		coutMaster << "Sorry, DDT_size*DDR_size != nproc" << std::endl;
//		coutMaster << " DDT_size = " << this->DDT_size << std::endl;
//		coutMaster << " DDR_size = " << this->DDR_size << std::endl;
//		coutMaster << " nproc    = " << GlobalManager.get_size() << std::endl;

		// TODO: throw error
//	}
	
	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::set_graph(BGMGraph<PetscVector> &new_graph, int DDR_size) {

	if(destroy_DDR_arrays){
		free(DDR_affiliation);
		free(DDR_permutation);
		free(DDR_invpermutation);
		free(DDR_lengths);
		free(DDR_ranges);
	}

	/* decompose graph */
	this->DDR_size = DDR_size;
	new_graph.decompose(DDR_size);

	this->graph = &new_graph;
	destroy_DDR_arrays = false;
	DDR_affiliation = new_graph.get_DD_affiliation();
	DDR_permutation = new_graph.get_DD_permutation();
	DDR_invpermutation = new_graph.get_DD_invpermutation();
	DDR_lengths = new_graph.get_DD_lengths();
	DDR_ranges = new_graph.get_DD_ranges();	
	
	compute_rank();
}

template<>
void Decomposition<PetscVector>::createGlobalVec_gamma(Vec *x_Vec) const {
	LOG_FUNC_BEGIN

	int T = this->get_T();
	int R = this->get_R();
	int K = this->get_K();
	int Tlocal = this->get_Tlocal();
	int Rlocal = this->get_Rlocal();

	TRYCXX( VecCreate(PETSC_COMM_WORLD,x_Vec) );

	#ifdef USE_CUDA
		TRYCXX(VecSetType(*x_Vec, VECMPICUDA));
	#else
		TRYCXX(VecSetType(*x_Vec, VECMPI));
	#endif

	TRYCXX( VecSetSizes(*x_Vec,Tlocal*Rlocal*K,T*R*K) );
	TRYCXX( VecSetFromOptions(*x_Vec) );

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::createGlobalVec_data(Vec *x_Vec) const {
	LOG_FUNC_BEGIN

	int T = this->get_T();
	int R = this->get_R();
	int xdim = this->get_xdim();
	int Tlocal = this->get_Tlocal();
	int Rlocal = this->get_Rlocal();

	TRYCXX( VecCreate(PETSC_COMM_WORLD,x_Vec) );
	
	#ifdef USE_CUDA
		TRYCXX(VecSetType(*x_Vec,VECMPICUDA));
	#else
		TRYCXX(VecSetType(*x_Vec,VECMPI));
	#endif

	TRYCXX( VecSetSizes(*x_Vec,Tlocal*Rlocal*xdim,T*R*xdim) );
	TRYCXX( VecSetFromOptions(*x_Vec) );

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::permute_TRxdim(Vec orig_Vec, Vec new_Vec, bool invert) const {
	LOG_FUNC_BEGIN

	permute_TRblocksize(orig_Vec, new_Vec, xdim, invert);

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::permute_TRK(Vec orig_Vec, Vec new_Vec, bool invert) const {
	LOG_FUNC_BEGIN
	
	permute_TRblocksize(orig_Vec, new_Vec, K, invert);

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::permute_TRblocksize(Vec orig_Vec, Vec new_Vec, int blocksize, bool invert) const {
	LOG_FUNC_BEGIN

	int Tbegin = get_Tbegin();
	int Tend = get_Tend();
	int Tlocal = get_Tlocal();
	int Rbegin = get_Rbegin();
	int Rend = get_Rend();
	int Rlocal = get_Rlocal();
	
	int local_size = Tlocal*Rlocal*blocksize;

	IS orig_local_is;
	IS new_local_is;

	Vec orig_local_Vec;
	Vec new_local_Vec;

	/* prepare index set with local data */
//	int orig_local_arr[local_size];
	int *orig_local_arr;
	orig_local_arr = new int [local_size];
	int j = 0;

	/* I assume the format of original data as blockTR */
	/* fill orig_local_arr */
	for(int t=Tbegin;t<Tend;t++){
		for(int i=0;i<R;i++){
			if(DDR_affiliation[i] == DDR_rank){
				for(int k=0;k<blocksize;k++){
					orig_local_arr[j*blocksize+k] = t*R*blocksize + i*blocksize + k;
				}
				j++;
			}
		}
	}

//	coutAll << "original: " << print_array(orig_local_arr,local_size) << std::endl;
//	coutAll.synchronize();

	/* I assume the new format as TRblock */
	/* fill new_local_arr */
	int *new_local_arr;
	new_local_arr = new int [local_size];
	int m = 0;
	for(int k=0;k<blocksize;k++){
		for(int i=Rbegin;i<Rend;i++){
			for(int t=0;t<Tend-Tbegin;t++){
				new_local_arr[m] = Tbegin*R*blocksize + Rbegin*blocksize + t*R*blocksize + i*blocksize + k;
				m++;
			}
		}
	}

//	coutAll << "new: " << print_array(new_local_arr,local_size) << std::endl;
//	coutAll.synchronize();


//	for(int i=0; i<local_size;i++){
//		coutMaster << i << ". = " << new_local_arr[i] << std::endl;
//	}

	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_COPY_VALUES,&orig_local_is) );
//	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_OWN_POINTER,&orig_local_is) );

	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, new_local_arr, PETSC_COPY_VALUES,&new_local_is) );
//	TRYCXX( ISCreateStride(PETSC_COMM_WORLD, local_size, Tbegin*R*blocksize + Rbegin*blocksize, 1, &new_local_is) );

	/* get subvector with local values from original data */
	TRYCXX( VecGetSubVector(new_Vec, new_local_is, &new_local_Vec) );
	TRYCXX( VecGetSubVector(orig_Vec, orig_local_is, &orig_local_Vec) );

	/* copy values */
	if(!invert){
		TRYCXX( VecCopy(orig_local_Vec, new_local_Vec) );
	} else {
		TRYCXX( VecCopy(new_local_Vec, orig_local_Vec) );
	}

	/* restore subvector with local values from original data */
	TRYCXX( VecRestoreSubVector(new_Vec, new_local_is, &new_local_Vec) );
	TRYCXX( VecRestoreSubVector(orig_Vec, orig_local_is, &orig_local_Vec) );

	/* destroy used stuff */
	TRYCXX( ISDestroy(&orig_local_is) );
	TRYCXX( ISDestroy(&new_local_is) );

	TRYCXX( PetscBarrier(NULL));

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::createIS_gammaK(IS *is, int k) const {
	LOG_FUNC_BEGIN
	
	TRYCXX( ISCreateStride(PETSC_COMM_WORLD, get_Tlocal()*get_Rlocal(), get_Tbegin()*get_R()*get_K() + get_Rbegin()*get_K() + k, get_K(), is) );

	LOG_FUNC_END
}


}
} /* end of namespace */

