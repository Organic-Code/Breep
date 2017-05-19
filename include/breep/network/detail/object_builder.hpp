#ifndef BREEP_NETWORK_OBJECT_BUILDER_HPP
#define BREEP_NETWORK_OBJECT_BUILDER_HPP

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

#include "breep/network/typedefs.hpp"

namespace breep {
	template <typename>
	class basic_network;
}

namespace breep { namespace detail {

	template <typename io_manager, typename T>
	class object_builder {
	public:

		using network = basic_network<io_manager>;
		using peer = typename network::peer;

		template <typename>
		using data_received_listener = std::function<void(network& lnetwork, const peer& received_from, const T& data, bool sent_to_all)>;

		object_builder()
				: m_listeners{}
				, m_to_add{}
			    , m_to_remove{}
				, m_mutex{}
		{}

		object_builder(const object_builder<T,network>&) = delete;
		object_builder(object_builder<T,network>&& other)
				: m_listeners(std::move(other.m_listeners))
				, m_mutex{}
		{}

		object_builder& operator=(const object_builder<T,network>&) = delete;

		bool build_and_call(network& lnetwork, const typename network::peer& received_from, boost::archive::binary_iarchive& data, bool sent_to_all) {

			m_mutex.lock();
			for (auto& pair : m_to_add) {
				m_listeners.emplace(std::move(pair));
			}
			for (auto& id : m_to_remove) {
				m_listeners.erase(id);
			}
			m_mutex.unlock();

			if (m_listeners.empty()) {
				return false;
			} else {
				T object;
				data >> object;
				for (auto& listeners_pair : m_listeners) {
					listeners_pair.second(lnetwork, received_from, object, sent_to_all);
				}
				return true;
			}
		}

		listener_id add_listener(listener_id id, data_received_listener<T> l) {
			std::lock_guard<std::mutex> lock_guard(m_mutex);
			m_to_add.push_back(std::pair<listener_id, data_received_listener<T>>(id, l));
			return id;
		}

		bool remove_listener(listener_id id) {
			if (m_listeners.count(id)) {
				std::lock_guard<std::mutex> lock_guard(m_mutex);
				if (std::find_if(m_to_remove.cbegin(), m_to_remove.cend(), [id](auto l_id) -> bool { return l_id == id; }) == m_to_remove.cend()) {
					m_to_remove.push_back(id);
					return true;
				}
			} else {
				std::lock_guard<std::mutex> lock_guard(m_mutex);
				auto it = std::find_if(m_to_add.begin(), m_to_add.end(), [id](auto l_it) -> bool { return l_it.first == id; });
				if (it != m_to_add.cend()) {
					*it = m_to_add.back();
					m_to_add.pop_back();
					return true;
				}
			}
			return false;
		}


	private:
		std::unordered_map<listener_id, data_received_listener<T>> m_listeners;
		std::vector<std::pair<listener_id, data_received_listener<T>>> m_to_add;
		std::vector<listener_id> m_to_remove;
		std::mutex m_mutex;
	};
}}


#endif //BREEP_NETWORK_OBJECT_BUILDER_HPP
