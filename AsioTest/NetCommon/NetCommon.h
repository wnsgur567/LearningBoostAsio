
#pragma once

#include <optional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <stdint.h>
#include <queue>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <boost/asio.hpp>
