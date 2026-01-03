#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <functional>
#include <algorithm>
#include <utility>
#include <memory>
#include <vector>
#include <queue>
#include <cassert>
#include <type_traits>
#include <cstdlib>

#include "common_type.h"

#define KB(x) (1024 * x)
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define DEFAULT_ALIGNMENT alignof(std::max_align_t)

#define SIMPLE_LOG(x) std::cout << x << std::endl; 

#define VOID_DEBUG


