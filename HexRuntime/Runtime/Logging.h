#pragma once
#include "RuntimeAlias.h"
#include <memory>
#include <vector>
#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/thread_pool.h>

namespace Runtime
{
	using Logger = std::shared_ptr<spdlog::async_logger>;

	template<class U>
	struct SpecificLogger
	{
		std::shared_ptr<spdlog::async_logger> GetLogger(std::vector<spdlog::sink_ptr>& sinks)
		{
			auto logger = std::make_shared<spdlog::async_logger>("hex-runtime", 
				sinks.begin(), 
				sinks.end(), 
				spdlog::details::thread_pool(), 
				spdlog::async_overflow_policy::block);

			logger->set_pattern("[%n (%t)][%c] %l : %v");
			spdlog::register_logger(logger);

			return logger;
		}
	};

#define SPECIFIC_LOGGER_FOR(TYPE) \
	template<> \
	struct SpecificLogger<TYPE>


#define GET_LOGGER_METHOD std::shared_ptr<spdlog::async_logger> GetLogger(std::vector<spdlog::sink_ptr>& sinks)

	class LoggerFactory
	{
	public:
		template<class U>
		static std::shared_ptr<spdlog::async_logger> Get()
		{
			std::vector<spdlog::sink_ptr> sinks;
#ifdef RTDEBUG
			auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			stdout_sink->set_level(spdlog::level::debug);
			sinks.push_back(stdout_sink);
#endif
			return SpecificLogger<U>::GetLogger(sinks);
		}
	};

#define INJECT_LOGGER(TYPE) Logger mLogger =  LoggerFactory::Get<TYPE>()

#define INFO(MESSAGE, ...) mLogger.info(MESSAGE, ##__VA_ARGS__)

#define WARN(MESSAGE, ...) mLogger.warn(MESSAGE, ##__VA_ARGS__)

#define ERROR(MESSAGE, ...) mLogger.error(MESSAGE, ##__VA_ARGS__)

#define CRITICAL(MESSAGE, ...) mLogger.critical(MESSAGE, ##__VA_ARGS__)

#define DEBUG(MESSAGE, ...) mLogger.debug(MESSAGE, ##__VA_ARGS__)
}