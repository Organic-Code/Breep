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
	for (const peer& p : m_peers) {
		const peer& p2 = m_me.path_to(p);
		if (&p == &p2) {
			m_manager.send(commands::send_to_all, data, p.address());
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to_all(data_container&& data) const {
	send_to_all(data);
}


template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to(const peer& p, const data_container& data) const {
	m_manager.send(commands::send_to, data, m_me.path_to(p).address());
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to(const peer& p, data_container&& data) const {
	m_manager.send(commands::send_to, data, m_me.path_to(p).address());
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
		return m_manager.connect(address);
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
	for (const peer& p : m_peers) {
		m_manager.disconnect(p.address());
	}
	std::vector<peer>{}.swap(m_peers); // clear and shrink
}

template <typename T>
inline listener_id breep::network<T>::add_listener(connection_listener listener) {
	return add_listener(std::move(listener));
}

template <typename T>
inline listener_id breep::network<T>::add_listener(connection_listener&& listener) {
	m_co_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline listener_id breep::network<T>::add_listener(data_received_listener listener) {
	return add_listener(std::move(listener));
}

template <typename T>
inline listener_id breep::network<T>::add_listener(data_received_listener&& listener) {
	m_data_r_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline listener_id breep::network<T>::add_listener(disconnection_listener listener) {
	return add_listener(std::move(listener));
}

template <typename T>
inline listener_id breep::network<T>::add_listener(disconnection_listener&& listener) {
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
