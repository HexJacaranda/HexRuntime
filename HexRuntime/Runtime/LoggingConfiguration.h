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

	struct SpecificLogger
	{
		GET_LOGGER_METHOD
		{
			auto logger = std::make_shared<spdlog::async_logger>("hex-runtime",
				sinks.begin(),
				sinks.end(),
				threadPool,
				spdlog::async_overflow_policy::block);

			logger->set_level(spdlog::level::debug);
			logger->set_pattern("[%n(%t)][%l]:%v");
			spdlog::register_logger(logger);

			return logger;
		}
	};

#define SPECIFIC_LOGGER_FOR(TYPE) \
	struct TYPE##SpecificLogger
}