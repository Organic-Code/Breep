///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>
#include <boost/uuid/string_generator.hpp>

#include "breep/network/detail/utils.hpp"


/**
 * @file basic_peer_manager.tcc
 * @author Lucas Lazare
 * @since 0.1.0
 */


template <typename T>
breep::basic_peer_manager<T>::basic_peer_manager(T&& manager, unsigned short port) noexcept
	: m_peers{}
	, m_co_listener{}
	, m_data_r_listener{}
	, m_dc_listener{}
	, m_predicate{[](const auto&){return true;}}
	, m_me{}
	, m_failed_connections{}
	, m_manager{std::move(manager)}
	, m_id_count{0}
	, m_port{port}
	, m_running(false)
    , m_waitfor_run{}
	, m_co_mutex{}
	, m_dc_mutex{}
	, m_data_mutex{}
	, m_thread{nullptr}

{
	static_assert(std::is_base_of<breep::io_manager_base<T>, T>::value, "Specified type not derived from breep::io_manager_base");
	static_cast<io_manager_base<T>*>(&m_manager)->owner(this);

    m_waitfor_run.lock();

	m_command_handlers[static_cast<uint8_t>(commands::send_to)]                = &breep::basic_peer_manager<T>::send_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::send_to_all)]            = &breep::basic_peer_manager<T>::send_to_all_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forward_to)]             = &breep::basic_peer_manager<T>::forward_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::stop_forwarding)]        = &breep::basic_peer_manager<T>::stop_forwarding_handler;
	m_command_handlers[static_cast<uint8_t>(commands::stopped_forwarding)]     = &breep::basic_peer_manager<T>::stopped_forwarding_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forwarding_to)]          = &breep::basic_peer_manager<T>::forwarding_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::connect_to)]             = &breep::basic_peer_manager<T>::connect_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::cant_connect)]           = &breep::basic_peer_manager<T>::cant_connect_handler;
	m_command_handlers[static_cast<uint8_t>(commands::update_distance)]        = &breep::basic_peer_manager<T>::update_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::retrieve_distance)]      = &breep::basic_peer_manager<T>::retrieve_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::retrieve_peers)]         = &breep::basic_peer_manager<T>::retrieve_peers_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peers_list)]             = &breep::basic_peer_manager<T>::peers_list_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peer_disconnection)]     = &breep::basic_peer_manager<T>::peer_disconnection_handler;
	m_command_handlers[static_cast<uint8_t>(commands::keep_alive)]             = &breep::basic_peer_manager<T>::keep_alive_handler;
	m_command_handlers[static_cast<uint8_t>(commands::connection_accepted)]    = &breep::basic_peer_manager<T>::empty_handler;
	m_command_handlers[static_cast<uint8_t>(commands::connection_refused)]     = &breep::basic_peer_manager<T>::empty_handler;

}

template <typename T>
template <typename data_container>
inline void breep::basic_peer_manager<T>::send_to_all(const data_container& data) const {

	std::vector<uint8_t> transformed_data;
	transformed_data.reserve(data.size() + m_me.id().size() + 1);
	transformed_data.push_back(m_me.id().size());
	std::copy(m_me.id().begin(), m_me.id().end(), std::back_inserter(transformed_data));
	std::copy(data.begin(), data.end(), std::back_inserter(transformed_data));

	std::vector<uint8_t> sendable_data;
	detail::make_little_endian(transformed_data, sendable_data);

	m_log.debug("Sending " + std::to_string(sendable_data.size()) + " octets");
	for (const std::pair<boost::uuids::uuid, peer>& pair : m_peers) {
		if (pair.second.distance() == 0) {
			m_log.trace("Sending to " + pair.second.id_as_string());
			m_manager.send(commands::send_to_all, sendable_data, pair.second);
		} else {
			m_log.trace("Expecting another peer to forward to " + pair.second.id_as_string() + " (no direct connection)");
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::basic_peer_manager<T>::send_to(const peer& p, const data_container& data) const {

	std::vector<uint8_t> processed_data;
	processed_data.reserve(data.size() + m_me.id().size() * 2 + 1);

	processed_data.push_back(static_cast<uint8_t>(m_me.id().size()));
	std::copy(m_me.id().begin(), m_me.id().end(), std::back_inserter(processed_data));
	std::copy(p.id().begin(),    p.id().end(),    std::back_inserter(processed_data));
	std::copy(data.cbegin(),     data.cend(),     std::back_inserter(processed_data));

	std::vector<uint8_t> sendable_data;
	detail::make_little_endian(processed_data, sendable_data);

	m_log.debug("Sending private data to " + p.id_as_string());
	m_log.debug("(" + std::to_string(data.size()) + " octets)");

	auto path = m_me.path_to(p);
	if (p.distance() != 0) {
		m_log.trace("Passing through " + path->id_as_string() + " (no direct connection)");
	}
	m_manager.send(commands::send_to, sendable_data, *path);

}

template <typename T>
inline void breep::basic_peer_manager<T>::run() {
	require_non_running();
	if (m_thread) {
		m_thread->join();
	}
	m_thread = std::make_unique<std::thread>(&peer_manager::sync_run, this);
    m_waitfor_run.lock();
}

template <typename T>
inline void breep::basic_peer_manager<T>::sync_run() {
	require_non_running();
	m_log.info("Starting the network.");
	m_running = true;
    m_waitfor_run.unlock();
	m_manager.run();
	m_running = false;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::connect(const boost::asio::ip::address& address, unsigned short port_) {
	if (try_connect(address, port_)) {
		run();
		return true;
	} else {
		return false;
	}
}

template <typename T>
inline bool breep::basic_peer_manager<T>::sync_connect(const boost::asio::ip::address& address, unsigned short port_) {
	if (try_connect(address, port_)) {
		sync_run();
		return true;
	}
	return false;
}

template <typename T>
void breep::basic_peer_manager<T>::disconnect() {

	m_log.info("Shutting the network off.");

	m_manager.disconnect();
	for (auto&& peer_pair : m_peers) {
		peer& p = peer_pair.second;
		m_manager.disconnect(p);

		p.distance(std::numeric_limits<unsigned char>::max());
		m_log.info("Peer " + p.id_as_string() + " disconnected");

		std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
		for(auto& l : m_dc_listener) {
			try {
				m_log.trace("Calling disconnection listener (id: " + std::to_string(l.first) + ")");
				l.second(*this, p);

			} catch (const std::exception& e) {
				m_log.warning("Exception thrown while calling disconnection listener " + l.first);
				m_log.warning(e.what());
			} catch (const std::exception* e) {
				m_log.warning("Exception thrown while calling disconnection listener " + l.first);
				m_log.warning(e->what());
				delete e;
			}
		}
	}

	m_peers.clear();
	m_me.path_to_passing_by().clear();
	m_me.bridging_from_to().clear();
	m_failed_connections.clear();
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_connection_listener(connection_listener listener) {
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	m_co_listener.emplace(m_id_count, listener);
	m_log.trace("Adding connection listener (id: " + std::to_string(m_id_count) + ")");
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_data_listener(data_received_listener listener){
	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	m_data_r_listener.emplace(m_id_count, listener);
	m_log.trace("Adding data listener (id: " + std::to_string(m_id_count) + ")");
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::basic_peer_manager<T>::add_disconnection_listener(disconnection_listener listener){
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	m_dc_listener.emplace(m_id_count, listener);
	m_log.trace("Adding disconnection listener (id: " + std::to_string(m_id_count) + ")");
	return m_id_count++;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_connection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	m_log.trace("Removing connection listener (id: " + std::to_string(m_id_count) + ")");
	return m_co_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_disconnection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	m_log.trace("Removing disconnection listener (id: " + std::to_string(m_id_count) + ")");
	return m_dc_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::basic_peer_manager<T>::remove_data_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	m_log.trace("Removing data listener (id: " + std::to_string(m_id_count) + ")");
	return m_data_r_listener.erase(id) > 0;
}

/* PRIVATE */

template <typename T>
bool breep::basic_peer_manager<T>::try_connect(const boost::asio::ip::address& address, unsigned short port_){
	/* /!\ todo: what if two peers connect at once on the opposite side of the network? /!\
	 * (maybe use the ignored commands::successfully_connected command?)
	 * (care however, as this command is currently being sent > to remove).
	 */
	require_non_running();
	detail::optional<peer> new_peer(m_manager.connect(address, port_));
	if (new_peer) {
		m_log.info
				("Successfully connected to " + new_peer->id_as_string() + "@" + address.to_string() + ":" + std::to_string(port_));
		boost::uuids::uuid uuid = new_peer->id();
		m_ignore_predicate = true;
		peer_connected(std::move(*new_peer));
		m_ignore_predicate = false;

		m_manager.send(commands::retrieve_peers, constant::unused_param, m_peers.at(uuid));
		return true;

	} else {
		m_log.warning
				("Connection to [" + address.to_string() + "]:" + std::to_string(port_) + " failed");
		m_manager.disconnect();
		return false;
	}
}

template <typename T>
inline void breep::basic_peer_manager<T>::peer_connected(peer&& p) {
	if (m_peers.count(p.id())) {
		m_log.warning("Peer with id " + p.id_as_string()
				+ " tried to connect, but a peer with equal id is already connected.");
		m_manager.process_connection_denial(p);
		return;
	}

	if (m_ignore_predicate || m_predicate(p)) {

		boost::uuids::uuid id = p.id();
		m_peers.emplace(std::make_pair(id, std::move(p)));

		m_me.path_to_passing_by()[id] = &m_peers.at(id);
		m_me.bridging_from_to().insert(std::make_pair(id, std::vector<const peer*>{}));


		peer& new_peer = m_peers.at(id);
		new_peer.distance(0);
		m_manager.process_connected_peer(new_peer);

		m_log.info("Peer " + boost::uuids::to_string(id) + " connected");

		std::lock_guard<std::mutex> lock_guard(m_co_mutex);
		for(auto& l : m_co_listener) {
			try {
				m_log.trace("Calling connection listener (id: " + std::to_string(l.first) + ")");
				l.second(*this, p);

			} catch (const std::exception& e) {
				m_log.warning("Exception thrown while calling connection listener " + l.first);
				m_log.warning(e.what());
			} catch (const std::exception* e) {
				m_log.warning("Exception thrown while calling connection listener " + l.first);
				m_log.warning(e->what());
				delete e;
			}
		}
	} else {
		m_log.info("Peer " + boost::uuids::to_string(p.id()) + ": local connection_predicate rejected the connection");
		m_manager.process_connection_denial(p);
	}
}

template <typename T>
inline void breep::basic_peer_manager<T>::peer_connected(peer&& p, unsigned char distance, peer& bridge) {
	boost::uuids::uuid id = p.id();
	m_peers.emplace(std::make_pair(id, std::move(p)));

	m_me.path_to_passing_by()[id] = &bridge;
	m_me.bridging_from_to().insert(std::make_pair(id, std::vector<const peer *>{}));

	peer& new_peer = m_peers.at(id);
	new_peer.distance(distance);
	m_manager.process_connected_peer(new_peer);

	m_log.info("Peer " + boost::uuids::to_string(id) + " connected");

	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	for (auto& l : m_co_listener) {
		try {
			m_log.trace("Calling connection listener (id: " + std::to_string(l.first) + ")");
			l.second(*this, new_peer);

		} catch (const std::exception& e) {
			m_log.warning("Exception thrown while calling connection listener " + l.first);
			m_log.warning(e.what());
		} catch (const std::exception * e) {
			m_log.warning("Exception thrown while calling connection listener " + l.first);
			m_log.warning(e->what());
			delete e;
		}
	}

	update_distance(p);
}

template <typename T>
inline void breep::basic_peer_manager<T>::peer_disconnected(peer& p) {
	p.distance(std::numeric_limits<unsigned char>::max());

	m_log.info("Peer " + p.id_as_string() + " disconnected");

	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	for(auto& l : m_dc_listener) {
		try {
			m_log.trace("Calling disconnection listener (id: " + std::to_string(l.first) + ")");
			l.second(*this, p);

		} catch (const std::exception& e) {
			m_log.warning("Exception thrown while calling disconnection listener " + l.first);
			m_log.warning(e.what());
		} catch (const std::exception* e) {
			m_log.warning("Exception thrown while calling disconnection listener " + l.first);
			m_log.warning(e->what());
			delete e;
		}
	}
	m_me.path_to_passing_by().erase(p.id());
	m_me.bridging_from_to().erase(p.id());

	m_peers.erase(p.id());
}

template <typename T>
void breep::basic_peer_manager<T>::data_received(const peer& source, commands command, const std::vector<uint8_t>& data) {
	((*this).*(m_command_handlers[static_cast<uint8_t>(command)]))(source, data);
}

template <typename T>
void breep::basic_peer_manager<T>::update_distance(const peer& concerned_peer) {
	std::vector<uint8_t> data;
	const boost::uuids::uuid& uuid = concerned_peer.id();
	data.reserve(1 + sizeof(uuid.data));
	data.push_back(concerned_peer.distance());
	std::copy(uuid.data, uuid.data + sizeof(uuid.data), std::back_inserter(data));

	std::vector<uint8_t> sendable;
	detail::make_little_endian(data, sendable);
	for (const auto& the_peer : m_peers) {
		if (the_peer.second.distance() == 0 && the_peer.second.id() != concerned_peer.id()) {
			m_manager.send(commands::update_distance, sendable, the_peer.second);
		}
	}
}


template <typename T>
inline void breep::basic_peer_manager<T>::forward_if_needed(const peer& source, commands command, const std::vector<uint8_t>& data) {
	const std::vector<const peer*>& peers =	m_me.bridging_from_to().at(source.id());
	for (const peer* the_peer : peers) {
		m_log.trace
				("Forwarding " + std::to_string(data.size()) + " octets from " + source.id_as_string() + " to " + the_peer->id_as_string());
		m_manager.send(command, data, *the_peer);
	}
}

template <typename T>
void breep::basic_peer_manager<T>::send_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> processed_data;
	detail::unmake_little_endian(data, processed_data);

	size_t id_size = processed_data[0];
	// todo: check if vector is empty before hand

	boost::uuids::uuid sender_id, target_id;
	if (id_size != sender_id.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}

	if (processed_data.size() < 1 + 2 * id_size) {
		m_log.error("Unexpectedly small amount of data received.");
		return;
	}
	std::copy(processed_data.begin() + 1, processed_data.begin() + 1 + id_size, sender_id.begin());
	std::copy(processed_data.begin() + 1 + id_size, processed_data.begin() + 1 + 2 * id_size, target_id.begin());

	if (!m_peers.count(sender_id)) {
		m_log.error("Received data from peer " + boost::uuids::to_string(sender_id)
				+ " which is disconnected.");
		return;
	}

	if (m_me.id() == target_id) {

		const peer& sender(m_peers.at(sender_id));
		m_log.debug
				("Received " + std::to_string(data.size() - id_size) + " octets in a private message from " + sender.id_as_string());
		std::lock_guard<std::mutex> lock_guard(m_data_mutex);
		for (auto& l : m_data_r_listener) {
			try {
				m_log.trace("Calling data listener (id: " + std::to_string(l.first) + ")");
				l.second(*this, sender, processed_data.data() + 1 + 2 * id_size, processed_data.size() - 1 - 2 * id_size, false);

			} catch (const std::exception& e) {
				m_log.warning("Exception thrown while calling data listener " + l.first);
				m_log.warning(e.what());
			} catch (const std::exception* e) {
				m_log.warning("Exception thrown while calling data listener " + l.first);
				m_log.warning(e->what());
				delete e;
			}
		}
	} else {
		auto& vect = m_me.bridging_from_to().at(source.id());
		if (std::find_if(vect.begin(), vect.end(), [target_id](const peer* p) { return p->id() == target_id; }) != vect.end()) {
			try {
				m_manager.send(commands::send_to, data, *m_me.path_to(m_peers.at(target_id)));
				m_log.trace("Forwarding private message to " + boost::uuids::to_string(target_id));
			} catch (const std::out_of_range&) {
				m_log.warning("Received message to forward to " + boost::uuids::to_string(target_id) + " which is unknown");
			}
		} else {
			m_log.warning("Received private message that local peer was not meant to receive...");
			m_log.warning("(target was " + boost::uuids::to_string(target_id) + ", local peer is " + m_me.id_as_string() + ").");
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::send_to_all_handler(const peer& source, const std::vector<uint8_t>& data) {

	forward_if_needed(source, commands::send_to_all, data);

	std::vector<uint8_t> processed_data;
	detail::unmake_little_endian(data, processed_data);

	m_log.debug
			("Received " + std::to_string(data.size()) + "octets from " + source.id_as_string());

	uint8_t id_size = processed_data[0];
	boost::uuids::uuid id;
	if (id.size() != id_size) {
		m_log.warning("Received an id with incorrect size.");
		return;
	}
	std::copy(++processed_data.begin(), processed_data.begin() + id_size + 1, id.begin());

	const peer* actual_source = &source;
	if (id != source.id()) {
		m_log.debug("Actual source: " + boost::uuids::to_string(id));
		auto source_it = m_peers.find(id);
		if (source_it != m_peers.end()) {
			actual_source = &(source_it->second);
		} else {
			m_log.warning("Received data from unknown peer: " + boost::uuids::to_string(id) + ".");
			m_log.warning("Maybe its connection was refused, but someone is bridging");
			return;
		}
	}

	std::lock_guard<std::mutex> lock_guard(m_data_mutex);

	for (auto& l : m_data_r_listener) {
		try {
			m_log.trace("Calling data listener (id: " + std::to_string(l.first) + ")");
			l.second(*this, *actual_source, processed_data.data() + id_size + 1, processed_data.size() - id_size - 1, true);

		} catch (const std::exception& e) {
			m_log.warning("Exception thrown while calling data listener " + l.first);
			m_log.warning(e.what());
		} catch (const std::exception* e) {
			m_log.warning("Exception thrown while calling data listener " + l.first);
			m_log.warning(e->what());
			delete e;
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::forward_to_handler(const peer& source, const std::vector<uint8_t>& data) {

	std::string id;
	detail::unmake_little_endian(data, id);
	boost::uuids::uuid uuid;
	if (id.size() != uuid.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(id.begin(), id.end(), uuid.data);

	try {
		peer& target = m_peers.at(uuid);
		m_me.bridging_from_to().at(uuid).push_back(&m_peers.at(source.id()));
		m_me.bridging_from_to().at(source.id()).push_back(&target);

		m_log.trace("Now bridging from " + source.id_as_string() + " to " + boost::uuids::to_string(uuid));

		std::vector<uint8_t> ldata;
		std::uint8_t dist = target.distance();
		const boost::uuids::uuid& target_id = target.id();
		detail::make_little_endian(
				std::string(&dist, &dist + 1) + std::string(target_id.begin(), target_id.end())
				, ldata
		);
		m_manager.send(commands::forwarding_to, ldata, source);

		ldata.clear();
		dist = source.distance();
		const boost::uuids::uuid& source_id = source.id();
		detail::make_little_endian(
				std::string(&dist, &dist + 1) + std::string(source_id.begin(), source_id.end())
				, ldata
		);
		m_manager.send(commands::forwarding_to, ldata, target);

	} catch (const std::out_of_range&) {
		m_log.warning("Received untreatable forwarding request: from " + source.id_as_string() + " to " + boost::uuids::to_string(uuid));
	}
}

template <typename T>
void breep::basic_peer_manager<T>::stop_forwarding_handler(const peer& source, const std::vector<uint8_t>& data) {

	std::string data_str;
	detail::unmake_little_endian(data, data_str);
	boost::uuids::uuid id;
	if (data_str.size() != id.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(data_str.begin(), data_str.end(), id.data);


	auto target_it = m_peers.find(id);
	// TODO: this check every time
	// vvvvvvvv
	if (target_it == m_peers.end()) {
		m_log.info("Ignoring invalid bridge stopping request from " + source.id_as_string() + " [requested unknown id " + boost::uuids::to_string(id) + "].");
		return;
	}

	peer& target = target_it->second;

	m_log.trace("Stopping to forward from " + source.id_as_string() + " to " + target.id_as_string());

	std::vector<const peer*>& v = m_me.bridging_from_to().at(target.id());
	auto it = std::find_if(v.begin(), v.end(), [&source](const peer* p) { return p->id() == source.id(); });
	if (it != v.end()) {
		*it = v.back();
		v.pop_back();
	}

	std::vector<const peer*>& v2 = m_me.bridging_from_to().at(source.id());
	it = std::find_if(v2.begin(), v2.end(), [&target](const peer* p) { return p->id() == target.id(); });
	if (it != v2.end()) {
		*it = v2.back();
		v2.pop_back();
	}

	std::vector<uint8_t> sendable_id;
	detail::make_little_endian(detail::unowning_linear_container(source.id().data), sendable_id);
	m_manager.send(commands::stopped_forwarding, sendable_id, target);
}

template <typename T>
void breep::basic_peer_manager<T>::stopped_forwarding_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string data_str;
	detail::unmake_little_endian(data, data_str);
	boost::uuids::uuid id;
	if (data_str.size() != id.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(data_str.begin(), data_str.end(), id.data);

	auto target_it = m_peers.find(id);
	if (target_it == m_peers.end()) {
		m_log.warning("Ignoring invalid bridge stop acknowledgement from " + source.id_as_string() + " [requested unknown id " + boost::uuids::to_string(id) + "].");
		return;
	}

	peer& target = target_it->second;
	if (m_me.path_to(target)->id() != source.id()) {
		m_log.warning("Received an unused bridge stop acknowledgement from " + source.id_as_string());
		return;
	}

	m_log.info(source.id_as_string() + " stopped bridging to " + target.id_as_string());
	peer_disconnected(target);
}

template <typename T>
void breep::basic_peer_manager<T>::forwarding_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string str;
	detail::unmake_little_endian(data, str);
	boost::uuids::uuid uuid;
	if (str.size() != uuid.size() + 1) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(str.begin() + 1, str.end(), uuid.data);

	unsigned char distance = static_cast<unsigned char>(str[0]);

	auto target = m_peers.find(uuid);
	if (target != m_peers.end()) {
		m_me.path_to(target->second) = &source;
		target->second.distance(static_cast<unsigned char>(distance + 1));
		m_log.trace("Peer " + source.id_as_string() + " is now bridging local peer to " + boost::uuids::to_string(uuid)
					+ " (distance: " + std::to_string(target->second.distance()) + ")");
	} else {
		auto p = std::find_if(m_failed_connections.begin(), m_failed_connections.end(), [&uuid](const std::unique_ptr<peer>& p_) {
			return p_ && p_->id() == uuid;
		});

		if (p != m_failed_connections.end()) {
			m_log.debug("Peer " + boost::uuids::to_string(uuid) + " connected through bridging.");

			p->swap(m_failed_connections.back());
			peer_connected(std::move(*p->get()), static_cast<unsigned char>(distance + 1), m_peers.at(source.id()));
			m_failed_connections.pop_back();
		} else {

			m_log.warning("Peer " + source.id_as_string() + " attempted to bridge to " + boost::uuids::to_string(uuid) + ", but the latter is not known.");
			m_log.warning("Maybe its connection was refused.");
			std::vector<uint8_t> sendable_id;
			detail::make_little_endian(detail::unowning_linear_container(uuid.data), sendable_id);
			m_manager.send(commands::stop_forwarding, sendable_id, source);
		}
	}
}

template <typename T>
void breep::basic_peer_manager<T>::connect_to_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> ldata;
	detail::unmake_little_endian(data, ldata);
	auto remote_port = static_cast<unsigned short>(ldata[0] << 8 | ldata[1]);

	size_t id_size = ldata[2];
	std::string buff;
	buff.reserve(id_size++);
	size_t i{3};
	for (; --id_size; ++i) {
		buff.push_back(ldata[i]);
	}

	boost::uuids::uuid id;
	if (buff.size() != id.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(buff.begin(), buff.end(), id.data);
	id_size = buff.size();

	std::string buff2;
	buff2.reserve(ldata.size() - id_size - 1);
	while(i < ldata.size()) {
		buff2.push_back(ldata[i++]);
	}

	boost::asio::ip::address addr = boost::asio::ip::address::from_string(buff2);

	peer attempted_peer(id, addr, remote_port);
	if (m_predicate(attempted_peer)) {

		m_log.debug("Connecting to " + boost::uuids::to_string(id) + "@" + addr.to_string() + ":" +
		            std::to_string(remote_port));
		detail::optional<peer> p(m_manager.connect(addr, remote_port));

		ldata.clear();
		detail::make_little_endian(buff, ldata);

		if (p && p->id() == id) {
			m_log.trace("Connection successful");
			m_ignore_predicate = true;
			peer_connected(std::move(*p));
			m_ignore_predicate = false;
		} else {
			m_log.trace("Connection failed. Requesting a forwarding.");
			m_failed_connections.push_back(std::make_unique<peer>(attempted_peer));
			m_manager.send(commands::forward_to, ldata, source);
		}
	} else {
		m_log.info("Peer " + attempted_peer.id_as_string() + ": local connection_predicate rejected the outgoing connection");
	}
}

template <typename T>
void breep::basic_peer_manager<T>::cant_connect_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> id_vect;
	detail::unmake_little_endian(data, id_vect);

	boost::uuids::uuid target_id;
	if (id_vect.size() != target_id.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(id_vect.begin(), id_vect.end(), target_id.data);

	std::vector<uint8_t> data_to_send;
	const boost::uuids::uuid& source_id = source.id();
	std::string source_addr = source.address().to_string();
	data_to_send.reserve(3 + source_id.size());

	unsigned short remote_port = source.connection_port();
	detail::insert_uint16(data_to_send, remote_port);
	data_to_send.push_back(static_cast<uint8_t>(source_id.size()));
	std::copy(source_id.data, source_id.data + source_id.size(), std::back_inserter(data_to_send));
	std::copy(source_addr.cbegin(), source_addr.cend(), std::back_inserter(data_to_send));

	std::vector<uint8_t> buffer;
	detail::make_little_endian(data_to_send, buffer);
	m_manager.send(commands::connect_to, buffer, m_peers.at(target_id));
}

template <typename T>
void breep::basic_peer_manager<T>::update_distance_handler(const peer& source, const std::vector<uint8_t>& data) {
	std::string ldata;
	detail::unmake_little_endian(data, ldata);
	boost::uuids::uuid uuid;
	std::copy(++ldata.begin(), ldata.end(), uuid.data);
	auto distance = static_cast<uint8_t>(ldata[0] + 1);
	auto item = m_peers.find(uuid);
	if (item != m_peers.end()) {
		peer& p = item->second;
		if (p.distance() > distance) {
			m_log.trace("Found a better path for " + p.id_as_string() + " (through " + source.id_as_string() + ")");
			std::vector<uint8_t> peer_id;
			detail::make_little_endian(detail::unowning_linear_container(uuid.data), peer_id);
			m_manager.send(commands::forward_to, peer_id, source);

			std::vector<uint8_t> sendable;
			detail::make_little_endian(std::string(&distance, &distance + 1) + std::string(uuid.begin(), uuid.end()), sendable);
			for (const auto& peer_p : m_peers) {
				if (peer_p.second.distance() == 0) {
					m_manager.send(commands::update_distance, sendable, peer_p.second);
				}
			}
		}

	} else {
		auto p = std::find_if(m_failed_connections.begin(), m_failed_connections.end(), [&uuid](const std::unique_ptr<peer>& p_) {
			return p_ && p_->id() == uuid;
		});

		if (p != m_failed_connections.end()) {
			std::vector<uint8_t> peer_id;
			m_log.trace("Path to " + (*p)->id_as_string() + " found (through " + source.id_as_string() + "). Requesting bridge");
			detail::make_little_endian(detail::unowning_linear_container(uuid.data), peer_id);
			m_manager.send(commands::forward_to, peer_id, source);
			std::vector<uint8_t> sendable;
			detail::make_little_endian(std::string(&distance, &distance + 1) + std::string(uuid.data, uuid.data + uuid.size()), sendable);
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
	boost::uuids::uuid uuid;
	if (id.size() != uuid.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(id.begin(), id.end(), uuid.data);

	unsigned char dist = m_peers.at(uuid).distance();
	std::vector<uint8_t> ldata;
	detail::make_little_endian(std::string(&dist, &dist + 1) + std::string(uuid.begin(), uuid.end()), ldata);
	m_log.trace("Sending distances to " + source.id_as_string());
	m_manager.send(commands::update_distance, ldata, source);
}

template <typename T>
void breep::basic_peer_manager<T>::retrieve_peers_handler(const peer& source, const std::vector<uint8_t>& /*data*/) {

	auto peers_nbr = static_cast<unsigned short>(m_peers.size());
	assert(peers_nbr == m_peers.size());

	std::vector<uint8_t> ans;
	ans.reserve(2 + peers_nbr * (2 + m_me.id_as_string().size() + m_me.address().to_string().size()));
	detail::insert_uint16(ans, peers_nbr);


	for (auto& p : m_peers) {
		detail::insert_uint16(ans, p.second.connection_port());

		const boost::uuids::uuid& uuid = p.second.id();
		ans.push_back(static_cast<uint8_t>(uuid.size()));
		std::copy(uuid.data, uuid.data + uuid.size(), std::back_inserter(ans));

		std::string addr = p.second.address().to_string();
		ans.push_back(static_cast<uint8_t>(addr.size()));
		std::copy(addr.cbegin(), addr.cend(), std::back_inserter(ans));
	}

	std::vector<uint8_t> ldata;
	detail::make_little_endian(ans, ldata);
	m_log.trace("Sending peers list to " + source.id_as_string());
	m_manager.send(commands::peers_list, ldata, source);
}

template <typename T>
void breep::basic_peer_manager<T>::peers_list_handler(const peer& source, const std::vector<uint8_t>& data) {

	m_log.trace("Received a list of peers. Scanning through it.");

	std::vector<uint8_t> ldata;
	detail::unmake_little_endian(data, ldata);
	size_t peers_nbr(ldata[0] << 8 | ldata[1]);
	std::vector<peer> peers_list;
	peers_list.reserve(peers_nbr);

	size_t index = 2;
	while(peers_nbr--) {
		auto remote_port = static_cast<unsigned short>(ldata[index] << 8 | ldata[index + 1]);

		uint8_t id_size = ldata[index + 2];
		std::string id;
		id.reserve(id_size);
		index += 3;
		std::copy(ldata.cbegin() + index, ldata.cbegin() + index + id_size, std::back_inserter(id));
		boost::uuids::uuid uuid;
		if (id.size() != uuid.size()) {
			m_log.error("Received an id with incorrect size. Skipping.");
			continue;
		}
		std::copy(id.begin(), id.end(), uuid.data);
		index += id_size;

		uint8_t address_size = ldata[index++];
		std::string addr_str;
		addr_str.reserve(address_size);
		std::copy(ldata.cbegin() + index, ldata.cbegin() + index + address_size, std::back_inserter(addr_str));
		boost::asio::ip::address address(boost::asio::ip::address::from_string(addr_str));
		index += address_size;

		// boost::asio does not consider ipv4-mapped-to-ipv6-loopback-addresses as loopback addresses
		// leading to this fix
		auto is_loopback = [](const boost::asio::ip::address& addr) {
			if (addr.is_v4()) {
				return addr.is_loopback();
			}

			boost::asio::ip::address_v6 v6 = addr.to_v6();
			if (!v6.is_v4_mapped()) {
				return v6.is_loopback();
			}

			// ipv4 loopback addresses take the whole 127.0.0.0 -> 127.255.255.255 range
			// ipv6 mapped to ipv4 store the ipv4 on the last 32bits (this is in network byte order)
			return v6.to_bytes()[12] == 127;
		};

		if (is_loopback(address)) {
			address = source.address();
		}

		if (uuid != m_me.id() && m_peers.count(uuid) == 0) {
			auto eq_peer = std::find_if(peers_list.cbegin(), peers_list.cend(), [&address, &remote_port](const peer& p) {
				return p.address() == address && p.connection_port() == remote_port;
			});

			if (eq_peer == peers_list.cend()) {
				detail::optional<peer> new_peer(m_manager.connect(address, remote_port));
				if (new_peer) {
					if (new_peer->id() != uuid) {
						m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
					}
					peers_list.emplace_back(std::move(*new_peer));
				} else {
					m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
				}
			} else {
				eq_peer == std::find_if(peers_list.cbegin(), peers_list.cend(), [&uuid](const peer& p) {
					return p.id() == uuid;
				});

				if (eq_peer == peers_list.cend()) {
					m_failed_connections.push_back(std::make_unique<peer>(uuid, address, remote_port));
				}
			}
		}
	}

    m_ignore_predicate = true;
	for (auto& peer_ptr : peers_list) {
		peer_connected(std::move(peer_ptr));
	}
    m_ignore_predicate = false;

	std::vector<uint8_t> sendable_uuid;
	if (!m_failed_connections.empty()) {
		m_log.debug("There were unsuccessful connections. Asking for distances>bridging.");
	}
	for (std::unique_ptr<peer>& peer_ptr : m_failed_connections) {
		peer_ptr->distance(std::numeric_limits<unsigned char>::max());
		detail::make_little_endian(detail::unowning_linear_container(peer_ptr->id().data), sendable_uuid);

		for (const std::pair<boost::uuids::uuid, peer>& pair : m_peers) {
			if (pair.second.distance() == 0) {
				m_manager.send(commands::retrieve_distance, sendable_uuid, pair.second);
			}
		}
		sendable_uuid.clear();
	}
}

template <typename T>
void breep::basic_peer_manager<T>::peer_disconnection_handler(const peer& source, const std::vector<uint8_t>& data) {
	forward_if_needed(source, commands::peer_disconnection, data);
	std::string local_data;
	detail::unmake_little_endian(data, local_data);

	boost::uuids::uuid uuid;
	if (local_data.size() != uuid.size()) {
		m_log.error("Received an id with incorrect size.");
		return;
	}
	std::copy(local_data.begin(), local_data.end(), uuid.data);
	peer_disconnected(m_peers.at(uuid));
}
