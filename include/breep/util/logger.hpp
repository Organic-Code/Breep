#ifndef BREEP_LOGGER_HPP
#define BREEP_LOGGER_HPP

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

	namespace {
		static const auto start_time = std::chrono::high_resolution_clock::now();
		static std::mutex logging_lock{};
	}

	enum class logging_severity {
		trace,
		debug,
		info,
		warning,
		error,
		fatal
	};

	namespace detail {

		template<typename T>
		class logger {
		public:

			logger() : severity{logging_severity::info}
					, tclass{type_traits<T>::universal_name().substr(0, type_traits<T>::universal_name().find('<'))}
					, hash{}
			{}

			logger& operator()() {
				return *this;
			}

			void trace(const std::string& str) {
				if (greater_or_equal(logging_severity::trace, severity)) {
					log_impl("(trace)  ", str);
				}
			}

			void debug(const std::string& str) {
				if (greater_or_equal(logging_severity::debug, severity)) {
					log_impl("(debug)  ", str);
				}
			}

			void info(const std::string& str) {
				if (greater_or_equal(logging_severity::info, severity)) {
					log_impl("(info)   ", str);
				}
			}

			void warning(const std::string& str) {
				if (greater_or_equal(logging_severity::warning, severity)) {
					log_impl("(warning)", str);
				}
			}

			void error(const std::string& str) {
				if (greater_or_equal(logging_severity::error, severity)) {
					log_impl("(error)  ", str);
				}
			}

			void fatal(const std::string& str) {
				log_impl("(fatal)  ", str);
				abort();
			}

			void set_severity(logging_severity ls) {
				severity = ls;
			}

		private:
			bool greater_or_equal(logging_severity lhs, logging_severity rhs) {
				return static_cast<int>(lhs) >= static_cast<int>(rhs);
			}

			void log_impl(const std::string& level, const std::string& str) {
				std::lock_guard<std::mutex> lock(logging_lock);
				std::clog << str_base(level) << str << std::endl;
			}

			std::string str_base(const std::string& str) {
				auto count = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time).count();
				auto h = count / 3600;
				auto m = (count - h * 3600) / 60;
				auto s = count % 60;

				std::string h_str = h < 10 ? "0" + std::to_string(h) : std::to_string(h);
				std::string m_str = m < 10 ? "0" + std::to_string(m) : std::to_string(m);
				std::string s_str = s < 10 ? "0" + std::to_string(s) : std::to_string(s);


				return str + " [" + h_str + ':' + m_str + ':' + s_str + "] " + tclass + "@"
				       + std::to_string(hash(std::this_thread::get_id())) + ": ";

			}

			logging_severity severity;
			std::string tclass;
			std::hash<std::thread::id> hash;
		};
	}

	template <typename T>
	detail::logger<T> logger;
}
#endif //BREEP_LOGGER_HPP
