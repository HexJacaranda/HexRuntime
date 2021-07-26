#pragma once
#include "RuntimeAlias.h"
#include <memory>
#include <vector>

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/thread_pool.h>

namespace Runtime
{
	using Logger = std::shared_ptr<spdlog::async_logger>;

#define GET_LOGGER_METHOD static std::shared_ptr<spdlog::async_logger> GetLogger(\
	std::vector<spdlog::sink_ptr>& sinks,\
	std::weak_ptr<spdlog::details::thread_pool> threadPool)

	template<class U>
	struct SpecificLogger
	{
		GET_LOGGER_METHOD
		{
			auto logger = std::make_shared<spdlog::async_logger>("hex-runtime", 
				sinks.begin(), 
				sinks.end(), 
				threadPool, 
				spdlog::async_overflow_policy::block);

			logger->set_pattern("[%n (%t)][%c] %l : %v");
			spdlog::register_logger(logger);

			return logger;
		}
	};

#define SPECIFIC_LOGGER_FOR(TYPE) \
	template<> \
	struct SpecificLogger<TYPE>

	class LoggerFactory
	{
		static std::shared_ptr<spdlog::details::thread_pool> defaultPool;
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
			return SpecificLogger<U>::GetLogger(sinks, defaultPool);
		}
	};

#define INJECT_LOGGER(TYPE) Logger mLogger =  LoggerFactory::Get<TYPE>()

#define LOG_INFO(MESSAGE, ...) mLogger->info(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_WARN(MESSAGE, ...) mLogger->warn(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_ERROR(MESSAGE, ...) mLogger->error(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_CRITICAL(MESSAGE, ...) mLogger->critical(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_DEBUG(MESSAGE, ...) mLogger->debug(TEXT(MESSAGE), __VA_ARGS__)
}