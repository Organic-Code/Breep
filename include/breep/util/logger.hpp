#ifndef BREEP_UTIL_LOGGER_HPP
#define BREEP_UTIL_LOGGER_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
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

	namespace detail {
		namespace logger_cst {
			inline const auto start_time = std::chrono::high_resolution_clock::now();
			inline std::mutex logging_lock{};
			inline auto global_maximum_level = log_level::trace;
		}
	}

    bool operator>=(log_level lhs, log_level rhs);
	inline bool operator>=(log_level lhs, log_level rhs) {
		return static_cast<int>(lhs) >= static_cast<int>(rhs);
	}

	namespace detail {

		template<typename T>
		class logger {
		public:

			logger() : m_level{log_level::warning}
					, tclass{type_traits<T>::universal_name().substr(0, type_traits<T>::universal_name().find('<'))}
					, hash{}
			{}

			logger& operator()() {
				return *this;
			}

			/**
			 * @since 0.1.0
			 */
			void trace(const std::string& str) {
				if (log_level::trace >= level()) {
					log_impl("(trace)  ", str);
				}
			}

			/**
			 * @since 0.1.0
			 */
			void debug(const std::string& str) {
				if (log_level::debug >= level()) {
					log_impl("(debug)  ", str);
				}
			}

			/**
			 * @since 0.1.0
			 */
			void info(const std::string& str) {
				if (log_level::info >= level()) {
					log_impl("(info)   ", str);
				}
			}

			/**
			 * @since 0.1.0
			 */
			void warning(const std::string& str) {
				if (log_level::warning >= level()) {
					log_impl("(warning)", str);
				}
			}

			/**
			 * @since 0.1.0
			 */
			void error(const std::string& str) {
				if (log_level::error >= level()) {
					log_impl("(error)  ", str);
				}
			}

			/**
			 * @brief calls abort().
			 * @since 0.1.0
			 */
			void fatal(const std::string& str) {
				if (log_level::fatal >= level()) {
					log_impl("(fatal)  ", str);
				}
				abort();
			}

			/**
			 * @brief calls exit(exit_code).
			 * @since 0.1.0
			 */
			void fatal(const std::string& str, int exit_code) {
				if (log_level::fatal >= level()) {
					log_impl("(fatal)  ", str);
				}
				exit(exit_code);
			}

			/**
			 * sets the logging level
			 *
			 * @since 0.1.0
			 */
			void level(log_level ll) {
				m_level = ll;
			}

			/**
			 * gets the logging level
			 *
			 * @since 0.1.0
			 */
			log_level level() {
				return std::max(m_level, logger_cst::global_maximum_level);
			}

		private:

			void log_impl(const std::string& level, const std::string& str) {
				std::lock_guard<std::mutex> lock(detail::logger_cst::logging_lock);
				std::clog << str_base(level) << str << std::endl;
			}

			std::string str_base(const std::string& str) {
				auto count = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - detail::logger_cst::start_time).count();
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

	template <typename T>
	detail::logger<T> logger;

	namespace logging {

		/**
		 * Sets the maximum logging level for all loggers.
		 *
		 * @since 1.0.0
		 */
		void set_max_level(log_level level);
		inline void set_max_level(log_level level) {
			detail::logger_cst::global_maximum_level = level;
		}
	}
}
#endif //BREEP_UTIL_LOGGER_HPP
