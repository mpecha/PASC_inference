#ifndef GENERALVECTOR_H
#define	GENERALVECTOR_H

namespace pascinference {

	/* deal with all */
	class General_all_type {
		typedef minlin::detail::all_type *minlin_all_type;

		public:
			/* convert to PetscVector all */
			operator petscvector::petscvector_all_type() const {  // TODO: if USE_PETSCVECTOR
				return petscvector::all;
			}

			operator minlin_all_type() const {  // TODO: if USE_MINLIN
				return minlin::all;
			}


	} gall; /* sorry for gall, but minlin is using global all and I don't know how to retype it. */ // TODO: deal with all


	/* general vector class - take original class and add multiplication with GeneralMatrix */
	template<class VectorBase>
	class GeneralVector : public VectorBase {

		public:
			/* constructors */
			GeneralVector(): VectorBase() {}
			template<class ArgType> GeneralVector(ArgType arg): VectorBase(arg) {}
			
			/* matrix-vector multiplication with General matrix */
			GeneralVector<VectorBase> &operator=(GeneralMatrixRHS<VectorBase> rhs){
				rhs.matmult(*this);
				return *this;
			}

	};

}

#endif