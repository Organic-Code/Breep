#ifndef BREEP_OBJECT_BUILDER_HPP
#define BREEP_OBJECT_BUILDER_HPP

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
 * @file basic_network.hpp
 * @author Lucas Lazare
 */

#include <mutex>
#include <functional>
#include <unordered_map>
#include <boost/archive/binary_iarchive.hpp>

namespace breep {
	template <typename>
	class basic_network;
}

namespace breep::detail {

	template <typename io_manager, typename T>
	class object_builder {
	public:

		using network = basic_network<io_manager>;
		using peer = typename network::peer;

		template <typename>
		using data_received_listener = std::function<void(network& lnetwork, const peer& received_from, const T& data, bool sent_to_all)>;

		object_builder()
				: m_listeners{}
				, m_mutex{}
		{}

		object_builder(const object_builder<T,network>&) = delete;
		object_builder(object_builder<T,network>&& other)
				: m_listeners(std::move(other.m_listeners))
				, m_mutex{}
		{}

		object_builder& operator=(const object_builder<T,network>&) = delete;

		bool build_and_call(network& lnetwork, const typename network::peer& received_from, boost::archive::binary_iarchive& data, bool sent_to_all) {
			if (m_listeners.empty()) {
				return false;
			} else {
				T object;
				data >> object;
				std::lock_guard<std::mutex> lock_guard(m_mutex);
				for (auto& listeners_pair : m_listeners) {
					listeners_pair.second(lnetwork, received_from, object, sent_to_all);
				}
				return true;
			}
		}

		listener_id add_listener(listener_id id, data_received_listener<T> l) {
			std::lock_guard<std::mutex> lock_guard(m_mutex);
			m_listeners.insert(std::pair<listener_id, data_received_listener<T>>(id, l));
			return id;
		}

		bool remove_listener(listener_id id) {
			std::lock_guard<std::mutex> lock_guard(m_mutex);
			return m_listeners.erase(id) > 0;
		}


	private:
		std::unordered_map<listener_id, data_received_listener<T>> m_listeners;
		std::mutex m_mutex;
	};
}


#endif //BREEP_OBJECT_BUILDER_HPP
