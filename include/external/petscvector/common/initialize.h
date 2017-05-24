#ifndef PASC_PETSCVECTOR_INITIALIZE_H
#define	PASC_PETSCVECTOR_INITIALIZE_H

#include "petsc.h"
#include "petscsys.h"

#include "external/petscvector/algebra/vector/generalvector.h"
#include "general/common/initialize.h"

namespace pascinference {
namespace common {

extern bool PETSC_INITIALIZED;

/* for loading PETSc options */
extern char **argv_petsc;
extern int argc_petsc;

template<> bool Initialize<PetscVector>(int argc, char *argv[]);
template<> void Finalize<PetscVector>();
template<> void allbarrier<PetscVector>();

#ifdef USE_GPU
	extern void cuda_warmup();
	extern void cuda_barrier();
#endif

}
} /* end of namespace */


#endif
