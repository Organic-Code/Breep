///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "network.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

#include <thread>

#include "invalid_state.hpp"

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to_all(const data_container& data) const {
	for (const std::pair<boost::uuids::uuid, peer<T>>& pair : m_peers) {
		if (pair.second == m_me.path_to(pair.second)) {
			m_manager.send(commands::send_to_all, data, pair.second);
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to_all(data_container&& data) const {
	send_to_all(data);
}

template <typename T>
template <typename data_iterator>
void breep::network<T>::send_to_all(const data_iterator& begin, const data_iterator& end) const {
	for (const std::pair<boost::uuids::uuid, peer<T>>& pair : m_peers) {
		if (pair.second == m_me.path_to(pair.second)) {
			m_manager.send(commands::send_to_all, begin, end, pair.second);
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to(const peer<T>& p, const data_container& data) const {
	m_manager.send(commands::send_to, data, m_me.path_to(p));
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to(const peer<T>& p, data_container&& data) const {
	m_manager.send(commands::send_to, data, m_me.path_to(p));
}

template <typename T>
template <typename data_iterator>
inline void breep::network<T>::send_to(const peer<T>& p, const data_iterator& begin, const data_iterator& end) const {
	m_manager.send(commands::send_to, begin, end, m_me.path_to(p));
}

template <typename T>
inline void breep::network<T>::connect(const boost::asio::ip::address& address) {
	std::thread(&network<T>::connect_sync, this, address).detach();
}

template <typename T>
inline void breep::network<T>::connect(boost::asio::ip::address&& address) {
	connect(address);
}

template <typename T>
inline bool breep::network<T>::connect_sync(const boost::asio::ip::address& address) {
	if (m_peers.empty()) {
		peer<T> new_peer(m_manager.connect(address, m_port));
		if (new_peer != peer<T>::bad_peer) {
			m_peers.insert(std::make_pair(new_peer.id(), new_peer));
			m_me.path_to_passing_by().insert(std::make_pair(new_peer.id(), new_peer));
			m_me.bridging_from_to().insert(std::make_pair(new_peer.id(), new_peer));
			return true;
		} else {
			return false;
		}
	} else {
		throw invalid_state("Cannot connect to more than one network.");
	}
}

template <typename T>
inline bool breep::network<T>::connect_sync(boost::asio::ip::address&& address) {
	return connect_sync(address);
}

template <typename T>
void breep::network<T>::disconnect() {
	std::thread(&network<T>::disconnect_sync, this).detach();
}

template <typename T>
void breep::network<T>::disconnect_sync() {
	for (const std::pair<boost::uuids::uuid, peer<T>>& pair : m_peers) {
		m_manager.disconnect(pair.second);
	}
	std::unordered_map<boost::uuids::uuid, peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_peers); // clear and shrink
	std::unordered_map<boost::uuids::uuid, peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.path_to_passing_by());
	std::unordered_map<boost::uuids::uuid, peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.bridging_from_to());
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(connection_listener listener) {
		return add_listener(std::move(listener));
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(connection_listener&& listener) {
	m_co_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(data_received_listener listener) {
	return add_listener(std::move(listener));
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(data_received_listener&& listener) {
	m_data_r_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(disconnection_listener listener) {
	return add_listener(std::move(listener));
}

template <typename T>
inline breep::listener_id breep::network<T>::add_listener(disconnection_listener&& listener) {
	m_dc_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline bool breep::network<T>::remove_connection_listener(listener_id id) {
	return m_co_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network<T>::remove_disconnection_listener(listener_id id) {
	return m_dc_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network<T>::remove_data_listener(listener_id id) {
	return m_data_r_listener.erase(id) > 0;
}
