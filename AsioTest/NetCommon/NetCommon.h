
#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE

#pragma warning(push)
#pragma warning(disable:6255)
#pragma warning(disable:6387)
#pragma warning(disable:6031)
#pragma warning(disable:6258)
#pragma warning(disable:6001)
#pragma warning(disable:26812)
#pragma warning(disable:26498)
#pragma warning(disable:26495)
#pragma warning(disable:26451)
#pragma warning(disable:26439)
#include <boost/asio.hpp>
#pragma warning(pop)
