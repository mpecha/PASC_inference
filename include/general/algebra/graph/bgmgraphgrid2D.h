/** @file bgmgraphgrid2D.h
 *  @brief class for manipulation with 2D grid
 *
 *  Defines some basic functions for manipulaton with grids, especially for BLOCKGRAPHMATRIX.
 *
 *  @author Lukas Pospisil
 */
 
#ifndef PASC_COMMON_BGMGRAPHGRID2D_H
#define	PASC_COMMON_BGMGRAPHGRID2D_H

#include "general/algebra/graph/bgmgraph.h"

namespace pascinference {
namespace algebra {

/** \class BGMGraphGrid2D
 *  \brief Graph of two dimensional grid.
 *
 *  Could be used for faster and simplier manipulation with image graph.
 * 
*/
template<class VectorBase>
class BGMGraphGrid2D: public BGMGraph<VectorBase> {
	public:
		class ExternalContent;
	
	protected:
		friend class ExternalContent;
		ExternalContent *externalcontent;			/**< for manipulation with external-specific stuff */

		int width; /**< dimension of grid */
		int height; /**< dimension of grid */
		
		void process_grid_cuda();
	public:
	
		BGMGraphGrid2D(int width, int height);
		BGMGraphGrid2D(std::string filename, int dim=2) : BGMGraph<VectorBase>(filename, dim) {};
		BGMGraphGrid2D(const double *coordinates_array, int n, int dim) : BGMGraph<VectorBase>(coordinates_array, n, dim) {};

		~BGMGraphGrid2D();
		
		virtual std::string get_name() const;
		virtual void process_grid();
		
		int get_width() const;
		int get_height() const;

		void decompose(BGMGraphGrid2D<VectorBase> *finer_grid, int *bounding_box1, int *bounding_box2);

		ExternalContent *get_externalcontent() const;
};


}
} /* end of namespace */



/* -------- IMPLEMENTATION ------- */
namespace pascinference {
namespace algebra {

template<class VectorBase>
BGMGraphGrid2D<VectorBase>::BGMGraphGrid2D(int width, int height) : BGMGraph<VectorBase>(){
	LOG_FUNC_BEGIN

	this->width = width;
	this->height = height;

	this->dim = 2;
	this->n = width*height;
	
	//TODO

	LOG_FUNC_END
}

template<class VectorBase>
BGMGraphGrid2D<VectorBase>::~BGMGraphGrid2D(){
	LOG_FUNC_BEGIN
	
	LOG_FUNC_END
}

template<class VectorBase>
void BGMGraphGrid2D<VectorBase>::process_grid(){
	LOG_FUNC_BEGIN

	this->threshold = 1.1;
	this->m = height*(width-1) + width*(height-1);
	this->m_max = 4;

	/* prepare array for number of neighbors */
	this->neighbor_nmbs = (int*)malloc(this->n*sizeof(int));
	this->neighbor_ids = (int**)malloc(this->n*sizeof(int*));

//	#pragma omp parallel for
	for(int idx=0;idx<width*height;idx++){
		int i = idx/(double)width; /* index of row */
		int j = idx - i*width; /* index of column */	

		/* compute number of neighbors */
		int nmb = 0;
		if(j>0){
			nmb+=1;				
		}
		if(j<width-1){
			nmb+=1;				
		}
		if(i>0){
			nmb+=1;				
		}
		if(i<height-1){
			nmb+=1;				
		}
		this->neighbor_nmbs[idx] = nmb;
		this->neighbor_ids[idx] = (int*)malloc(this->neighbor_nmbs[idx]*sizeof(int));
			
		/* fill neighbors */
		nmb = 0;
		if(j>0){ /* left */
			this->neighbor_ids[idx][nmb] = idx-1;
			nmb+=1;	
		}
		if(j<width-1){ /* right */
			this->neighbor_ids[idx][nmb] = idx+1;
			nmb+=1;	
		}
		if(i>0){ /* down */
			this->neighbor_ids[idx][nmb] = idx-width;
			nmb+=1;	
		}
		if(i<height-1){ /* up */
			this->neighbor_ids[idx][nmb] = idx+width;
			nmb+=1;	
		}
	}

	this->processed = true;

	LOG_FUNC_END
}

template<class VectorBase>
std::string BGMGraphGrid2D<VectorBase>::get_name() const {
	return "BGMGraphGrid2D";
}

template<class VectorBase>
int BGMGraphGrid2D<VectorBase>::get_width() const {
	return this->width;
}

template<class VectorBase>
int BGMGraphGrid2D<VectorBase>::get_height() const {
	return this->height;
}

template<class VectorBase>
void BGMGraphGrid2D<VectorBase>::decompose(BGMGraphGrid2D<VectorBase> *finer_grid, int *bounding_box1, int *bounding_box2) {
	LOG_FUNC_BEGIN
	
	//TODO: if finer_grid is not decomposed, then give error
	
	/* from finer grid decomposition create the decomposition of this graph */
	this->DD_decomposed = true;
	this->DD_size = finer_grid->get_DD_size();

	/* allocate arrays */
	this->DD_affiliation = (int*)malloc(this->n*sizeof(int)); /* domain id */
	this->DD_permutation = (int*)malloc(this->n*sizeof(int)); /* Rorig = P(Rnew) */
	this->DD_invpermutation = (int*)malloc(this->n*sizeof(int)); /* Rnew = Pinv(Rorig) */
	this->DD_lengths = (int*)malloc(this->DD_size*sizeof(int)); /* array of local lengths */
	this->DD_ranges = (int*)malloc((this->DD_size+1)*sizeof(int)); /* ranges in permutation array */

	/* we use DD_lengths as a counters for domains */
	set_value_array(this->DD_size, this->DD_lengths, 0);

	/* pointer to original domain ids */
	int *DD_affiliation1 = finer_grid->get_DD_affiliation(); 

	int id_x1, id_y1; /* coordinates in finer grid */
	int id_x2, id_y2; /* coordinates in coarser grid */

	/* dimensions of the grid */
	int width1 = finer_grid->get_width();
	int height1 = finer_grid->get_height();
	int width2 = width;
	int height2 = height;

	double diff_x = (width1 - 1)/(double)(width2 - 1);
	double diff_y = (height1 - 1)/(double)(height2 - 1);

	double center_x1, left_x1, right_x1;
	double center_y1, left_y1, right_y1;

	/* counter of affiliations in corresponding part in finer grid */
	int DD_affiliation1_sub[this->DD_size];

	int x1_min, x1_max, y1_min, y1_max;
	bounding_box1[0] = std::numeric_limits<int>::max(); /* inf */
	bounding_box1[1] = (-1)*std::numeric_limits<int>::max(); /* -inf */
	bounding_box1[2] = std::numeric_limits<int>::max(); /* inf */
	bounding_box1[3] = (-1)*std::numeric_limits<int>::max(); /* -inf */

	for(int i=0;i<this->n;i++){
		id_y2 = floor(i/((double)width2));
		id_x2 = i - id_y2*width2;
		
		center_x1 = id_x2*diff_x;
		left_x1 = center_x1 - diff_x;
		right_x1 = center_x1 + diff_x;

		center_y1 = id_y2*diff_y;
		left_y1 = center_y1 - diff_y;
		right_y1 = center_y1 + diff_y;

		/* find the affiliation of this point */
		set_value_array(this->DD_size, DD_affiliation1_sub, 0);

		x1_min = floor(left_x1);
		x1_max = floor(right_x1);
		y1_min = floor(left_y1);
		y1_max = floor(right_y1);

		for(id_x1 = x1_min; id_x1 <= x1_max; id_x1++){
			for(id_y1 = y1_min; id_y1 <= y1_max; id_y1++){
				if(id_x1 >= 0 && id_x1 < width1 && id_y1 >= 0 && id_y1 < height1){
					DD_affiliation1_sub[DD_affiliation1[id_y1*width1 + id_x1]] += 1;
				}
			}
		}
		
		/* set the right affiliation of this point */
		this->DD_affiliation[i] = max_arg_array(this->DD_size, DD_affiliation1_sub);
		
		/* increase the counters of nodes in each domain */
		this->DD_lengths[this->DD_affiliation[i]] += 1;
		
		/* set new values of local bounding box */
		if(this->DD_affiliation[i] == GlobalManager.get_rank()){
			if(x1_min < bounding_box1[0] && x1_min >= 0)		bounding_box1[0] = x1_min;
			if(x1_max > bounding_box1[1] && x1_max < width1)	bounding_box1[1] = x1_max;
			if(y1_min < bounding_box1[2] && y1_min >= 0)		bounding_box1[2] = y1_min;
			if(y1_max > bounding_box1[3] && y1_max < height1)	bounding_box1[3] = y1_max;
		}
	}

	/* prepare ranges and zero DD_lengths - it will be used as counters in next step */
	this->DD_ranges[0] = 0;
	for(int i=1;i < this->DD_size+1; i++){
		this->DD_ranges[i] = this->DD_ranges[i-1] + this->DD_lengths[i-1];
		this->DD_lengths[i-1] = 0;
	}

	/* prepare permutation arrays */
	for(int i=0;i<this->n;i++){
		this->DD_invpermutation[i] = this->DD_ranges[this->DD_affiliation[i]] + this->DD_lengths[this->DD_affiliation[i]];
		this->DD_permutation[this->DD_ranges[this->DD_affiliation[i]] + this->DD_lengths[this->DD_affiliation[i]]] = i;

		this->DD_lengths[this->DD_affiliation[i]] += 1;
	}

	/* compute bounding_box2 from bounding_box1 */
	bounding_box2[0] = floor(bounding_box1[0]/diff_x);
	bounding_box2[1] = floor(bounding_box1[1]/diff_x);
	bounding_box2[2] = floor(bounding_box1[2]/diff_y);
	bounding_box2[3] = floor(bounding_box1[3]/diff_y);

	LOG_FUNC_END
}


}
} /* end of namespace */

#endif
