#pragma once
#include "RuntimeAlias.h"
#include "LoggingConfiguration.h"

namespace Runtime
{
	class LoggerFactory
	{
		static std::shared_ptr<spdlog::details::thread_pool> defaultPool;
	public:
		template<class U>
		static Logger Get()
		{
			std::vector<spdlog::sink_ptr> sinks;
#ifdef RTDEBUG
			auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			stdout_sink->set_color_mode(spdlog::color_mode::always);
			sinks.push_back(stdout_sink);
#endif
			return U::GetLogger(sinks, defaultPool);
		}
	};

#define USE_LOGGER(TYPE) Logger mLogger

#define INJECT_LOGGER(TYPE) mLogger = LoggerFactory::Get<TYPE##SpecificLogger>()

#define LOG_INFO(MESSAGE, ...) mLogger->info(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_WARN(MESSAGE, ...) mLogger->warn(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_ERROR(MESSAGE, ...) mLogger->error(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_CRITICAL(MESSAGE, ...) mLogger->critical(TEXT(MESSAGE), __VA_ARGS__)

#define LOG_DEBUG(MESSAGE, ...) mLogger->debug(TEXT(MESSAGE), __VA_ARGS__)
}