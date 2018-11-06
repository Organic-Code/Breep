#ifndef BREEP_UTIL_LOGGER_HPP
#define BREEP_UTIL_LOGGER_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @file peer.hpp
 * @author Lucas Lazare
 */

#include <iostream>
#include <chrono>
#include <mutex>
#include <thread>
#include <string>

#if __cplusplus >= 201703L
#include <string_view>
#endif

#include <breep/network/detail/utils.hpp>
#include <breep/util/type_traits.hpp>

namespace breep {

	enum class log_level {
		trace,
		debug,
		info,
		warning,
		error,
		fatal,
		none
	};

	bool operator>=(log_level lhs, log_level rhs);
	inline bool operator>=(log_level lhs, log_level rhs) {
		return static_cast<int>(lhs) >= static_cast<int>(rhs);
	}

	namespace detail {
		namespace loggers_values {
#if __cplusplus >= 201703L
			inline const auto start_time = std::chrono::high_resolution_clock::now();
			inline const std::chrono::system_clock::time_point& get_starting_time() {
				return start_time;
			}

			inline auto global_maximum_level = log_level::trace;
			inline log_level& get_global_maximum_level() {
				return global_maximum_level;
			}

			inline std::mutex mutex{};
			inline std::mutex& get_mutex() {
				return mutex;
			}
#else
			static const auto start_time = std::chrono::high_resolution_clock::now();
			inline const std::chrono::system_clock::time_point& get_starting_time() {
				return start_time;
			}

			inline log_level& get_global_maximum_level() {
				static auto global_maximum_level = log_level::trace;
				return global_maximum_level;
			}

			inline std::mutex& get_mutex() {
				static std::mutex mutex{};
				return mutex;
			}
#endif
		}
	}

	/**
	 * Basic logger
	 *
	 * @since 1.0.0
	 */
	class logger {
#if __cplusplus >= 201703L
		using string_type = std::string_view;
#else
		using string_type = const std::string&;
#endif
	public:

		logger(std::string prefix) : m_level{log_level::warning}
				, tclass{std::move(prefix)}
				, hash{}
		{}

		template <typename T>
		static logger from_class() {
			return logger(type_traits<T>::universal_name().substr(0, type_traits<T>::universal_name().find('<')));
		}

		/**
		 * Sets the maximum logging level for all loggers.
		 *
		 * @since 1.0.0
		 */
		static void set_global_logging_level(log_level ll) {
			detail::loggers_values::get_global_maximum_level() = ll;
		}

		/**
		 * @since 1.0.0
		 */
		void trace(string_type str) {
			if (log_level::trace >= level()) {
				log_impl("(trace)  ", str);
			}
		}

		/**
		 * @since 1.0.0
		 */
		void debug(string_type str) {
			if (log_level::debug >= level()) {
				log_impl("(debug)  ", str);
			}
		}

		/**
		 * @since 1.0.0
		 */
		void info(string_type str) {
			if (log_level::info >= level()) {
				log_impl("(info)   ", str);
			}
		}

		/**
		 * @since 1.0.0
		 */
		void warning(string_type str) {
			if (log_level::warning >= level()) {
				log_impl("(warning)", str);
			}
		}

		/**
		 * @since 1.0.0
		 */
		void error(string_type str) {
			if (log_level::error >= level()) {
				log_impl("(error)  ", str);
			}
		}

		/**
		 * @brief calls std::terminate.
		 * @since 1.0.0
		 */
		void fatal(string_type str) {
			if (log_level::fatal >= level()) {
				log_impl("(fatal)  ", str);
			}
			std::terminate();
		}

		/**
		 * @brief calls exit(exit_code).
		 * @since 1.0.0
		 */
		void fatal(string_type str, int exit_code) {
			if (log_level::fatal >= level()) {
				log_impl("(fatal)  ", str);
			}
			std::exit(exit_code);
		}

		/**
		 * sets the logging level
		 *
		 * @since 1.0.0
		 */
		void level(log_level ll) {
			m_level = ll;
		}

		/**
		 * gets the logging level
		 *
		 * @since 1.0.0
		 */
		log_level level() {
			return std::max(m_level, detail::loggers_values::get_global_maximum_level());
		}

	private:

		void log_impl(string_type level, string_type str) {
			std::lock_guard<std::mutex> lock(detail::loggers_values::get_mutex());
			std::clog << str_base(level) << str << std::endl;
		}

		std::string str_base(string_type str) {
#if __cplusplus >= 201703L
			return str_base_impl(std::string(str));
#else
			return str_base_impl(str);
#endif
		}

		std::string str_base_impl(const std::string& str) {
			auto count = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - detail::loggers_values::get_starting_time()).count();
			auto h = count / 3600;
			auto m = (count - h * 3600) / 60;
			auto s = count % 60;

			std::string h_str = h < 10 ? "0" + std::to_string(h) : std::to_string(h);
			std::string m_str = m < 10 ? "0" + std::to_string(m) : std::to_string(m);
			std::string s_str = s < 10 ? "0" + std::to_string(s) : std::to_string(s);


			return str + " [" + h_str + ':' + m_str + ':' + s_str + "] " + tclass + "@"
			       + std::to_string(hash(std::this_thread::get_id())).substr(0, 4) + ": ";

		}

		log_level m_level;
		std::string tclass;
		std::hash<std::thread::id> hash;
	};
}
#endif //BREEP_UTIL_LOGGER_HPP
