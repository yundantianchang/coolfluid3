// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Common_mpi_all_gather_hpp
#define CF_Common_mpi_all_gather_hpp

////////////////////////////////////////////////////////////////////////////////

#include "Common/Foreach.hpp"
#include "Common/BasicExceptions.hpp"
#include "Common/CodeLocation.hpp"
#include "Common/MPI/types.hpp"
#include "Common/MPI/datatype.hpp"

// #include "Common/MPI/debug.hpp" // for debugging mpi

////////////////////////////////////////////////////////////////////////////////

/**
  @file all_gather.hpp
  @author Tamas Banyai
  all_gather collective communication interface to MPI standard.
  Due to the nature of MPI standard, at the lowest level the memory required to be linear meaning &xyz[0] should give a single and continous block of memory.
  Some functions support automatic evaluation of number of items on the receive side but be very cautious with using them because it requires two collective communication and may end up with degraded performance.
  Currently, the interface supports raw pointers and std::vectors.
  Three types of communications is implemented:
  - Constant size send and receive on all processors via MPI_all_gather
  - Variable size send and receive via MPI_all_gatherv
  - Extension of the variable sized communication to support mapped storage both on send and receive side.
**/

////////////////////////////////////////////////////////////////////////////////

namespace CF {
  namespace Common {
    namespace mpi {

////////////////////////////////////////////////////////////////////////////////

namespace detail {

////////////////////////////////////////////////////////////////////////////////

  /**
    Implementation to the all_gather interface with constant size communication.
    Don't call this function directly, use mpi::all_gather instead.
    In_values and out_values must be linear in memory and their sizes should be #processes*n.
    @param comm mpi::Communicator
    @param in_values pointer to the send buffer
    @param in_n size of the send array (number of items)
    @param out_values pointer to the receive buffer
    @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
  **/
  template<typename T>
  inline void
  all_gatherc_impl(const Communicator& comm, const T* in_values, const int in_n, T* out_values, const  int stride )
  {
    // get data type and number of processors
    Datatype type = mpi::get_mpi_datatype(*in_values);
    int nproc;
    MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

    // if stride is greater than one
    cf_assert( stride>0 );

    // set up out_buf
    T* out_buf=out_values;
    if (in_values==out_values) {
      if ( (out_buf=new T[nproc*in_n*stride+1]) == (T*)0 ) throw CF::Common::NotEnoughMemory(FromHere(),"Could not allocate temporary buffer."); // +1 for avoiding possible zero allocation
    }

    // do the communication
    /// @bug (in OpenMPI ) MPI_CHECK_RESULT(MPI_Allgather, (const_cast<T*>(in_values), in_n*stride, type, out_buf, in_n*stride, type, comm));
    /// @todo check if in later versions it is fixed
    int *displs=new int[nproc];
    for (int i=0; i<nproc; i++) displs[i]=i*in_n*stride;
    int *counts=new int[nproc];
    for (int i=0; i<nproc; i++) counts[i]=in_n*stride;
    MPI_CHECK_RESULT(MPI_Allgatherv, (const_cast<T*>(in_values), in_n*stride, type, out_buf, counts, displs, type, comm ));
    delete[] displs;
    delete[] counts;

    // deal with out_buf
    if (in_values==out_values) {
      memcpy(out_values,out_buf,nproc*in_n*stride*sizeof(T));
      delete[] out_buf;
    }
  }

////////////////////////////////////////////////////////////////////////////////

  /**
    Implementation to the all_gather interface with variable size communication through in and out map.
    Don't call this function directly, use mpi::all_gathervm instead.
    In_values and out_values must be linear in memory and their sizes should be sum(in_n[i]) and sum(out_n[i]) i=0..#processes-1.
    @param comm mpi::Communicator
    @param in_values pointer to the send buffer
    @param in_n array holding send counts of size #processes
    @param in_map array of size #processes holding the mapping. If zero pointer passed, no mapping on send side.
    @param out_values pointer to the receive buffer
    @param out_n array holding receive counts of size #processes. If zero pointer passed, no mapping on receive side.
    @param out_map array of size #processes holding the mapping
    @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
  **/
  template<typename T>
  inline void
  all_gathervm_impl(const Communicator& comm, const T* in_values, const int in_n, const int *in_map, T* out_values, const int *out_n, const int *out_map, const int stride )
  {
    // get data type and number of processors
    Datatype type = mpi::get_mpi_datatype(*in_values);
    int nproc;
    MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

    // if stride is smaller than one and unsupported functionality
    cf_assert( stride>0 );

    // compute displacements both on send an receive side
    // also compute stride-multiplied send and receive counts
    int *out_nstride=new int[nproc];
    int *out_disp=new int[nproc];
    out_disp[0]=0;
    for(int i=0; i<nproc-1; i++) {
      out_nstride[i]=stride*out_n[i];
      out_disp[i+1]=out_disp[i]+out_nstride[i];
    }
    out_nstride[nproc-1]=out_n[nproc-1]*stride;

    // compute total number of send and receive items
    const int in_sum=stride*in_n;
    const int out_sum=out_disp[nproc-1]+stride*out_n[nproc-1];

    // set up in_buf
    T *in_buf=(T*)in_values;
    if (in_map!=0) {
      if ( (in_buf=new T[in_sum+1]) == (T*)0 ) throw CF::Common::NotEnoughMemory(FromHere(),"Could not allocate temporary buffer."); // +1 for avoiding possible zero allocation
      if (stride==1) { for(int i=0; i<in_sum; i++) in_buf[i]=in_values[in_map[i]]; }
      else { for(int i=0; i<in_sum/stride; i++) memcpy(&in_buf[stride*i],&in_values[stride*in_map[i]],stride*sizeof(T)); }
    }

    // set up out_buf
    T *out_buf=out_values;
    if ((out_map!=0)||(in_values==out_values)) {
      if ( (out_buf=new T[out_sum+1]) == (T*)0 ) throw CF::Common::NotEnoughMemory(FromHere(),"Could not allocate temporary buffer."); // +1 for avoiding possible zero allocation
    }

    // do the communication
    MPI_CHECK_RESULT(MPI_Allgatherv, (in_buf, in_sum, type, out_buf, out_nstride, out_disp, type, comm));

    // re-populate out_values
    if (out_map!=0) {
      if (stride==1) { for(int i=0; i<out_sum; i++) out_values[out_map[i]]=out_buf[i]; }
      else { for(int i=0; i<out_sum/stride; i++) memcpy(&out_values[stride*out_map[i]],&out_buf[stride*i],stride*sizeof(T)); }
      delete[] out_buf;
    } else if (in_values==out_values) {
      memcpy(out_values,out_buf,out_sum*sizeof(T));
      delete[] out_buf;
    }

    // free internal memory
    if (in_map!=0) delete[] in_buf;
    delete[] out_disp;
    delete[] out_nstride;
  }

////////////////////////////////////////////////////////////////////////////////

} // end namespace detail

////////////////////////////////////////////////////////////////////////////////

/**
  Interface to the constant size all_gather communication with specialization to raw pointer.
  If null pointer passed for out_values then memory is allocated and the pointer to it is returned, otherwise out_values is returned.
  @param comm mpi::Communicator
  @param in_values pointer to the send buffer
  @param in_n size of the send array (number of items)
  @param out_values pointer to the receive buffer
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline T*
all_gather(const Communicator& comm, const T* in_values, const int in_n, T* out_values, const int stride=1)
{
  // get nproc, irank
  int nproc;
  MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

  // allocate out_buf if incoming pointer is null
  T* out_buf=out_values;
  if (out_values==0) {
    const int size=stride*nproc*in_n>1?stride*nproc*in_n:1;
    if ( (out_buf=new T[size]) == (T*)0 ) throw CF::Common::NotEnoughMemory(FromHere(),"Could not allocate temporary buffer.");
  }

  // call c_impl
  detail::all_gatherc_impl(comm, in_values, in_n, out_buf, stride);
  return out_buf;
}

////////////////////////////////////////////////////////////////////////////////

/**
  Interface to the constant size all_gather communication with specialization to std::vector.
  @param comm mpi::Communicator
  @param in_values send buffer
  @param out_values receive buffer
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline void
all_gather(const Communicator& comm, const std::vector<T>& in_values, std::vector<T>& out_values, const int stride=1)
{
  // get nproc, irank
  int nproc;
  MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

  // set out_values's sizes
  cf_assert( in_values.size() % (stride) == 0 );
  int in_n=(int)in_values.size();
  out_values.resize(in_n*nproc);
  out_values.reserve(in_n*nproc);

  // call c_impl
  detail::all_gatherc_impl(comm, (T*)(&in_values[0]), in_n/(stride), (T*)(&out_values[0]), stride);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Interface to the constant size all_gather communication with specialization to std::vector.
  @param comm mpi::Communicator
  @param in_values send buffer
  @param out_values receive buffer
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline void
all_gather(const Communicator& comm, const T& in_value, std::vector<T>& out_values)
{
  // get nproc, irank
  int nproc;
  MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

  // set out_values's sizes
  out_values.resize(nproc);
  out_values.reserve(nproc);

  // call c_impl
  detail::all_gatherc_impl(comm, (T*)(&in_value), 1, (T*)(&out_values[0]), 1);
}

////////////////////////////////////////////////////////////////////////////////

//needs a forward
template<typename T>
inline T*
all_gather(const Communicator& comm, const T* in_values, const int in_n, const int *in_map, T* out_values, int *out_n, const int *out_map, const int stride=1);

/**
  Interface to the variable size all_gather communication with specialization to raw pointer.
  If null pointer passed for out_values then memory is allocated and the pointer to it is returned, otherwise out_values is returned.
  If out_n (receive counts) contains only -1, then a pre communication occurs to fill out_n.
  @param comm mpi::Communicator
  @param in_values pointer to the send buffer
  @param in_n array holding send counts of size #processes
  @param out_values pointer to the receive buffer
  @param out_n array holding receive counts of size #processes
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline T*
all_gather(const Communicator& comm, const T* in_values, const int in_n, T* out_values, int *out_n, const int stride=1)
{
  // call mapped variable all_gather
  return all_gather(comm,in_values,in_n,0,out_values,out_n,0,stride);
}

////////////////////////////////////////////////////////////////////////////////

//needs a forward
template<typename T>
inline void
all_gather(const Communicator& comm, const std::vector<T>& in_values, const int in_n, const std::vector<int>& in_map, std::vector<T>& out_values, std::vector<int>& out_n, const std::vector<int>& out_map, const int stride=1);

/**
  Interface to the constant size all_gather communication with specialization to std::vector.
  If out_values's size is zero then its resized.
  If out_n (receive counts) is not of size of #processes, then error occurs.
  If out_n (receive counts) is filled with -1s, then a pre communication occurs to fill out_n.
  @param comm mpi::Communicator
  @param in_values send buffer
  @param in_n send counts of size #processes
  @param out_values receive buffer
  @param out_n receive counts of size #processes
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline void
all_gather(const Communicator& comm, const std::vector<T>& in_values, const int in_n, std::vector<T>& out_values, std::vector<int>& out_n, const int stride=1)
{
  // call mapped variable all_gather
  std::vector<int> in_map(0);
  std::vector<int> out_map(0);
  all_gather(comm,in_values,in_n,in_map,out_values,out_n,out_map,stride);
}

////////////////////////////////////////////////////////////////////////////////

/**
  Interface to the variable size mapped all_gather communication with specialization to raw pointer.
  If null pointer passed for out_values then memory is allocated to fit the max in map and the pointer is returned, otherwise out_values is returned.
  If out_n (receive counts) contains only -1, then a pre communication occurs to fill out_n.
  However due to the fact that map already needs all the information if you use all_gather to allocate out_values and fill out_n then you most probably doing something wrong.
  @param comm mpi::Communicator
  @param in_values pointer to the send buffer
  @param in_n array holding send counts of size #processes
  @param in_map array of size #processes holding the mapping. If zero pointer passed, no mapping on send side.
  @param out_values pointer to the receive buffer
  @param out_n array holding receive counts of size #processes. If zero pointer passed, no mapping on receive side.
  @param out_map array of size #processes holding the mapping
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline T*
all_gather(const Communicator& comm, const T* in_values, const int in_n, const int *in_map, T* out_values, int *out_n, const int *out_map, const int stride)
{
  // get nproc, irank
  int nproc;
  MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));

  // if out_n consist of -1s then communicate for number of receives
  int out_sum=0;
  for (int i=0; i<nproc; i++) out_sum+=out_n[i];
  if (out_sum==-nproc) {
    if (out_map!=0) throw CF::Common::ParallelError(FromHere(),"Trying to perform communication with receive map while receive counts are unknown, this is bad usage of parallel environment.");
    detail::all_gatherc_impl(comm,&in_n,1,out_n,1);
    out_sum=0;
    for (int i=0; i<nproc; i++) out_sum+=out_n[i];
  }

  // allocate out_buf if incoming pointer is null
  T* out_buf=out_values;
  if (out_values==0) {
    if (out_map!=0){
      int out_sum_tmp=0;
      for (int i=0; i<out_sum; i++) out_sum_tmp=out_map[i]>out_sum_tmp?out_map[i]:out_sum_tmp;
      out_sum=out_sum_tmp+1;
    }
    if ( (out_buf=new T[stride*out_sum]) == (T*)0 ) throw CF::Common::NotEnoughMemory(FromHere(),"Could not allocate temporary buffer.");
  }

  // call vm_impl
  detail::all_gathervm_impl(comm, in_values, in_n, in_map, out_buf, out_n, out_map, stride);
  return out_buf;
}

////////////////////////////////////////////////////////////////////////////////

/**
  Interface to the constant size all_gather communication with specialization to raw pointer.
  If out_values's size is zero then its resized.
  If out_n (receive counts) is not of size of #processes, then error occurs.
  If out_n (receive counts) is filled with -1s, then a pre communication occurs to fill out_n.
  However due to the fact that map already needs all the information if you use all_gather to allocate out_values and fill out_n then you most probably doing something wrong.
  @param comm mpi::Communicator
  @param in_values send buffer
  @param in_n send counts of size #processes
  @param in_map array of size #processes holding the mapping. If zero pointer or zero size vector passed, no mapping on send side.
  @param out_values receive buffer
  @param out_n receive counts of size #processes
  @param out_map array of size #processes holding the mapping. If zero pointer or zero size vector passed, no mapping on receive side.
  @param stride is the number of items of type T forming one array element, for example if communicating coordinates together, then stride==3:  X0,Y0,Z0,X1,Y1,Z1,...,Xn-1,Yn-1,Zn-1
**/
template<typename T>
inline void
all_gather(const Communicator& comm, const std::vector<T>& in_values, const int in_n, const std::vector<int>& in_map, std::vector<T>& out_values, std::vector<int>& out_n, const std::vector<int>& out_map, const int stride)
{
  // number of processes and checking in_n and out_n (out_n deliberately throws exception because the vector can arrive from arbitrary previous usage)
  int nproc;
  MPI_CHECK_RESULT(MPI_Comm_size,(comm,&nproc));
  if ((int)out_n.size()!=nproc) CF::Common::BadValue(FromHere(),"Size of vector for number of items to be received does not match to number of processes.");

  // compute number of send and receive
  int out_sum=0;
  boost_foreach( int i, out_n ) out_sum+=i;

  // if necessary, do communication for out_n
  if (out_sum == -nproc){
    if (out_map.size()!=0) throw CF::Common::ParallelError(FromHere(),"Trying to perform communication with receive map while receive counts are unknown, this is bad usage of parallel environment.");
    detail::all_gatherc_impl(comm,&in_n,1,&out_n[0],1);
    out_sum=0;
    boost_foreach( int & i, out_n ) out_sum+=i;
  }

  // resize out_values if vector size is zero
  if (out_values.size() == 0 ){
    if (out_map.size()!=0) {
      out_sum=0;
      boost_foreach( int i, out_map ) out_sum=i>out_sum?i:out_sum;
      if (out_sum!=0) out_sum++;
    }
    out_values.resize(stride*out_sum);
    out_values.reserve(stride*out_sum);
  }

  // call vm_impl
  detail::all_gathervm_impl(comm, (T*)(&in_values[0]), in_n, &in_map[0], (T*)(&out_values[0]), &out_n[0], &out_map[0], stride);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace mpi
} // namespace Common
} // namespace CF

////////////////////////////////////////////////////////////////////////////////

#endif // CF_Common_mpi_all_gather_hpp
