#include "Logging.h"

std::shared_ptr<spdlog::details::thread_pool> RT::LoggerFactory::defaultPool = 
	std::make_shared<spdlog::details::thread_pool>(8192, 1);