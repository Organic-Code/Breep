///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "breep/network/basic_peer_manager.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "breep/network/detail/utils.hpp"

template <typename T>
breep::basic_peer_manager<T>::basic_peer_manager(T&& io_manager, unsigned short port) noexcept
	: m_peers{}
	, m_co_listener{}
	, m_data_r_listener{}
	, m_dc_listener{}
	, m_master_listener{}
	, m_me{}
    , m_uuid_gen{}
	, m_failed_connections{}
	, m_manager{std::move(io_manager)}
	, m_id_count{0}
	, m_port{port}
	, m_running(false)
	, m_co_mutex{}
	, m_dc_mutex{}
	, m_data_mutex{}

{
	static_assert(std::is_base_of<breep::io_manager_base<T>, T>::value, "Specified type not derived from breep::io_manager_base");
	static_cast<io_manager_base<T>*>(&m_manager)->owner(this);

	m_command_handlers[static_cast<uint8_t>(commands::send_to)]                = &breep::basic_peer_manager<T>::send_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::send_to_all)]            = &breep::basic_peer_manager<T>::send_to_all_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forward_to)]             = &breep::basic_peer_manager<T>::forward_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::stop_forwarding)]        = &breep::basic_peer_manager<T>::stop_forwarding_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forwarding_to)]          = &breep::basic_peer_manager<T>::forwarding_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::connect_to)]             = &breep::basic_peer_manager<T>::connect_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::cant_connect)]           = &breep::basic_peer_manager<T>::cant_connect_handler;
	m_command_handlers[static_cast<uint8_t>(commands::update_distance)]        = &breep::basic_peer_manager<T>::update_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::retrieve_distance)]      = &breep::basic_peer_manager<T>::retrieve_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::retrieve_peers)]         = &breep::basic_peer_manager<T>::retrieve_peers_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peers_list)]             = &breep::basic_peer_manager<T>::peers_list_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peer_disconnection)]     = &breep::basic_peer_manager<T>::peer_disconnection_handler;
	m_command_handlers[static_cast<uint8_t>(commands::keep_alive)]             = &breep::basic_peer_manager<T>::keep_alive_handler;
}

template <typename T>
template <typename data_container>
inline void breep::basic_peer_manager<T>::send_to_all(const data_container& data) const {

	std::vector<uint8_t> sendable_data;
	detail::make_little_endian(data, sendable_data);

	for (const std::pair<boost::uuids::uuid, peer>& pair : m_peers) {
		if (pair.second.distance() == 0) {
			m_manager.send(commands::send_to_all, sendable_data, pair.second);
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::basic_peer_manager<T>::send_to(const peer& p, const data_container& data) const {
	const std::string& id_string = m_me.id_as_string();

	std::vector<uint8_t> processed_data;
	processed_data.reserve(data.size() + m_me.id_as_string().size() + 1);
	processed_data.push_back(static_cast<uint8_t>(id_string.size()));
	for (size_t i{0} ; i < id_string.size() ; ++i) {
		processed_data.emplace_back(id_string[i]);
	}
	std::copy(data.cbegin(), data.cend(), std::back_inserter(processed_data));

	std::vector<uint8_t> sendable_data;
	detail::make_little_endian(processed_data, sendable_data);
	m_manager.send(commands::send_to, sendable_data, m_me.path_to(p));
}

template <typename T>
inline void breep::basic_peer_manager<T>::run() {
	require_non_running();
	std::thread(&peer_manager::sync_run, this).detach();
}

template <typename T>
inline void breep::basic_peer_manager<T>::sync_run() {
	require_non_running();
	m_running = true;
	m_manager.run();
	m_running = false;
}

template <typename T>
inline void breep::basic_peer_manager<T>::connect(boost::asio::ip::address address, unsigned short port_) {
	require_non_running();
	std::thread(&peer_manager::sync_connect_impl, this, address, port_).detach();
}

template <typename T>
inline bool breep::basic_peer_manager<T>::sync_connect(const boost::asio::ip::address& address, unsigned short port_) {
	return sync_connect_impl(address, port_);
}

template <typename T>
void breep::basic_peer_manager<T>::disconnect() {
	m_manager.disconnect();

	m_peers.clear();
	m_me.path_to_passing_by().clear();
	m_me.bridging_from_to().clear();
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_connection_listener(connection_listener listener) {
	m_co_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_data_listener(data_received_listener listener){
	m_data_r_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_disconnection_listener(disconnection_listener listener){
	m_dc_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_connection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	return m_co_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_disconnection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	return m_dc_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_data_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	return m_data_r_listener.erase(id) > 0;
}

/* PRIVATE */

template <typename T>
inline void breep::basic_peer_manager<T>::peer_connected(peer&& p) {
	boost::uuids::uuid id = p.id();
	m_peers.emplace(std::make_pair(id, std::move(p)));

	std::pair<boost::uuids::uuid, const peer*> pair_wptr = std::make_pair(id, &(m_peers.at(id)));
	m_me.path_to_passing_by().insert(pair_wptr);

	m_me.bridging_from_to().insert(std::make_pair(id, std::vector<const peer*>{}));


	peer& new_peer = m_peers.at(id);
	new_peer.distance(0);
	m_manager.process_connected_peer(new_peer);

	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	for(auto& l : m_co_listener) {
		try {
			l.second(*this, p);
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		} catch (std::exception* e) {
			std::cerr << e->what() << std::endl;
			delete e;
		}
	}
}

template <typename T>
inline void breep::basic_peer_manager<T>::peer_connected(peer&& p, unsigned char distance, peer& bridge) {

	boost::uuids::uuid id = p.id();
	m_peers.emplace(std::make_pair(id, std::move(p)));

	std::pair<boost::uuids::uuid, const peer*> pair_wptr = std::make_pair(id, &bridge);
	m_me.path_to_passing_by().insert(pair_wptr);
	m_me.bridging_from_to().insert(std::make_pair(id, std::vector<const peer*>{}));

	peer& new_peer = m_peers.at(id);
	new_peer.distance(distance);
	m_manager.process_connected_peer(new_peer);

	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	for(auto& l : m_co_listener) {
		try {
			l.second(*this, p);
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		} catch (std::exception* e) {
			std::cerr << e->what() << std::endl;
			delete e;
		}
	}

	update_distance(p);
}

template <typename T>
inline void breep::basic_peer_manager<T>::peer_disconnected(peer& p) {
	m_me.path_to_passing_by().erase(p.id());
	m_me.bridging_from_to().erase(p.id());

	m_peers.erase(p.id());

	p.distance(std::numeric_limits<unsigned char>::max());
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	for(auto& l : m_dc_listener) {
		try {
			l.second(*this, p);
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
		} catch (std::exception* e) {
			std::cerr << e->what() << std::endl;
			delete e;
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::data_received(const peer& source, commands command, const std::vector<uint8_t>& data) {
	((*this).*(m_command_handlers[static_cast<uint8_t>(command)]))(source, data);
}

template <typename T>
void breep::basic_peer_manager<T>::update_distance(const peer& concerned_peer) {
	std::vector<uint8_t> data;
	std::string uuid = boost::uuids::to_string(concerned_peer.id());
	data.reserve(1 + uuid.size());
	data.push_back(concerned_peer.distance());
	std::copy(uuid.cbegin(), uuid.cend(), std::back_inserter(data));

	std::vector<uint8_t> sendable;
	detail::make_little_endian(data, sendable);
	for (const auto& peer : m_peers) {
		if (peer.second.distance() == 0) {
			m_manager.send(commands::update_distance, sendable, peer.second);
		}
	}
}


template <typename T>
inline void breep::basic_peer_manager<T>::forward_if_needed(const peer& source, commands command, const std::vector<uint8_t>& data) {
	const std::vector<const peer*>& peers =	m_me.bridging_from_to().at(source.id());
	for (const peer* peer : peers) {
		m_manager.send(command, data, *peer);
	}
}

template <typename T>
bool breep::basic_peer_manager<T>::sync_connect_impl(const boost::asio::ip::address address, unsigned short port_) {
	/* /!\ todo: what if two peers connect at once on the opposite side of the network? /!\
	 * (maybe use the ignored commands::successfully_connected command?)
	 * (care however, as this command is currently being sent > to remove).
	 */
	require_non_running();
	detail::optional<peer> new_peer(m_manager.connect(address, port_));
	if (new_peer) {
		boost::uuids::uuid uuid = new_peer->id();
		peer_connected(std::move(new_peer.get()));

		m_manager.send(commands::retrieve_peers, constant::unused_param, m_peers.at(uuid));
		sync_run();
		return true;
	} else {
		return false;
	}
}

template <typename T>
void breep::basic_peer_manager<T>::send_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> processed_data;
	detail::unmake_little_endian(data, processed_data);

	size_t id_size = processed_data[0];
	std::string id;
	id.reserve(id_size);
	++id_size;
	std::copy(processed_data.cbegin() + 1, processed_data.cbegin() + id_size, std::back_inserter(id));

	if (m_me.id_as_string() == id) {
		std::lock_guard<std::mutex> lock_guard(m_data_mutex);
		if (m_master_listener) {
			m_master_listener(*this, source, reinterpret_cast<char*>(processed_data.data()) + id_size, processed_data.size() - id_size, false);
		}
		for (auto& l : m_data_r_listener) {
			try {
				l.second(*this, source, processed_data.data() + id_size, processed_data.size() - id_size, false);
			}  catch (const std::exception& e) {
				std::cerr << e.what() << '\n';
			} catch (std::exception* e) {
				std::cerr << e->what() << '\n';
				delete e;
			}
		}
	} else {
		m_manager.send(commands::send_to, data, *m_me.path_to(m_peers.at(m_uuid_gen(id))));
	}
}

template <typename T>
void breep::basic_peer_manager<T>::send_to_all_handler(const peer& source, const std::vector<uint8_t>& data) {

	forward_if_needed(source, commands::send_to_all, data);

	std::vector<uint8_t> processed_data;
	detail::unmake_little_endian(data, processed_data);

	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	if (m_master_listener) {
		m_master_listener(*this, source, reinterpret_cast<char*>(processed_data.data()), processed_data.size(), false);
	}
	for (auto& l : m_data_r_listener) {
		try {
			l.second(*this, source, processed_data.data(), processed_data.size(), true);
		} catch (const std::exception& e) {
			std::cerr << e.what() << '\n';
		} catch (std::exception* e) {
			std::cerr << e->what() << '\n';
			delete e;
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::forward_to_handler(const peer& source, const std::vector<uint8_t>& data) {

	std::string id;
	detail::unmake_little_endian(data, id);
	boost::uuids::uuid uuid = m_uuid_gen(id);

	peer& target = m_peers.at(uuid);
	m_me.bridging_from_to().at(uuid).push_back(&source);
	m_me.bridging_from_to().at(source.id()).push_back(&target);

	std::vector<uint8_t> ldata;
	unsigned char dist = source.distance();
	detail::make_little_endian(std::string(&dist, &dist + 1) + boost::uuids::to_string(source.id()), ldata);
	m_manager.send(commands::forwarding_to, ldata, target);

	ldata.clear();
	dist = target.distance();
	detail::make_little_endian(std::string(&dist, &dist + 1) + boost::uuids::to_string(target.id()), ldata);
	m_manager.send(commands::forwarding_to, ldata, source);
}

template <typename T>
void breep::basic_peer_manager<T>::stop_forwarding_handler(const peer& source, const std::vector<uint8_t>& data) {

	std::string data_str;
	detail::unmake_little_endian(data, data_str);
	boost::uuids::uuid id = m_uuid_gen(data_str);
	peer& target = m_peers.at(id);

	std::vector<const peer*>& v = m_me.bridging_from_to().at(id);
	auto it = std::find(v.begin(), v.end(), &source); // keeping a vector because this code is unlikely to be called.
	if (it != v.end()) {
		*it = v.back();
		v.pop_back();
	}

	std::vector<const peer*>& v2 = m_me.bridging_from_to().at(source.id());
	it = std::find(v2.begin(), v2.end(), &target); // keeping a vector because this code is unlikely to be called.
	if (it != v2.end()) {
		*it = v2.back();
		v2.pop_back();
	}
}

template <typename T>
void breep::basic_peer_manager<T>::forwarding_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string str;
	detail::unmake_little_endian(data, str);
	boost::uuids::uuid uuid = m_uuid_gen(str.substr(1));

	unsigned char distance = static_cast<unsigned char>(str[0]);
	try {
		peer& target = m_peers.at(uuid);
		m_me.path_to(target) = &source;
		m_me.path_to(source) = &target;
		target.distance(static_cast<unsigned char>(distance + 1));
	} catch (std::out_of_range&) {
		std::unique_ptr<peer>* p = nullptr;
		size_t i{m_failed_connections.size()};
		while (p == nullptr && i--) {
			if (m_failed_connections[i]->id() == uuid) {
				p = &m_failed_connections[i];
			}
		}

		if (p != nullptr) {
			p->swap(m_failed_connections.back());
			peer_connected(std::move(*p->get()), static_cast<unsigned char>(distance + 1), m_peers.at(source.id()));
			m_failed_connections.pop_back();
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::connect_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> ldata;
	detail::unmake_little_endian(data, ldata);
	unsigned short remote_port = static_cast<unsigned short>(ldata[0] << 8 | ldata[1]);

	size_t id_size = ldata[2];
	std::string buff;
	buff.reserve(id_size++);
	size_t i{3};
	for (; --id_size; ++i) {
		buff.push_back(ldata[i]);
	}
	boost::uuids::uuid id = m_uuid_gen(buff);
	id_size = buff.size();

	std::string buff2;
	buff2.reserve(ldata.size() - id_size - 1);
	while(i < ldata.size()) {
		buff2.push_back(ldata[i++]);
	}

	detail::optional<peer> p(m_manager.connect(boost::asio::ip::address::from_string(buff2), remote_port));

	ldata.clear();
	detail::make_little_endian(buff, ldata);

	if (p && p->id() == id) {
		peer_connected(std::move(p.get()));
	} else {
		m_manager.send(commands::forward_to, ldata, source);
	}
}

template <typename T>
void breep::basic_peer_manager<T>::cant_connect_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string id_string;
	detail::unmake_little_endian(data, id_string);

	boost::uuids::uuid target_id = m_uuid_gen(id_string);

	std::vector<uint8_t> data_to_send;
	id_string = boost::uuids::to_string(source.id());
	std::string source_addr = source.address().to_string();
	data_to_send.reserve(3 + id_string.size() + target_id.size());

	unsigned short remote_port = source.connection_port();
	detail::insert_uint16(data_to_send, remote_port);
	data_to_send.push_back(static_cast<uint8_t>(id_string.size()));
	std::copy(id_string.cbegin(), id_string.cend(), std::back_inserter(data_to_send));
	std::copy(source_addr.cbegin(), source_addr.cend(), std::back_inserter(data_to_send));

	std::vector<uint8_t> buffer;
	detail::make_little_endian(data, buffer);
	m_manager.send(commands::connect_to, buffer, m_peers.at(target_id));
}

template <typename T>
void breep::basic_peer_manager<T>::update_distance_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string ldata;
	detail::unmake_little_endian(data, ldata);
	boost::uuids::uuid uuid = m_uuid_gen(ldata.substr(1));
	unsigned char distance = static_cast<unsigned char>(ldata[0] + 1);

	try {
		peer& p = m_peers.at(uuid);
		if (p.distance() > distance) {
			std::vector<uint8_t> peer_id;
			detail::make_little_endian(boost::uuids::to_string(uuid), peer_id);
			m_manager.send(commands::forward_to, peer_id, source);
			std::vector<uint8_t> sendable;
			detail::make_little_endian(std::string(&distance, &distance + 1) + boost::uuids::to_string(uuid), sendable);
			for (const auto& peer_p : m_peers) {
				if (peer_p.second.distance() == 0) {
					m_manager.send(commands::update_distance, sendable, peer_p.second);
				}
			}
		}
	} catch (std::out_of_range&) {
		std::unique_ptr<peer>* p = nullptr;
		size_t i{m_failed_connections.size()};
		while (p == nullptr && i--) {
			if (m_failed_connections[i]->id() == uuid) {
				p = &m_failed_connections[i];
			}
		}

		if (p != nullptr) {
			std::vector<uint8_t> peer_id;
			detail::make_little_endian(boost::uuids::to_string(uuid), peer_id);
			m_manager.send(commands::forward_to, peer_id, source);
			std::vector<uint8_t> sendable;
			detail::make_little_endian(std::string(&distance, &distance + 1) + boost::uuids::to_string(uuid), sendable);
			for (const auto& peer_p : m_peers) {
				if (peer_p.second.distance() == 0) {
					m_manager.send(commands::update_distance, sendable, peer_p.second);
				}
			}
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::retrieve_distance_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string id;
	detail::unmake_little_endian(data, id);
	boost::uuids::uuid uuid = m_uuid_gen(id);
	unsigned char dist = m_peers.at(uuid).distance();
	std::vector<uint8_t> ldata;
	detail::make_little_endian(std::string(&dist, &dist + 1) + boost::uuids::to_string(uuid), ldata);
	m_manager.send(commands::update_distance, ldata, source);
}

template <typename T>
void breep::basic_peer_manager<T>::retrieve_peers_handler(const peer& source, const std::vector<uint8_t>& /*data*/) {

	unsigned short peers_nbr = static_cast<unsigned short>(m_peers.size());
	assert(peers_nbr == m_peers.size());

	std::vector<uint8_t> ans;
	ans.reserve(2 + peers_nbr * (2 + m_me.id_as_string().size() + m_me.address().to_string().size()));
	detail::insert_uint16(ans, peers_nbr);


	for (auto& p : m_peers) {
		detail::insert_uint16(ans, p.second.connection_port());

		std::string id = boost::uuids::to_string(p.second.id());
		ans.push_back(static_cast<uint8_t>(id.size()));
		std::copy(id.cbegin(), id.cend(), std::back_inserter(ans));

		std::string addr = p.second.address().to_string();
		ans.push_back(static_cast<uint8_t>(addr.size()));
		std::copy(addr.cbegin(), addr.cend(), std::back_inserter(ans));
	}

	std::vector<uint8_t> ldata;
	detail::make_little_endian(ans, ldata);
	m_manager.send(commands::peers_list, ldata, source);
}

template <typename T>
void breep::basic_peer_manager<T>::peers_list_handler(const peer& /*source*/, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> ldata;
	detail::unmake_little_endian(data, ldata);
	size_t peers_nbr(ldata[0] << 8 | ldata[1]);
	std::vector<peer> peers_list;
	peers_list.reserve(peers_nbr);

	size_t index = 2;
	while(peers_nbr--) {
		unsigned short remote_port = static_cast<unsigned short>(ldata[index] << 8 | ldata[index + 1]);

		uint8_t id_size = ldata[index + 2];
		std::string id;
		id.reserve(id_size);
		index += 3;
		std::copy(ldata.cbegin() + index, ldata.cbegin() + index + id_size, std::back_inserter(id));
		boost::uuids::uuid uuid = m_uuid_gen(id);
		index += id_size;

		uint8_t address_size = ldata[index++];
		std::string addr_str;
		addr_str.reserve(address_size);
		std::copy(ldata.cbegin() + index, ldata.cbegin() + index + address_size, std::back_inserter(addr_str));
		boost::asio::ip::address address(boost::asio::ip::address::from_string(addr_str));
		index += address_size;

		if (uuid != m_me.id() && m_peers.count(uuid) == 0) {
			bool not_found = true;
			for (auto it = peers_list.cbegin(), end = peers_list.cend() ; it != end && not_found ; ++it) {
				if (it->address() == address && it->connection_port() == remote_port) {
					not_found = false;
				}
			}

			if (not_found) {
				detail::optional<peer> new_peer(m_manager.connect(address, remote_port));
				if (new_peer) {
					if (new_peer->id() != uuid) {
						m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
					}
					peers_list.emplace_back(std::move(new_peer.get()));
				} else {
					m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
				}
			} else {
				not_found = true;
				for (auto it = peers_list.cbegin(), end = peers_list.cend() ; it != end && not_found ; ++it) {
					if (it->id() == uuid) {
						not_found = false;
					}
				}
				if (not_found) {
					m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
				}
			}
		}
	}

	for (auto& peer_ptr : peers_list) {
		peer_connected(std::move(peer_ptr));
	}

	std::vector<uint8_t> vect;
	for (std::unique_ptr<peer>& peer_ptr : m_failed_connections) {
		peer_ptr->distance(std::numeric_limits<unsigned char>::max());
		detail::make_little_endian(boost::uuids::to_string(peer_ptr->id()), vect);

		for (const std::pair<boost::uuids::uuid, peer>& pair : m_peers) {
			if (pair.second.distance() == 0) {
				m_manager.send(commands::retrieve_distance, vect, pair.second);
			}
		}
		vect.clear();
	}
}

template <typename T>
void breep::basic_peer_manager<T>::peer_disconnection_handler(const peer& source, const std::vector<uint8_t>& data) {
	forward_if_needed(source, commands::peer_disconnection, data);
	std::string local_data;
	detail::unmake_little_endian(data, local_data);
	peer_disconnected(m_peers.at(m_uuid_gen(local_data)));
}