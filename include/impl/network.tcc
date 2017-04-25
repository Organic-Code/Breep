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

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to_all(const data_container& data) const {
	std::deque<uint8_t> ldata = detail::bigendian2<std::deque<uint8_t>>(data);
	ldata.push_front(static_cast<uint8_t>(ldata.size() - data.size())); // number of trailing 0 introduced by the endianness change.
	for (const std::pair<boost::uuids::uuid, breep::peer<T>>& pair : m_peers) {
		if (pair.second == m_me.path_to(pair.second)) {
			m_manager.send(commands::send_to_all, ldata, pair.second);
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::network<T>::send_to(const peer<T>& p, const data_container& data) const {
    std::deque<uint8_t> ldata;
    std::copy(data.cbegin(), data.cend(), std::back_inserter(ldata));
    std::string id_string = boost::uuids::to_string(m_me.id());
    for (size_t i{id_string.size()} ; i-- ;) {
        ldata.push_front(id_string[i]);
    }
    ldata.push_front(id_string.size());
    size_t size = ldata.size();
    ldata = detail::bigendian2<std::deque<uint8_t>>(ldata);
    ldata.push_front(ldata.size() - size); // number of trailing 0 introduced by the endianness change.
	m_manager.send(commands::send_to, ldata, m_me.path_to(p));
}

template <typename T>
inline void breep::network<T>::run() {
	require_non_running();
	std::thread(&network<T>::run_sync, this).detach();
}

template <typename T>
inline void breep::network<T>::run_sync() {
	require_non_running();
	m_running = true;
	m_manager.run();
	m_running = false;
}

template <typename T>
inline void breep::network<T>::connect(boost::asio::ip::address address, unsigned short port) {
	require_non_running();
	std::thread(&network<T>::connect_sync, this, address, port).detach();
}

template <typename T>
inline bool breep::network<T>::connect_sync(const boost::asio::ip::address& address, unsigned short port) {
	require_non_running();
    peer<T> new_peer(m_manager.connect(address, port));
	if (new_peer != breep::constant::bad_peer<T>) {
		peer_connected(std::move(new_peer), 0);
		run_sync();
		return true;
	} else {
		return false;
	}
}

template <typename T>
void breep::network<T>::disconnect() {
	std::thread(&network<T>::disconnect_sync, this).detach();
}

template <typename T>
void breep::network<T>::disconnect_sync() {
	for (std::pair<boost::uuids::uuid, breep::peer<T>> pair : m_peers) {
		m_manager.disconnect(pair.second);
	}
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_peers); // clear and shrink
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.path_to_passing_by());
	std::unordered_map<boost::uuids::uuid, std::vector<breep::peer<T>>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.bridging_from_to());
}

template <typename T>
inline breep::listener_id breep::network<T>::add_connection_listener(connection_listener listener) {
	m_co_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network<T>::add_data_listener(data_received_listener listener){
	m_data_r_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network<T>::add_disconnection_listener(disconnection_listener listener){
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
                                    // todo: store my id as string somewhere, store uuids::string_generator somewhere.
            uint8_t trailing = data[0];
			std::deque<uint8_t> processed_data = detail::bigendian2<std::deque<uint8_t>>(detail::fake_mini_container(data.data() + 1, data.size() - 1));
            while(trailing--) {
                processed_data.pop_back();
            }
            size_t id_size = processed_data[0];
            std::string id;
            id.reserve(id_size);
            processed_data.pop_front();
            for(++id_size ; --id_size ;){
                id.push_back(processed_data[0]);
                processed_data.pop_front();
            }
            if (boost::uuids::to_string(m_me.id()) == id) {
		    	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	    		for (auto& l : m_data_r_listener) {
				    l.second(*this, source, processed_data, false);
			    }
            } else {
                m_manager.send(commands::send_to, data, m_me.path_to(m_peers.at(boost::uuids::string_generator{}(id))));
            }
			break;
		}

		case commands::send_to_all: {
			forward_if_needed(source, command, data);
			// todo : extract trailing 0
			uint8_t trailing = data[0];
			std::deque<uint8_t> processed_data = detail::bigendian2<std::deque<uint8_t>>(detail::fake_mini_container(data.data() + 1, data.size() - 1));
			while (trailing--) {
				processed_data.pop_back();
			}
			std::lock_guard<std::mutex> lock_guard(m_data_mutex);
			for (auto& l : m_data_r_listener) {
				l.second(*this, source, processed_data, true);
			}
			break;
		}

		case commands::forward_to: {
            std::string id = detail::bigendian2<std::string>(data);
            for (size_t trailing = id.size() - data.size() + 1 ; --trailing ;) {
                id.pop_back();
            }
			boost::uuids::uuid id = boost::uuids::string_generator{}(id);
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
			// todo : extract trailing 0
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
			// todo : extract trailing 0
			std::string str = detail::bigendian2<std::string>(data);
			unsigned char distance = static_cast<unsigned char>(str[0]);
			peer<T>& target = m_peers.at(boost::uuids::string_generator{}(str.substr(1)));
			replace(m_me.path_to(target), source);
			replace(m_me.path_to(source), target);
			peer_connected(peer<T>(target), static_cast<unsigned char>(distance + 1));
			break;
		}

		case commands::connect_to: {
			// todo : extract trailing 0
			std::vector<uint8_t> ldata = detail::bigendian1(data);
			unsigned short port = static_cast<unsigned short>(data[0] << 8 | data[1]);
			size_t id_size = ldata[2];
			std::string buff;
			buff.reserve(id_size++);
			size_t i{3};
			for (; --id_size; ++i) {
				buff.push_back(ldata[i]);
			}
			boost::uuids::uuid id = boost::uuids::string_generator{}(buff);
			id_size = buff.size();
			buff.clear();
			buff.reserve(ldata.size() - id_size - 1);
			while(i < ldata.size()) {
				buff.push_back(ldata[i++]);
			}
			peer<T> p(m_manager.connect(boost::asio::ip::address::from_string(buff), port));
			if (p.id() == id) {
				peer_connected(std::move(p), 0);
				m_manager.send(commands::successfully_connected, detail::bigendian1(boost::uuids::to_string(id)), source);
			} else {
                // todo : send a forward_to instead of can't connect.
				m_manager.send(commands::cant_connect, , source);
			}
			break;
		}

		case commands::cant_connect: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::successfully_connected: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::update_distance: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::retrieve_distance: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::peers_list: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::new_peer: {
			// todo : extract trailing 0
			// todo
			break;
		}

		case commands::peer_disconnection: {
                                               // todo: check for forwarding and delete that guy everywhere
			// todo : extract trailing 0
			forward_if_needed(source, command, data);
			// todo
			break;
		}

		case commands::null_command:
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

template <typename T>
inline void breep::network<T>::forward_if_needed(const peer<T>& source, commands command, const std::vector<uint8_t>& data) {
	const std::vector<breep::peer<T>>& peers =	m_me.bridging_from_to().at(source.id());
	for (const peer<T>& peer : peers) {
		m_manager.send(command, data, peer);
	}
}
