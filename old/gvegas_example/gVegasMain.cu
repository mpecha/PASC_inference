#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h> /* printf in cuda */
#include <cmath>

#include <ctime>
#include <sys/time.h>
#include <sys/resource.h>

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <device_launch_parameters.h>
#include <device_functions.h>

/* cuda error check */ 
#define cutilSafeCall(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true)
{
    if (code != cudaSuccess){
	fprintf(stderr,"\n\x1B[31mCUDA error:\x1B[0m %s %s \x1B[33m%d\x1B[0m\n\n", cudaGetErrorString(code), file, line);
	if (abort) exit(code);
    }
}

void gVegas(double& avgi, double& sd, double& chi2a);

const int ndim_max = 20;
const double alph = 1.5;
double dx[ndim_max]; //
double randm[ndim_max];
const int nd_max = 50; //
double xin[nd_max];
double xjac;
double xl[ndim_max]; //
double xu[ndim_max]; //
double acc;
int ndim, ncall, itmx;
int nprn; //

double xi[ndim_max][nd_max]; //
double si, si2, swgt, schi; //
int ndo; //
int it;
int mds; //
double calls; //
double ti; //
double tsi; //
int npg; //
int ng; //
int nd; //
double dxg; //
double xnd; //
unsigned nCubes; //

int nBlockSize; //
double timeVegasCall; //
double timeVegasMove; //
double timeVegasFill; //
double timeVegasRefine; //


#include "kernels.h"


int DecodeInt(std::string numstr)
{

   if (numstr.size()<5) return 0;
   if (numstr.substr(0,1)!="e") return 0;
   if (numstr.substr(3,1)!="n") return 0;

   std::string expstr = numstr.substr(1,2);
   std::string manstr = numstr.substr(4,1);

   std::istringstream iss;
   iss.clear();
   iss.str(expstr);
   int exp;
   iss>>exp;

   iss.clear();
   iss.str(manstr);
   int man;
   iss>>man;

   return man*(int)(pow(10.,(double)exp)+0.5);
}

int main(int argc, char** argv)
{

   //
   // program interface:
   //   program -ncall="ncall0" -itmx="itmx0" -acc="acc0" -b="nBlockSize0"
   //
   // parameters:
   //   ncall0 = "exxny"
   //   ncall = y*10^xx
   //   itmx  = itmx0
   //   acc   = 0.01*acc0
   //   nBlockSize = nBlockSize0
   //

   //------------------
   //  Initialization
   //------------------

   int GPUdevice = 0;

   ncall = 10000; // number of calls
   itmx = 10;
   acc = 0.0001; // accuracy
   nBlockSize = 256; // CUDA size of the block

   cutilSafeCall(cudaSetDevice(GPUdevice));

   mds = 1;
   ndim = 8;
   
   ng = 0;
   npg = 0;

   for (int i=0;i<ndim;i++) {
      xl[i] = 0.;
      xu[i] = 1.;
   }
   
   nprn = 1;
//   nprn = -1;

   double startTotal, endTotal, timeTotal;
   timeTotal = 0.;
   startTotal = getrusage_usec();

   timeVegasCall = 0.;
   timeVegasMove = 0.;
   timeVegasFill = 0.;
   timeVegasRefine = 0.;

   double avgi = 0.;
   double sd = 0.;
   double chi2a = 0.;

   gVegas(avgi, sd, chi2a);

   endTotal = getrusage_usec();
   timeTotal = endTotal - startTotal;

   //-------------------------
   //  Print out information
   //-------------------------
   std::cout.clear();
   std::cout<<std::setw(10)<<std::setprecision(6)<<std::endl;
   std::cout<<"#============================="<<std::endl;
   std::cout<<"# No. of Thread Block Size  : "<<nBlockSize<<std::endl;
   std::cout<<"#============================="<<std::endl;
   std::cout<<"# No. of dimensions         : "<<ndim<<std::endl;
   std::cout<<"# No. of func calls / iter  : "<<ncall<<std::endl;
   std::cout<<"# No. of max. iterations    : "<<itmx<<std::endl;
   std::cout<<"# Desired accuracy          : "<<acc<<std::endl;
   std::cout<<"#============================="<<std::endl;
   std::cout<<std::scientific;
   std::cout<<std::left<<std::setfill(' ');
   std::cout<<"# Result                    : "
            <<std::setw(12)<<std::setprecision(5)<<avgi<<" +- "
            <<std::setw(12)<<std::setprecision(5)<<sd<<" ( "
            <<std::setw(7)<<std::setprecision(4)
            <<std::fixed<<100.*sd/avgi<<"%)"<<std::endl;
   std::cout<<std::fixed;
   std::cout<<"# Chisquare                 : "<<std::setprecision(4)
            <<chi2a<<std::endl;
   std::cout<<"#============================="<<std::endl;
   std::cout<<std::right;
   std::cout<<"# Total Execution Time(sec) : "
            <<std::setw(10)<<std::setprecision(4)<<timeTotal<<std::endl;
   std::cout<<"#============================="<<std::endl;
   std::cout<<"# Time for func calls (sec) : "
            <<std::setw(10)<<std::setprecision(4)<<timeVegasCall
            <<" ( "<<std::setw(5)<<std::setprecision(2)
            <<100.*timeVegasCall/timeTotal<<"%)"<<std::endl;
   std::cout<<"# Time for data transf (sec): "
            <<std::setw(10)<<std::setprecision(4)<<timeVegasMove
            <<" ( "<<std::setw(5)<<std::setprecision(2)
            <<100.*timeVegasMove/timeTotal<<"%)"<<std::endl;
   std::cout<<"# Time for data fill (sec)  : "
            <<std::setw(10)<<std::setprecision(4)<<timeVegasFill
            <<" ( "<<std::setw(5)<<std::setprecision(2)
            <<100.*timeVegasFill/timeTotal<<"%)"<<std::endl;
   std::cout<<"# Time for grid refine (sec): "
            <<std::setw(10)<<std::setprecision(4)<<timeVegasRefine
            <<" ( "<<std::setw(5)<<std::setprecision(2)
            <<100.*timeVegasRefine/timeTotal<<"%)"<<std::endl;
   std::cout<<"#============================="<<std::endl;

   cudaThreadExit();

   return 0;
}
