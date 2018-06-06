#include "gtest/gtest.h"
#include <dray/array.hpp>
#include <dray/array_registry.hpp>

TEST(dray_array, dray_registry_basic)
{
  dray::Array<int> int_array;
  int_array.resize(2);
  int *host = int_array.get_host_ptr();
  host[0] = 0; 
  host[1] = 1; 
  

  size_t dev_usage = dray::ArrayRegistry::device_usage();

  // we should not have allocated anything yet
  ASSERT_EQ(dev_usage, 0);
  int *dev = int_array.get_device_ptr();
  
  dev_usage = dray::ArrayRegistry::device_usage();
  // not we shold have two ints
  ASSERT_EQ(dev_usage, 2 * sizeof(int));
 
  dray::ArrayRegistry::release_device_res();
  dev_usage = dray::ArrayRegistry::device_usage();
  ASSERT_EQ(dev_usage, 0);
  
}