#ifndef DRAY_ARRAY_UTILS_HPP
#define DRAY_ARRAY_UTILS_HPP

#include <dray/array.hpp>
#include <dray/exports.hpp>
#include <dray/policies.hpp>
#include <dray/types.hpp>

#include <cstring>

namespace dray
{

template<typename T>
static void array_memset_zero(Array<T> &array)
{
  const size_t size = array.size();
#ifdef DRAY_CUDA_ENABLED 
  T * ptr = array.get_device_ptr();
  cudaMemset(ptr, 0, sizeof(T) * size);
#else
  T * ptr = array.get_host_ptr();
  std::memset(ptr, 0, sizeof(T) * size);
#endif

}

template<typename T, int32 S>
static void array_memset_vec(Array<Vec<T,S>> &array, const Vec<T,S> &val)
{
  
  const int32 size = array.size();

  Vec<T,S> *array_ptr = array.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    array_ptr[i] = val;
  });
}

template<typename T>
static void array_memset(Array<T> &array, const T val)
{
  
  const int32 size = array.size();

  T *array_ptr = array.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    array_ptr[i] = val;
  });
}

template<typename T>
static void array_copy(Array<T> &dest, Array<T> &src)
{
 
  assert(dest.size() == src.size());

  const int32 size = dest.size();

  T *dest_ptr = dest.get_device_ptr();
  T *src_ptr = src.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    dest_ptr[i] = src_ptr[i];
  });
}

static
Array<int32> array_counting(const int32 &size, 
                            const int32 &start,
                            const int32 &step)
{
  
  Array<int32> iterator;
  iterator.resize(size);
  int32 *ptr = iterator.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    ptr[i] = start + i * step;
  });

  return iterator;
}

static
Array<int32> array_random(const int32 &size,
                          const uint64 &seed,
                          const int32 &modulus)
{
  // I wanted to use both 'seed' and 'sequence number' (see CUDA curand).
  // The caller provides seed, which is shared by all array elements;
  // but different array elements have different sequence numbers.
  // The sequence numbers should advance by (size) on successive calls.

  // For the serial case, I will instead use a "call number" and change the seed,
  // by feeding (given seed + call number) into the random number generator.
  // Unfortunately, two arrays each of size N will get different entries
  // than one array of size 2N.

  static uint64 call_number = 1;    // Not 0: Avoid calling srand() with 0 and then 1.
  //static uint64 sequence_start = 0;    //future: for parallel random

  // Allocate the array.
  Array<int32> rand_array;
  rand_array.resize(size);
  //int32 *ptr = rand_array.get_device_ptr();
  int32 *host_ptr = rand_array.get_host_ptr();

  // Initialize serial random number generator, then fill array.
  srand(seed + call_number);
  for (int32 i = 0; i < size; i++)
    host_ptr[i] = rand() % modulus;
  

  // TODO parallel random number generation
//  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
//  {
//    curandState_t state;
//    curand_init(seed, sequence_start + i, 0, &state);
//    ptr[i] = curand(&state);
//  });

  call_number++;
  //sequence_start += size;

  return rand_array;
}


// Inputs: Array of something convertible to bool.
//
// Outputs: Array of destination indices. ([out])
//          The size number of things that eval'd to true.
template<typename T>
static
Array<int32> array_compact_indices(const Array<T> src, int32 &out_size)
{
  const int32 in_size = src.size();

  Array<int32> dest_indices;
  dest_indices.resize(in_size);

  // Convert the source array to 0s and 1s.
  { // (Limit the scope of one-time-use array pointers.)
    const int32 *src_ptr = src.get_device_ptr_const();
    int32 *dest_indices_ptr = dest_indices.get_device_ptr();
    RAJA::forall<for_policy>(RAJA::RangeSegment(0, in_size), [=] DRAY_LAMBDA (int32 ii)
    {
      dest_indices_ptr[ii] = (int32) (bool) src_ptr[ii];
    });
  }

  // Use an exclusive prefix sum to compute the destination indices.
  {
    int32 *dest_indices_ptr = dest_indices.get_device_ptr();
    RAJA::exclusive_scan_inplace<for_policy>(
        dest_indices_ptr,
        dest_indices_ptr + in_size,
        RAJA::operators::plus<int32>{});
  }

  // Retrieve the size of the output array.
  out_size = *(dest_indices.get_host_ptr_const() + in_size - 1) +
      ((*src.get_host_ptr_const()) ? 1 : 0);

  return dest_indices;
}


#ifdef DRAY_CUDA_ENABLED 
inline __device__
Vec<float32,4> const_get_vec4f(const Vec<float32,4> *const data)
{
  const float4 temp = __ldg((const float4*) data);;
  Vec<float32,4> res;
  res[0] = temp.x;
  res[1] = temp.y;
  res[2] = temp.z;
  res[3] = temp.w;
  return res;
}
#else
inline
Vec<float32,4> const_get_vec4f(const Vec<float32,4> *const data)
{
  return data[0];
}
#endif

} // namespace dray
#endif
