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
	DDT_ranges = new int[this->DDT_size+1];

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
	/* no graph provided, create one */
	double *coordinates_array = new double[R];
	for(int r=0;r<R;r++){
		coordinates_array[r] = r;
	}
	graph = new BGMGraph<PetscVector>(coordinates_array, R, 1);
	graph->process(0.0);

	destroy_DDR_graph = true; /* I created graph, I should destroy it */
	set_graph(*graph, DDR_size);

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
	destroy_DDR_graph = false;
	set_graph(new_graph, DDR_size);

	this->DDT_size = 1;
	this->DDR_size = new_graph.get_DD_size();

	/* prepare new layout for T */
	destroy_DDT_arrays = true;
	DDT_ranges = new int[this->DDT_size+1];
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
	destroy_DDR_graph = false;
	set_graph(new_graph, DDR_size);

	this->DDT_size = DDT_size;
	this->DDR_size = graph->get_DD_size();

	/* prepare new layout for T */
	destroy_DDT_arrays = true;
	/* unfortunatelly, we have to compute distribution of T manually */
	int DDT_optimal_local_size = T/(double)DDT_size;
	int DDT_optimal_local_size_residue = T - DDT_optimal_local_size*DDT_size;
	DDT_ranges = new int[this->DDT_size+1];
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

    free(DDTR_ranges);

	if(destroy_DDT_arrays){
		free(DDT_ranges);
	}

	if(destroy_DDR_graph){
		free(graph);
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

    this->DDTR_ranges = new int[get_DDTR_size()+1];
	DDTR_ranges[0] = 0;
    for(int ddt_rank=0; ddt_rank < DDT_size; ddt_rank++){
        for(int ddr_rank=0; ddr_rank < DDR_size; ddr_rank++){
            DDTR_ranges[ddt_rank*DDR_size + ddr_rank + 1] = DDTR_ranges[ddt_rank*DDR_size + ddr_rank] + (DDT_ranges[ddt_rank+1]-DDT_ranges[ddt_rank])*(DDR_ranges[ddr_rank+1]-DDR_ranges[ddr_rank]);
        }
    }

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::set_graph(BGMGraph<PetscVector> &new_graph, int DDR_size) {
	/* decompose graph */
	this->DDR_size = DDR_size;
	new_graph.decompose(DDR_size);

	this->graph = &new_graph;
	destroy_DDR_graph = false;
	DDR_affiliation = new_graph.get_DD_affiliation();
	DDR_permutation = new_graph.get_DD_permutation();
	DDR_invpermutation = new_graph.get_DD_invpermutation();
	DDR_lengths = new_graph.get_DD_lengths();
	DDR_ranges = new_graph.get_DD_ranges();

}

template<>
void Decomposition<PetscVector>::set_new_graph(BGMGraph<PetscVector> &new_graph, int DDR_size) {

	if(destroy_DDR_graph){
		free(graph);
	}

    set_graph(new_graph,DDR_size);
    destroy_DDR_graph = false;

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
void Decomposition<PetscVector>::permute_to_pdTRb(Vec orig_Vec, Vec new_Vec, int blocksize, int type, bool invert) const {
	LOG_FUNC_BEGIN

    /*	TRn */
    if(type==0) permute_gTRb_to_pdTRb(orig_Vec, new_Vec, blocksize, invert);

	/*  TnR */
    if(type==1) permute_gTbR_to_pdTRb(orig_Vec, new_Vec, blocksize, invert);

	/*  nTR */
    if(type==2) permute_gbTR_to_pdTRb(orig_Vec, new_Vec, blocksize, invert);

	LOG_FUNC_END
}


template<>
void Decomposition<PetscVector>::permute_gTRb_to_pdTRb(Vec orig_Vec, Vec new_Vec, int blocksize, bool invert) const {
	LOG_FUNC_BEGIN

	int TRbegin = get_TRbegin();
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
	int *orig_local_arr;
	orig_local_arr = new int [local_size];

 	/* original data is TRb */
	/* transfer it to TRb decomposed */
	for(int t=0;t<Tlocal;t++){
		for(int r=0;r<Rlocal;r++){
            for(int k=0;k<blocksize;k++){
				orig_local_arr[t*Rlocal*blocksize + r*blocksize + k] = (Tbegin+t)*blocksize*R + (Rbegin+r)*blocksize + k;
			}
		}
	}

	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_COPY_VALUES,&orig_local_is) );
//	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_OWN_POINTER,&orig_local_is) );

	createIS_dTR_to_pdTRb(&new_local_is, blocksize);

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
void Decomposition<PetscVector>::permute_gTbR_to_pdTRb(Vec orig_Vec, Vec new_Vec, int blocksize, bool invert) const {
	LOG_FUNC_BEGIN

	int TRbegin = get_TRbegin();
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
	int *orig_local_arr;
	orig_local_arr = new int [local_size];

 	/* original data is TbR */
	/* transfer it to TRb */
	for(int t=0;t<Tlocal;t++){
		for(int r=0;r<Rlocal;r++){
            for(int k=0;k<blocksize;k++){
				orig_local_arr[t*Rlocal*blocksize + r*blocksize + k] = (Tbegin+t)*blocksize*R + k*R + (Rbegin+r);
			}
		}
	}

	coutMaster << print_array(orig_local_arr, local_size) << std::endl;
	TRYCXX( PetscBarrier(NULL));
	
	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_COPY_VALUES, &orig_local_is) );
//	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_OWN_POINTER,&orig_local_is) );

	createIS_dTR_to_pdTRb(&new_local_is, blocksize);

	/* get subvector with local values from original data */
	
	coutMaster << "test1" << std::endl;
	
	TRYCXX( VecGetSubVector(new_Vec, new_local_is, &new_local_Vec) );

	coutMaster << "test2" << std::endl;

	coutMaster << "orig_Vec:" << std::endl;
	TRYCXX( VecView(orig_Vec, PETSC_VIEWER_STDOUT_WORLD) );

	coutMaster << "orig_local_is:" << std::endl;
	TRYCXX( ISView(orig_local_is, PETSC_VIEWER_STDOUT_WORLD) );


	TRYCXX( VecGetSubVector(orig_Vec, orig_local_is, &orig_local_Vec) );

	coutMaster << "test3" << std::endl;

	/* copy values */
	if(!invert){
		TRYCXX( VecCopy(orig_local_Vec, new_local_Vec) );
//		TRYCXX( VecCopy(orig_local_Vec, new_Vec) );
	} else {
		TRYCXX( VecCopy(new_local_Vec, orig_local_Vec) );
//		TRYCXX( VecCopy(new_Vec, orig_local_Vec) );
	}

	coutMaster << "test4" << std::endl;

	/* restore subvector with local values from original data */
	TRYCXX( VecRestoreSubVector(new_Vec, new_local_is, &new_local_Vec) );
	TRYCXX( VecRestoreSubVector(orig_Vec, orig_local_is, &orig_local_Vec) );

	coutMaster << "test5" << std::endl;

	/* destroy used stuff */
	TRYCXX( ISDestroy(&orig_local_is) );
	TRYCXX( ISDestroy(&new_local_is) );

	coutMaster << "test6" << std::endl;

	TRYCXX( PetscBarrier(NULL));

	coutMaster << "test7" << std::endl;


	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::permute_gbTR_to_pdTRb(Vec orig_Vec, Vec new_Vec, int blocksize, bool invert) const {
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
	int *orig_local_arr;
	orig_local_arr = new int [local_size];

	/* original data is TbR */
	/* transfer it to TRb */
	for(int t=0;t<Tlocal;t++){
		for(int r=0;r<Rlocal;r++){
            for(int k=0;k<blocksize;k++){
				orig_local_arr[t*Rlocal*blocksize + r*blocksize + k] = k*R*T + (Tbegin+t)*R + (Rbegin+r);
			}
		}
	}

	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_COPY_VALUES,&orig_local_is) );
//	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, orig_local_arr, PETSC_OWN_POINTER,&orig_local_is) );

	createIS_dTR_to_pdTRb(&new_local_is, blocksize);

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
void Decomposition<PetscVector>::createIS_dTR_to_pdTRb(IS *is, int blocksize) const {
	/* from dTRb to TRb */

	LOG_FUNC_BEGIN

	int TRbegin = get_TRbegin();
	int Tbegin = get_Tbegin();
	int Tend = get_Tend();
	int Tlocal = get_Tlocal();
	int Rbegin = get_Rbegin();
	int Rend = get_Rend();
	int Rlocal = get_Rlocal();

	int local_size = Tlocal*Rlocal*blocksize;

	/* fill orig_local_arr */
	int *local_arr;
	local_arr = new int[local_size];
	int *DD_permutation = get_DDR_permutation();
	int *DD_invpermutation = get_DDR_invpermutation();

	for(int t=0;t<Tlocal;t++){
		for(int r=0;r<Rlocal;r++){
            int r_global = DD_permutation[Rbegin+r]; /* space in global range */
            int t_global = Tbegin + t;                  /* time in global range */

            /* compute index in original decomposed array */
            int idx_pdTR = this->get_pdTR_idx(t_global, r_global);

            for(int k=0;k<blocksize;k++){
                local_arr[t*Rlocal*blocksize + r*blocksize + k] = idx_pdTR*blocksize + k;
            }
		}
	}

	TRYCXX( ISCreateGeneral(PETSC_COMM_WORLD, local_size, local_arr, PETSC_COPY_VALUES,is) );

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::createIS_gammaK(IS *is, int k) const {
	LOG_FUNC_BEGIN

	TRYCXX( ISCreateStride(PETSC_COMM_WORLD, get_Tlocal()*get_Rlocal(), get_TRbegin()*get_K() + k, get_K(), is) );

	LOG_FUNC_END
}

template<>
void Decomposition<PetscVector>::createIS_datan(IS *is, int n) const {
	LOG_FUNC_BEGIN

	TRYCXX( ISCreateStride(PETSC_COMM_WORLD, get_Tlocal()*get_Rlocal(), get_TRbegin()*get_xdim() + n, get_xdim(), is) );

	LOG_FUNC_END
}

template<>
int Decomposition<PetscVector>::get_pdTRb_idx(int t_global, int r_global, int blocksize, int k) const {
    int idx_pdTR = get_pdTR_idx(t_global, r_global);

    return idx_pdTR*blocksize + k;
}

template<> int Decomposition<PetscVector>::get_pdTR_idx(int t_global, int r_global) const {
    /* find ranks of given t and r_g */
    int ddt_rank = get_DDT_rank(t_global);
    int ddr_rank = get_DDR_rank(r_global);
    int ddtr_rank = get_DDTR_rank(ddt_rank, ddr_rank);

    /* compute index in original decomposed array */
    int idx_TR = this->DDTR_ranges[ddtr_rank] + (t_global - this->DDT_ranges[ddt_rank])*(this->DDR_ranges[ddr_rank+1] - this->DDR_ranges[ddr_rank]) + (r_global - this->DDR_ranges[ddr_rank]);

    return idx_TR;
}



}
} /* end of namespace */

