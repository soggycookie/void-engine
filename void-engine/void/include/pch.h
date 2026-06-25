#pragma once
#define _CRTDBG_MAP_ALLOC
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <crtdbg.h>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common_type.h"

#define KB(x) (1024 * x)
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define DEFAULT_ALIGNMENT alignof(std::max_align_t)

#define SIMPLE_LOG(x) std::cout << x << std::endl;
