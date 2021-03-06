/** @file pascinference.h
 *  @brief main header file
 *
 *  Include this header file to work with PASC_INFERENCE library.
 *
 *  @author Lukas Pospisil
 */

#ifndef PASCINFERENCE_H
#define	PASCINFERENCE_H

/* include common c++ header files */
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stack>
#include <limits>

/* include general templated functions */
#include "general/general.h"

/* include seqarrayvector */
#include "external/seqarrayvector/seqarrayvector.h"

/* include MINLIN */ 
#ifdef USE_MINLIN
 #include "external/minlin/minlin.h"
#endif

/* include Petsc */
#ifdef USE_PETSC
 #include "external/petscvector/petscvector.h"
#endif

/* PERMON */
#ifdef USE_PERMON
	#include "fllopqp.h"
#endif

/* boost stuff */
#ifdef USE_BOOST
 #include "external/boost/boost.h"
#endif

/** 
*  \namespace pascinference
*  \brief main namespace of the library
*
*/
namespace pascinference {
	/** 
	*  \namespace pascinference::common
	*  \brief general commonly-used stuff
	*
	*/
	namespace common {}

	/** 
	*  \namespace pascinference::algebra
	*  \brief for manipulation with algebraic structures
	*
	*/
	namespace algebra {}

	/** 
	*  \namespace pascinference::data
	*  \brief for manipulation with problem data
	*
	*/
	namespace data {}

	/** 
	*  \namespace pascinference::solver
	*  \brief solvers for different types of problems
	*
	*/
	namespace solver {}

	/** 
	*  \namespace pascinference::model
	*  \brief models for solving time-series problems
	*
	*/
	namespace model {}

	// TODO: is this good idea?
	using namespace common;
	using namespace algebra;
	using namespace data;
	using namespace solver;
	using namespace model;
	
}



#endif


