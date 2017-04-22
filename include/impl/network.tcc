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
#include <iostream>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "detail/utils.hpp"
#include "invalid_state.hpp"

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to_all(const data_container& data) const {
	for (const std::pair<boost::uuids::uuid, breep::peer<T>>& pair : m_peers) {
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
	for (const std::pair<boost::uuids::uuid, breep::peer<T>>& pair : m_peers) {
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
		if (new_peer != breep::constant::bad_peer<T>) {
			peer_connected(std::move(new_peer), 0);
			m_manager.run();
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
	for (std::pair<boost::uuids::uuid, breep::peer<T>>& pair : m_peers) {
		m_manager.disconnect(pair.second);
	}
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_peers); // clear and shrink
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.path_to_passing_by());
	std::unordered_map<boost::uuids::uuid, std::vector<breep::peer<T>>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.bridging_from_to());
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
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	return m_co_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network<T>::remove_disconnection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	return m_dc_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network<T>::remove_data_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	return m_data_r_listener.erase(id) > 0;
}


template <typename T>
inline void breep::network<T>::peer_connected(peer<T>&& p, unsigned char distance) {
	std::pair<boost::uuids::uuid, breep::peer<T>> pair = std::make_pair(p.id(), std::move(p));
	m_me.path_to_passing_by().insert(pair);
	m_me.bridging_from_to().insert(std::make_pair(pair.second.id(), std::vector<breep::peer<T>>{}));
	m_peers_mutex.lock();
	m_peers.insert(pair);
	m_peers_mutex.unlock();
	m_manager.process_connected_peer(m_peers.at(pair.second.id()));
	pair.second.distance(distance);
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	for(auto& l : m_co_listener) {
		l.second(*this, p, distance);
	}
}

template <typename T>
inline void breep::network<T>::peer_disconnected(const peer<T>& p) {
	m_me.path_to_passing_by().erase(p.id());
	m_me.bridging_from_to().erase(p.id());
	m_peers_mutex.lock();
	m_peers.erase(p.id());
	m_peers_mutex.unlock();
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	for(auto& l : m_dc_listener) {
		l.second(*this, p);
	}
}

template <typename T>
void breep::network<T>::data_received(const peer<T>& source, commands command, const std::vector<uint8_t>& data) {
	switch (command) {
		case commands::send_to: {
			std::vector<uint8_t> processed_data = detail::bigendian1(data);
			std::lock_guard<std::mutex> lock_guard(m_data_mutex);
			for (auto& l : m_data_r_listener) {
				l.second(*this, source, processed_data, false);
			}
			break;
		}
		case commands::send_to_all: {
			std::vector<uint8_t> processed_data = detail::bigendian1(data);
			std::lock_guard<std::mutex> lock_guard(m_data_mutex);
			for (auto& l : m_data_r_listener) {
				l.second(*this, source, processed_data, true);
			}
			break;
		}
		case commands::forward_to: {
			boost::uuids::uuid id = boost::uuids::string_generator{}(detail::bigendian2<std::string>(data));
			peer<T>& target = m_peers.at(id);
			m_me.bridging_from_to()[id].push_back(source);
			m_me.bridging_from_to()[source.id()].push_back(target);

			m_manager.send(
					commands::forwarding_to,
					detail::bigendian1(std::to_string(source.distance()) + boost::uuids::to_string(source.id())),
					target
			);
			m_manager.send(
					commands::forwarding_to,
					detail::bigendian1(std::to_string(target.distance()) + boost::uuids::to_string(target.id())),
					source
			);
			break;
		}
		case commands::stop_forwarding: {
			boost::uuids::uuid id = boost::uuids::string_generator{}(detail::bigendian2<std::string>(data));
			peer<T>& target = m_peers.at(id);
			std::vector<breep::peer<T>>& v = m_me.bridging_from_to()[id];
			auto it = std::find(v.begin(), v.end(), source); // keeping a vector because this code is unlikely to be called.
			if (it != v.end()) {
				replace(*it, v.back());
				v.pop_back();
			}

			std::vector<breep::peer<T>>& v2 = m_me.bridging_from_to()[source.id()];
			it = std::find(v2.begin(), v2.end(), target); // keeping a vector because this code is unlikely to be called.
			if (it != v2.end()) {
				replace(*it, v2.back());
				v2.pop_back();
			}
			break;
		}
		case commands::forwarding_to: {
			std::string str = detail::bigendian2<std::string>(data);
			unsigned char distance = static_cast<unsigned char>(str[0]);
			peer<T>& target = m_peers.at(boost::uuids::string_generator{}(str.substr(1)));
			replace(m_me.path_to(target), source);
			replace(m_me.path_to(source), target);
			peer_connected(peer<T>(target), static_cast<unsigned char>(distance + 1));
			break;
		}
		case commands::connect_to: {
			size_t id_size = data[0];
			std::string buff;
			buff.reserve(id_size++);
			size_t i{1};
			for (; --id_size; ++i) {
				buff.push_back(data[i]);
			}
			boost::uuids::uuid id = boost::uuids::string_generator{}(buff);
			id_size = buff.size();
			buff.clear();
			buff.reserve(data.size() - id_size - 1);
			while(i < data.size()) {
				buff.push_back(data[i++]);
			}
			peer<T> p(m_manager.connect(boost::asio::ip::address::from_string(buff), m_port));
			if (p.id() == id) {
				peer_connected(std::move(p), 0);
				m_manager.send(commands::successfully_connected, detail::bigendian1(boost::uuids::to_string(id)), source);
			} else {
				m_manager.send(commands::cant_connect, detail::bigendian1(boost::uuids::to_string(id)), source);
			}
			break;
		}
		case commands::cant_connect:break;
		case commands::successfully_connected:break;
		case commands::update_distance:break;
		case commands::retrieve_distance:break;
		case commands::peers_list:break;
		case commands::new_peer:break;
		case commands::peer_disconnection:break;
		default:
			std::cerr << "Warning: unknown command received...";
	}
}

// This method exist because it is not possible to implement peer<T>::operator=(const peer<T>&), as
// peer<T> has constant members (id & ip).
template <typename T>
inline void breep::network<T>::replace(peer<T>& ancestor, const peer<T>& successor) {
	ancestor.~peer<T>();
	new(&ancestor) peer<T>(successor);
}