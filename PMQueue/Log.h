#pragma once
#include <mutex>
#include <iostream>
static std::mutex log_mutex;
using namespace std;
inline void log_debug(const char* val) {
#if defined(LOG_DEBUG) || defined(LOG_VERBOSE)
	std::unique_lock<std::mutex> mlock(log_mutex);
	std::cout << val << endl;  
#endif
}

inline void log_verbose(const char* val) {
#if defined(LOG_VERBOSE)
	std::unique_lock<std::mutex> mlock(log_mutex);
	std::cout << val << endl;
#endif
}