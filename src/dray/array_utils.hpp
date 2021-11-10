// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#ifndef DRAY_ARRAY_UTILS_HPP
#define DRAY_ARRAY_UTILS_HPP

#include <dray/array.hpp>
#include <dray/device_array.hpp>
#include <dray/error.hpp>
#include <dray/exports.hpp>
#include <dray/error_check.hpp>
#include <dray/policies.hpp>
#include <dray/types.hpp>
#include <dray/vec.hpp>

#include <cstring>
#include <initializer_list>

namespace dray
{

template <typename T>
Array<T> list2array(const std::initializer_list<T> &list)
{
  return Array<T>(list.begin(), list.size());
}


template <typename T> static void array_memset_zero (Array<T> &array)
{
  const size_t size = array.size () * array.ncomp();
#ifdef DRAY_CUDA_ENABLED
  T *ptr = array.get_device_ptr ();
  cudaMemset (ptr, 0, sizeof (T) * size);
#else
  T *ptr = array.get_host_ptr ();
  std::memset (ptr, 0, sizeof (T) * size);
#endif
}



template <typename T, int32 S>
static inline void array_memset_vec (Array<Vec<T, S>> &array, const Vec<T, S> &val)
{

  const int32 size = array.size () * array.ncomp();

  Vec<T, S> *array_ptr = array.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size),
                            [=] DRAY_LAMBDA (int32 i) { array_ptr[i] = val; });
  DRAY_ERROR_CHECK();
}

template <typename T>
static inline void array_memset (Array<T> &array, const T val)
{

  const int32 size = array.size () * array.ncomp();

  T *array_ptr = array.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size),
                            [=] DRAY_LAMBDA (int32 i) { array_ptr[i] = val; });
  DRAY_ERROR_CHECK();
}


template <typename T>
static Array<T> array_zero(const size_t size, const int32 ncomp = 1)
{
  Array<T> ret;
  ret.resize(size, ncomp);
  array_memset_zero(ret);
  return ret;
}

template <typename T>
static Array<T> array_val(const T val, const size_t size, const int32 ncomp = 1)
{
  Array<T> ret;
  ret.resize(size, ncomp);
  array_memset(ret, val);
  return ret;
}


template <typename T>
static inline T array_min(Array<T> &array, const T identity)
{

  const int32 size = array.size ();

  const T *array_ptr = array.get_device_ptr_const();
  RAJA::ReduceMin<reduce_policy, T> min_value (identity);

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i)
  {
    const T val = array_ptr[i];
    min_value.min(val);
  });
  DRAY_ERROR_CHECK();

  return min_value.get();
}

template <typename T>
static inline T array_max(Array<T> &array, const T identity)
{

  const int32 size = array.size ();

  const T *array_ptr = array.get_device_ptr_const();
  RAJA::ReduceMax<reduce_policy, T> max_value (identity);

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i)
  {
    const T val = array_ptr[i];
    max_value.max(val);
  });
  DRAY_ERROR_CHECK();

  return max_value.get();
}

template <typename T>
static inline T array_sum(Array<T> &array)
{
  const int32 size = array.size();
  const T *array_ptr = array.get_device_ptr_const();
  RAJA::ReduceSum<reduce_policy, T> sum_value(0.0);

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    sum_value += array_ptr[i];
  });
  DRAY_ERROR_CHECK();

  return sum_value.get();
}

template <typename T>
static inline T array_dot(Array<T> &arra, Array<T> &arrb)
{
  const int32 size = arra.size();
  const T *arra_ptr = arra.get_device_ptr_const();
  const T *arrb_ptr = arrb.get_device_ptr_const();
  RAJA::ReduceSum<reduce_policy, T> sum_value(0.0);

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    sum_value += arra_ptr[i] * arrb_ptr[i];
  });
  DRAY_ERROR_CHECK();

  return sum_value.get();
}

template <typename T>
static inline T array_max_diff(const Array<T> input, const T reference)
{
  const int32 size = input.size();
  const T *ptr = input.get_device_ptr_const();
  RAJA::ReduceMax<reduce_policy, T> max_diff;
  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size),
      [=] DRAY_LAMBDA (int32 i)
  {
    const T val = ptr[i];
    const T diff = fabs(val - reference);
    max_diff.max(diff);
  });
  DRAY_ERROR_CHECK();
  return max_diff.get();
}


// Only modify array elements at indices in active_idx.
template <typename T, int32 S>
static inline void array_memset_vec (Array<Vec<T, S>> &array,
                                     const Array<int32> active_idx,
                                     const Vec<T, S> &val)
{
  const int32 asize = active_idx.size ();

  Vec<T, S> *array_ptr = array.get_device_ptr ();
  const int32 *active_idx_ptr = active_idx.get_device_ptr_const ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, asize), [=] DRAY_LAMBDA (int32 aii) {
    const int32 i = active_idx_ptr[aii];
    array_ptr[i] = val;
  });
  DRAY_ERROR_CHECK();
}

// Only modify array elements at indices in active_idx.
template <typename T>
static inline void array_memset (Array<T> &array,
                                 const Array<int32> active_idx,
                                 const T val)
{
  const int32 asize = active_idx.size ();

  T *array_ptr = array.get_device_ptr ();
  const int32 *active_idx_ptr = active_idx.get_device_ptr_const ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, asize), [=] DRAY_LAMBDA (int32 aii) {
    const int32 i = active_idx_ptr[aii];
    array_ptr[i] = val;
  });
  DRAY_ERROR_CHECK();
}


template <typename T>
static inline void array_copy (Array<T> &dest, const Array<T> &src)
{

  const int32 size = src.size ();
  dest.resize(size);

  T *dest_ptr = dest.get_device_ptr ();
  const T *src_ptr = src.get_device_ptr_const ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    dest_ptr[i] = src_ptr[i];
  });
  DRAY_ERROR_CHECK();
}

// this version expect that the destination is already allocated
template <typename T>
static inline void array_copy (Array<T> &dest, const Array<T> &src, const int32 offset)
{

  const int32 size = src.size ();
  const int32 dest_size = dest.size();
  if(size + offset > dest_size)
  {
    DRAY_ERROR("array_copy: destination too small.");
  }

  T *dest_ptr = dest.get_device_ptr ();
  const T *src_ptr = src.get_device_ptr_const ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    dest_ptr[i+offset] = src_ptr[i];
  });
  DRAY_ERROR_CHECK();
}

// may return back the same array unmodified if new_size == src.size()
template <typename T>
static inline Array<T> array_resize_copy(Array<T> &src, int32 new_size)
{
  const int32 old_size = src.size();
  if (new_size == old_size)
    return src;
  else if (new_size < old_size)
  {
    DRAY_ERROR("array_resize_copy: destination too small.");
  }

  Array<T> dest;
  dest.resize(new_size);
  T *dest_ptr = dest.get_device_ptr();
  const T *src_ptr = src.get_device_ptr_const();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, old_size), [=] DRAY_LAMBDA (int32 i) {
    dest_ptr[i] = src_ptr[i];
  });

  return dest;
}

// may return back the same array unmodified if new_size == src.size()
template <typename T>
static inline Array<T> array_resize_copy(Array<T> &src, int32 new_size, T fill_val)
{
  const int32 old_size = src.size();
  if (new_size == old_size)
    return src;
  else if (new_size < old_size)
  {
    DRAY_ERROR("array_resize_copy: destination too small.");
  }

  Array<T> dest;
  dest.resize(new_size);
  T *dest_ptr = dest.get_device_ptr();
  const T *src_ptr = src.get_device_ptr_const();
  const T fillv = fill_val;

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, new_size), [=] DRAY_LAMBDA (int32 i) {
    dest_ptr[i] = (i < old_size ? src_ptr[i] : fill_val);
  });

  return dest;
}



template <typename InType, class UnaryFunctor>
static auto array_map(const Array<InType> input, const UnaryFunctor apply_)
  -> Array<decltype(apply_(InType{}))>
{
  using OutType = decltype(apply_(InType{}));

  const size_t size = input.size();
  const size_t ncomp = input.ncomp();
  if (size * ncomp < 1)
    return Array<OutType>();

  Array<OutType> output;
  output.resize(size, ncomp);

  const InType * in_ptr = input.get_device_ptr_const();
  OutType * out_ptr = output.get_device_ptr();
  const UnaryFunctor apply = apply_;

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size * ncomp),
      [=] DRAY_LAMBDA (int32 i)
  {
    out_ptr[i] = apply(in_ptr[i]);
  });

  return output;
}




template <typename T>
Array<T> array_exc_scan_plus(Array<T> &array_of_sizes)
{
  T * in_ptr = array_of_sizes.get_device_ptr();

  const size_t arr_size = array_of_sizes.size();

  Array<T> array_of_sums;
  array_of_sums.resize(arr_size);
  T * out_ptr = array_of_sums.get_device_ptr();

  RAJA::exclusive_scan<for_policy>(in_ptr, in_ptr + arr_size, out_ptr, RAJA::operators::plus<T>{});

  return array_of_sums;
}

template <typename T>
Array<T> array_exc_scan_plus(Array<T> &array_of_sizes, T &total_size)
{
  const size_t arr_size = array_of_sizes.size();

  Array<T> array_of_sums = array_exc_scan_plus(array_of_sizes);

  total_size = (arr_size > 0
      ? array_of_sizes.get_value(arr_size-1) + array_of_sums.get_value(arr_size-1)
      : 0);

  return array_of_sums;
}


/**
 * segmented_reduce()
 *
 * @param segment_splitters size N+1, where N is the number of segments,
 *        inclusive begins/exclusive ends, [0]->0, [N]->summands.size().
 */
template <typename T>
Array<T> segmented_reduce(Array<T> summands, Array<int32> segment_splitters)
{
  //TODO gpu-optimized without global memory
  //TODO do it without pollution from neighboring segments
  //TODO do it wihout subtraction operator (only use associative property)

  Array<T> prefix_sums = array_exc_scan_plus(summands);
  const T *prefix_sums_ptr = prefix_sums.get_device_ptr_const();
  const T *summands_ptr = summands.get_device_ptr_const();

  int32 segments = segment_splitters.size() - 1;
  const int32 *segment_splitters_ptr = segment_splitters.get_device_ptr();

  Array<T> segment_sums;
  segment_sums.resize(segments);
  T *segment_sums_ptr = segment_sums.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, segments),
      [=] DRAY_LAMBDA (int32 i)
  {
    const int32 begin = segment_splitters_ptr[i];
    const int32 end = segment_splitters_ptr[i+1];
    const T first_prefix = prefix_sums_ptr[begin];
    const T last_prefix = prefix_sums_ptr[end-1];
    const T last_summand = summands_ptr[end-1];
    const T sum = last_prefix - first_prefix + last_summand;
    segment_sums_ptr[i] = sum;
  });

  return segment_sums;
}


//
// return a compact array containing the indices of the flags
// that are set
//
template <typename T>
static inline Array<T> index_flags (Array<int32> &flags, const Array<T> &ids)
{
  const int32 size = flags.size ();
  // TODO: there is an issue with raja where this can't be const
  // when using the CPU
  // const uint8 *flags_ptr = flags.get_device_ptr_const();
  int32 *flags_ptr = flags.get_device_ptr ();
  Array<int32> offsets;
  offsets.resize (size);
  int32 *offsets_ptr = offsets.get_device_ptr ();

  RAJA::operators::safe_plus<int32> plus{};
  RAJA::exclusive_scan<for_policy> (flags_ptr, flags_ptr + size, offsets_ptr, plus);
  DRAY_ERROR_CHECK();

  int32 out_size = (size > 0) ? offsets.get_value (size - 1) : 0;
  // account for the exclusive scan by adding 1 to the
  // size if the last flag is positive
  if (size > 0 && flags.get_value (size - 1) > 0) out_size++;

  Array<T> output;
  output.resize (out_size);
  T *output_ptr = output.get_device_ptr ();

  const T *ids_ptr = ids.get_device_ptr_const ();
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    int32 in_flag = flags_ptr[i];
    // if the flag is valid gather the sparse intput into
    // the compact output
    if (in_flag > 0)
    {
      const int32 out_idx = offsets_ptr[i];
      output_ptr[out_idx] = ids_ptr[i];
    }
  });
  DRAY_ERROR_CHECK();

  return output;
}

static inline Array<int32> index_flags (Array<int32> &flags)
{
  // The width of the flags array must match the width of the offsets array (int32).
  // Otherwise something goes wrong; either plus<small_type> overflows
  // or maybe the exclusive_scan<> template doesn't handle two different types.
  // Using a uint8 flags, things were broken, but changing to int32 fixed it.

  const int32 size = flags.size ();
  // TODO: there is an issue with raja where this can't be const
  // when using the CPU
  // const uint8 *flags_ptr = flags.get_device_ptr_const();
  int32 *flags_ptr = flags.get_device_ptr ();
  Array<int32> offsets;
  offsets.resize (size);
  int32 *offsets_ptr = offsets.get_device_ptr ();

  RAJA::operators::safe_plus<int32> plus{};
  RAJA::exclusive_scan<for_policy> (flags_ptr, flags_ptr + size, offsets_ptr, plus);
  DRAY_ERROR_CHECK();

  int32 out_size = (size > 0) ? offsets.get_value (size - 1) : 0;
  // account for the exclusive scan by adding 1 to the
  // size if the last flag is positive
  if (size > 0 && flags.get_value (size - 1) > 0) out_size++;

  Array<int32> output;
  output.resize (out_size);
  int32 *output_ptr = output.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    int32 in_flag = flags_ptr[i];
    // if the flag is valid gather the sparse intput into
    // the compact output
    if (in_flag > 0)
    {
      const int32 out_idx = offsets_ptr[i];
      output_ptr[out_idx] = i;
    }
  });
  DRAY_ERROR_CHECK();

  return output;
}


template <typename X>
static inline Array<int32> index_any_nonzero (Array<X> items)
{
  Array<int32> flags;
  flags.resize(items.size());

  const int32 size = items.size();
  const int32 ncomp = items.ncomp();

  ConstDeviceArray<X> items_deva(items);
  NonConstDeviceArray<int32> flags_deva(flags);

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    bool is_nonzero = false;
    for (int32 component = 0; component < ncomp; ++component)
      if (items_deva.get_item(i, component) != X{})
        is_nonzero = true;
    flags_deva.get_item(i) = int32(is_nonzero);
  });

  return index_flags(flags);
}

template <typename X>
static inline Array<int32> index_all_nonzero (Array<X> items)
{
  Array<int32> flags;
  flags.resize(items.size());

  const int32 size = items.size();
  const int32 ncomp = items.ncomp();

  ConstDeviceArray<X> items_deva(items);
  NonConstDeviceArray<int32> flags_deva(flags);

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  {
    bool is_nonzero = true;
    for (int32 component = 0; component < ncomp; ++component)
      if (!(items_deva.get_item(i, component) != X{}))
        is_nonzero = false;
    flags_deva.get_item(i) = int32(is_nonzero);
  });

  return index_flags(flags);
}


//
// this function produces a list of ids less than or equal to the input ids
// provided.
//
// ids: an index into the 'input' array where any value in ids must be > 0
//      and < input.size()
//
// input: any array type that can be used with a unary functor
//
// BinaryFunctor: a binary operation that returns a boolean value. If false
//               the index from ids is removed in the output and if true
//               the index remains. Ex functor that returns true for any
//               input value > 0.
//
// forward declare
template <typename T, typename X, typename Y, typename BinaryFunctor>
static inline Array<T>
compact (Array<T> &ids, Array<X> &input_x, Array<Y> &input_y, BinaryFunctor _apply)
{
  if (ids.size () < 1)
  {
    return Array<T> ();
  }

  const T *ids_ptr = ids.get_device_ptr_const ();
  const X *input_x_ptr = input_x.get_device_ptr_const ();
  const Y *input_y_ptr = input_y.get_device_ptr_const ();

  // avoid lambda capture issues by declaring new functor
  BinaryFunctor apply = _apply;

  const int32 size = ids.size ();
  Array<uint8> flags;
  flags.resize (size);

  uint8 *flags_ptr = flags.get_device_ptr ();

  // apply the functor to the input to generate the compact flags
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    const int32 idx = ids_ptr[i];
    bool flag = apply (input_x_ptr[idx], input_y_ptr[idx]);
    int32 out_val = 0;

    if (flag)
    {
      out_val = 1;
    }

    flags_ptr[i] = out_val;
  });
  DRAY_ERROR_CHECK();

  return index_flags<T> (flags, ids);
}

template <typename UnaryFunctor>
static inline Array<int32> array_where_true(int32 size, UnaryFunctor _apply)
{
  if (size < 1)
  {
    return Array<int32> ();
  }

  // avoid lambda capture issues by declaring new functor
  UnaryFunctor apply = _apply;

  Array<int32> flags;
  flags.resize (size);

  int32 *flags_ptr = flags.get_device_ptr ();

  // apply the functor to the input to generate the compact flags
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    bool flag = apply (i);
    int32 out_val = 0;

    if (flag)
    {
      out_val = 1;
    }

    flags_ptr[i] = out_val;
  });
  DRAY_ERROR_CHECK();

  return index_flags(flags);
}

template <typename X, typename UnaryFunctor>
static inline Array<int32> array_where_true(Array<X> &input_x, UnaryFunctor _apply)
{
  struct ApplyToLookup
  {
    ConstDeviceArray<X> m_xs;
    UnaryFunctor m_apply;
    //---------------------------------
    ApplyToLookup(Array<X> xs, UnaryFunctor apply) : m_xs(xs), m_apply(apply) {}
    ApplyToLookup(const ApplyToLookup &) = default;
    //---------------------------------
    DRAY_EXEC bool operator()(int32 i)
    {
      return m_apply(m_xs.get_item(i));
    }
  };

  ApplyToLookup apply(input_x, _apply);
  return array_where_true(input_x.size(), apply);
}


template <typename T, typename X, typename UnaryFunctor>
static inline Array<T> compact (Array<T> &ids, Array<X> &input_x, UnaryFunctor _apply)
{
  if (ids.size () < 1)
  {
    return Array<T> ();
  }

  const T *ids_ptr = ids.get_device_ptr_const ();
  const X *input_x_ptr = input_x.get_device_ptr_const ();

  // avoid lambda capture issues by declaring new functor
  UnaryFunctor apply = _apply;

  const int32 size = ids.size ();
  Array<uint8> flags;
  flags.resize (size);

  uint8 *flags_ptr = flags.get_device_ptr ();

  // apply the functor to the input to generate the compact flags
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    const int32 idx = ids_ptr[i];
    bool flag = apply (input_x_ptr[idx]);
    int32 out_val = 0;

    if (flag)
    {
      out_val = 1;
    }

    flags_ptr[i] = out_val;
  });
  DRAY_ERROR_CHECK();

  return index_flags<T> (flags, ids);
}


template <typename T, typename IndexFunctor>
static inline Array<T> compact (Array<T> &ids, IndexFunctor _filter)
{
  if (ids.size () < 1)
  {
    return Array<T> ();
  }

  const T *ids_ptr = ids.get_device_ptr_const ();

  // avoid lambda capture issues by declaring new functor
  IndexFunctor filter = _filter;

  const int32 size = ids.size ();
  Array<int32> flags;
  flags.resize (size);

  int32 *flags_ptr = flags.get_device_ptr ();

  // apply the functor to the input to generate the compact flags
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    const int32 idx = ids_ptr[i];
    bool flag = filter (idx);
    int32 out_val = 0;

    if (flag)
    {
      out_val = 1;
    }

    flags_ptr[i] = out_val;
  });
  DRAY_ERROR_CHECK();

  return index_flags<T> (flags, ids);
}


// A strange compactor over a ternary functor and three sizes of arrays.
// The small input array has intrinsic indices (an i for an i).
// There is an array of indices for the mid input array and for the large input array.
// The arrays of indices have the same size as the small input array.
// Uses the mid index array for ids.
template <typename T, typename X, typename Y, typename Z, class TernaryFunctor>
static inline Array<T> compact (Array<T> &large_ids,
                                Array<T> &mid_ids,
                                Array<X> &input_large,
                                Array<Y> &input_mid,
                                Array<Z> &input_small,
                                TernaryFunctor _apply)
{
  if (mid_ids.size () < 1)
  {
    return Array<T> ();
  }

  const T *large_ids_ptr = large_ids.get_device_ptr_const ();
  const T *mid_ids_ptr = mid_ids.get_device_ptr_const ();
  const X *input_large_ptr = input_large.get_device_ptr_const ();
  const Y *input_mid_ptr = input_mid.get_device_ptr_const ();
  const Z *input_small_ptr = input_small.get_device_ptr_const ();

  // avoid lambda capture issues by declaring new functor
  TernaryFunctor apply = _apply;

  const int32 size = input_small.size ();
  Array<uint8> flags;
  flags.resize (size);

  uint8 *flags_ptr = flags.get_device_ptr ();

  // apply the functor to the input to generate the compact flags
  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    const int32 large_idx = large_ids_ptr[i];
    const int32 mid_idx = mid_ids_ptr[i];
    bool flag =
    apply (input_large_ptr[large_idx], input_mid_ptr[mid_idx], input_small_ptr[i]);
    int32 out_val = 0;

    if (flag)
    {
      out_val = 1;
    }

    flags_ptr[i] = out_val;
  });
  DRAY_ERROR_CHECK();

  return index_flags<T> (flags, mid_ids);
}


// This method returns an array of a subset of the values from input.
// The output has the same length as indices, where each element of the output
// is drawn from input using the corresponding index in indices.
template <typename T>
static inline Array<T> gather (const Array<T> input, Array<int32> indices)
{
  const int32 size_ind = indices.size ();

  Array<T> output;
  output.resize (size_ind);

  const T *input_ptr = input.get_device_ptr_const ();
  const int32 *indices_ptr = indices.get_device_ptr_const ();
  T *output_ptr = output.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size_ind), [=] DRAY_LAMBDA (int32 ii) {
    output_ptr[ii] = input_ptr[indices_ptr[ii]];
  });
  DRAY_ERROR_CHECK();

  return output;
}

// Same as above, except input is assumed to be a flattened array
// consisting of chunks of size 'chunk_size', and 'indices' refers to chunks
// instead of individual elements.
template <typename T>
static Array<T> gather (const Array<T> input, int32 chunk_size, Array<int32> indices)
{
  const int32 size_ind = indices.size ();

  Array<T> output;
  output.resize (chunk_size * size_ind);

  const T *input_ptr = input.get_device_ptr_const ();
  const int32 *indices_ptr = indices.get_device_ptr_const ();
  T *output_ptr = output.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, chunk_size * size_ind), [=] DRAY_LAMBDA (int32 ii) {
    const int32 chunk_id = ii / chunk_size;  //TODO use nested iteration instead of division.
    const int32 within_chunk = ii % chunk_size;
    output_ptr[ii] = input_ptr[chunk_size * indices_ptr[chunk_id] + within_chunk];
  });
  DRAY_ERROR_CHECK();

  return output;
}


// output_in_place[out_indices[i]] = input[i]
template <typename T>
static inline void scatter (
    const Array<T> input,
    const Array<int32> out_indices,
    Array<T> output_in_place)
{
  const int32 size_ind = out_indices.size ();
  assert(size_ind == input.size());

  if (size_ind > 0)
  {
    const T *input_ptr = input.get_device_ptr_const ();
    const int32 *indices_ptr = out_indices.get_device_ptr_const ();
    T *output_ptr = output_in_place.get_device_ptr ();

    RAJA::forall<for_policy> (RAJA::RangeSegment (0, size_ind), [=] DRAY_LAMBDA (int32 ii) {
      output_ptr[indices_ptr[ii]] = input_ptr[ii];
    });
    DRAY_ERROR_CHECK();
  }
}





static inline Array<int32> array_counting (const int32 &size,
                                           const int32 &start,
                                           const int32 &step)
{

  Array<int32> iterator;
  iterator.resize (size);
  int32 *ptr = iterator.get_device_ptr ();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {
    ptr[i] = start + i * step;
  });
  DRAY_ERROR_CHECK();

  return iterator;
}

static inline Array<int32> array_random (const int32 &size,
                                         const uint64 &seed,
                                         const int32 &modulus)
{
  // I wanted to use both 'seed' and 'sequence number' (see CUDA curand).
  // The caller provides seed, which is shared by all array elements;
  // but different array elements have different sequence numbers.
  // The sequence numbers should advance by (size) on successive calls.

  // For the serial case, I will instead use a "call number" and change the
  // seed, by feeding (given seed + call number) into the random number
  // generator. Unfortunately, two arrays each of size N will get different
  // entries than one array of size 2N.

  static uint64 call_number = 1; // Not 0: Avoid calling srand() with 0 and then 1.
  // static uint64 sequence_start = 0;    //future: for parallel random

  // Allocate the array.
  Array<int32> rand_array;
  rand_array.resize (size);
  // int32 *ptr = rand_array.get_device_ptr();
  int32 *host_ptr = rand_array.get_host_ptr ();

  // Initialize serial random number generator, then fill array.
  srand (seed + call_number);
  for (int32 i = 0; i < size; i++)
    host_ptr[i] = rand () % modulus;


  // TODO parallel random number generation
  //  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 i)
  //  {
  //    curandState_t state;
  //    curand_init(seed, sequence_start + i, 0, &state);
  //    ptr[i] = curand(&state);
  //  });
  DRAY_ERROR_CHECK();

  call_number++;
  // sequence_start += size;

  return rand_array;
}


// Inputs: Array of something convertible to bool.
//
// Outputs: Array of destination indices. ([out])
//          The size number of things that eval'd to true.
template <typename T>
static inline Array<int32> array_compact_indices (const Array<T> src, int32 &out_size)
{
  const int32 in_size = src.size ();

  Array<int32> dest_indices;
  dest_indices.resize (in_size);

  // Convert the source array to 0s and 1s.
  { // (Limit the scope of one-time-use array pointers.)
    const int32 *src_ptr = src.get_device_ptr_const ();
    int32 *dest_indices_ptr = dest_indices.get_device_ptr ();
    RAJA::forall<for_policy> (RAJA::RangeSegment (0, in_size), [=] DRAY_LAMBDA (int32 ii) {
      dest_indices_ptr[ii] = (int32) (bool)src_ptr[ii];
    });
  }

  // Use an exclusive prefix sum to compute the destination indices.
  {
    int32 *dest_indices_ptr = dest_indices.get_device_ptr ();
    RAJA::exclusive_scan_inplace<for_policy> (dest_indices_ptr, dest_indices_ptr + in_size,
                                              RAJA::operators::plus<int32>{});
    DRAY_ERROR_CHECK();
  }

  // Retrieve the size of the output array.
  out_size = *(dest_indices.get_host_ptr_const () + in_size - 1) +
             ((*src.get_host_ptr_const ()) ? 1 : 0);

  return dest_indices;
}


template <typename T>
static inline Array<int32> index_where(const Array<T> src, const T match_)
{
  if (src.size() < 1)
    return Array<int32>();

  const int32 in_size = src.size();

  Array<int32> dest_indices;
  dest_indices.resize(in_size);

  const T match = match_;
  const T * src_ptr = src.get_device_ptr_const();
  int32 * flags_ptr = dest_indices.get_device_ptr();

  // flags
  RAJA::ReduceSum<reduce_policy, int32> count(0);
  RAJA::forall<for_policy>(RAJA::RangeSegment(0, in_size),
      [=, &count] DRAY_LAMBDA (int32 i)
  {
    bool count_this = (match == src_ptr[i]);
    flags_ptr[i] = count_this;
    count += count_this;
  });

  // flags to indices
  RAJA::exclusive_scan_inplace<for_policy>(
      dest_indices.get_device_ptr(),
      dest_indices.get_device_ptr() + in_size);

  const int32 * dest_idx_ptr = dest_indices.get_device_ptr_const();

  Array<int32> orig_indices;
  orig_indices.resize(count.get());
  int32 * orig_indices_ptr = orig_indices.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, in_size),
      [=] DRAY_LAMBDA (int32 i)
  {
    bool count_this = (match == src_ptr[i]);
    if (count_this)
      orig_indices_ptr[dest_idx_ptr[i]] = i;
  });

  return orig_indices;
}


#ifdef DRAY_CUDA_ENABLED
inline __device__ Vec<float32, 4> const_get_vec4f (const Vec<float32, 4> *const data)
{
  const float4 temp = __ldg ((const float4 *)data);
  ;
  Vec<float32, 4> res;
  res[0] = temp.x;
  res[1] = temp.y;
  res[2] = temp.z;
  res[3] = temp.w;
  return res;
}
#else
inline Vec<float32, 4> const_get_vec4f (const Vec<float32, 4> *const data)
{
  return data[0];
}
#endif

} // namespace dray
#endif
