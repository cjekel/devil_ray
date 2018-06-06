#include <dray/array.hpp>
#include <dray/array_internals.hpp>

namespace dray
{

template<typename T> 
Array<T>::Array()
 : m_internals(new ArrayInternals<T>())
{
};

template<typename T> 
Array<T>::Array(const T *data, const int32 size)
 : m_internals(new ArrayInternals<T>(data, size))
{
};

template<typename T> 
void
Array<T>::set(const T *data, const int32 size)
{
  m_internals->set(data, size);
};

template<typename T>
Array<T>::~Array()
{

}

template<typename T>
void Array<T>::operator=(const Array<T> &other)
{
  m_internals = other.m_internals;
}

template<typename T>
size_t Array<T>::size() const
{
  return m_internals->size();
}

template<typename T>
void 
Array<T>::resize(const size_t size)
{
  m_internals->resize(size);
}

template<typename T>
T* 
Array<T>::get_host_ptr()
{
  return m_internals->get_host_ptr();
}

template<typename T>
T* 
Array<T>::get_device_ptr()
{
  return m_internals->get_device_ptr();
}

template<typename T>
const T* 
Array<T>::get_host_ptr_const() const
{
  return m_internals->get_host_ptr_const();
}

template<typename T>
const T* 
Array<T>::get_device_ptr_const() const
{
  return m_internals->get_device_ptr_const(); 
}

template<typename T>
void
Array<T>::summary()
{
  m_internals->summary();
}

// Type Explicit instatiations
template class Array<int32>;
template class Array<uint32>;
template class Array<int64>;
template class Array<uint64>;
template class Array<float32>;
template class Array<float64>;

} // namespace dray

// Class Explicit instatiations
#include <dray/aabb.hpp>
template class dray::Array<dray::AABB>;

#include <dray/vec.hpp>
template class dray::Array<dray::Vec<dray::uint32,2>>;

template class dray::Array<dray::Vec<dray::float32,3>>;
template class dray::Array<dray::Vec<dray::float64,3>>;

template class dray::Array<dray::Vec<dray::float32,4>>;
template class dray::Array<dray::Vec<dray::float64,4>>;
