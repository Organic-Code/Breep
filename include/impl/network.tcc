///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "network_manager.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

#include <thread>
#include <iostream>
#include <functional>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "detail/utils.hpp"

template <typename T>
breep::network_manager<T>::network_manager(T&& io_manager, unsigned short port) noexcept
	: m_peers{}
	, m_co_listener{}
	, m_data_r_listener{}
	, m_dc_listener{}
	, m_me{}
    , m_uuid_gen{}
	, m_manager{std::move(io_manager)}
	, m_id_count{0}
	, m_port{port}
	, m_running(false)
	, m_co_mutex{}
	, m_dc_mutex{}
	, m_data_mutex{}
	, m_peers_mutex{}

{
	static_assert(std::is_base_of<breep::io_manager_base<T>, T>::value, "Specified type not derived from breep::io_manager_base");
	static_cast<io_manager_base<T>*>(&m_manager)->owner(this);

	m_command_handlers[static_cast<uint8_t>(commands::send_to)] = &breep::network_manager<T>::send_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::send_to_all)] = &breep::network_manager<T>::send_to_all_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forward_to)] = &breep::network_manager<T>::forward_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::stop_forwarding)] = &breep::network_manager<T>::stop_forwarding_handler;
	m_command_handlers[static_cast<uint8_t>(commands::forwarding_to)] = &breep::network_manager<T>::forwarding_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::connect_to)] = &breep::network_manager<T>::connect_to_handler;
	m_command_handlers[static_cast<uint8_t>(commands::cant_connect)] = &breep::network_manager<T>::cant_connect_handler;
	m_command_handlers[static_cast<uint8_t>(commands::successfully_connected)] = &breep::network_manager<T>::successfully_connected_handler;
	m_command_handlers[static_cast<uint8_t>(commands::update_distance)] = &breep::network_manager<T>::update_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::retrieve_distance)] = &breep::network_manager<T>::retrieve_distance_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peers_list)] = &breep::network_manager<T>::peers_list_handler;
	m_command_handlers[static_cast<uint8_t>(commands::new_peer)] = &breep::network_manager<T>::new_peer_handler;
	m_command_handlers[static_cast<uint8_t>(commands::peer_disconnection)] = &breep::network_manager<T>::peer_disconnection_handler;
}

template <typename T>
template <typename data_container>
inline void breep::network_manager<T>::send_to_all(const data_container& data) const {
	std::deque<uint8_t> ldata = detail::littleendian2<std::deque<uint8_t>>(data);
	ldata.push_front(static_cast<uint8_t>(ldata.size() - data.size())); // number of trailing 0 introduced by the endianness change.

	for (const std::pair<boost::uuids::uuid, breep::peer<T>>& pair : m_peers) {
		if (pair.second == m_me.path_to(pair.second)) {
			m_manager.send(commands::send_to_all, ldata, pair.second);
		}
	}
}

template <typename T>
template <typename data_container>
inline void breep::network_manager<T>::send_to(const peer<T>& p, const data_container& data) const {
    std::deque<uint8_t> ldata;
    std::copy(data.cbegin(), data.cend(), std::back_inserter(ldata));
    const std::string& id_string = m_me.id_as_string();
    for (size_t i{id_string.size()} ; i-- ;) {
        ldata.emplace_front(id_string[i]);
    }
    ldata.push_front(static_cast<uint8_t>(id_string.size()));

    size_t size = ldata.size();
    ldata = detail::littleendian2<std::deque<uint8_t>>(ldata);
    ldata.push_front(static_cast<uint8_t>(ldata.size() - size)); // number of trailing 0 introduced by the endianness change.
	m_manager.send(commands::send_to, ldata, m_me.path_to(p));
}

template <typename T>
inline void breep::network_manager<T>::run() {
	require_non_running();
	std::thread(&network_manager<T>::run_sync, this).detach();
}

template <typename T>
inline void breep::network_manager<T>::run_sync() {
	require_non_running();
	m_running = true;
	m_manager.run();
	m_running = false;
}

template <typename T>
inline void breep::network_manager<T>::connect(boost::asio::ip::address address, unsigned short port) {
	require_non_running();
	port(port);
	std::thread(&network_manager<T>::connect_sync_impl, this, address).detach();
}

template <typename T>
inline bool breep::network_manager<T>::connect_sync(const boost::asio::ip::address& address, unsigned short port) {
	port(port);
	return connect_sync_impl(address);
}

template <typename T>
void breep::network_manager<T>::disconnect() {
	std::thread(&network_manager<T>::disconnect_sync, this).detach();
}

template <typename T>
void breep::network_manager<T>::disconnect_sync() {
	for (std::pair<boost::uuids::uuid, breep::peer<T>> pair : m_peers) {
		m_manager.disconnect(pair.second);
	}
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_peers); // clear and shrink
	std::unordered_map<boost::uuids::uuid, breep::peer<T>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.path_to_passing_by());
	std::unordered_map<boost::uuids::uuid, std::vector<breep::peer<T>>, boost::hash<boost::uuids::uuid>>{}.swap(m_me.bridging_from_to());
}

template <typename T>
inline breep::listener_id breep::network_manager<T>::add_connection_listener(connection_listener listener) {
	m_co_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network_manager<T>::add_data_listener(data_received_listener listener){
	m_data_r_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline breep::listener_id breep::network_manager<T>::add_disconnection_listener(disconnection_listener listener){
	m_dc_listener.emplace(m_id_count, listener);
	return m_id_count++;
}

template <typename T>
inline bool breep::network_manager<T>::remove_connection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_co_mutex);
	return m_co_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network_manager<T>::remove_disconnection_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
	return m_dc_listener.erase(id) > 0;
}

template <typename T>
inline bool breep::network_manager<T>::remove_data_listener(listener_id id) {
	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	return m_data_r_listener.erase(id) > 0;
}

/* PRIVATE */

template <typename T>
inline void breep::network_manager<T>::peer_connected(peer<T>&& p, unsigned char distance) {
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
inline void breep::network_manager<T>::peer_disconnected(const peer<T>& p) {
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
void breep::network_manager<T>::data_received(const peer<T>& source, commands command, const std::vector<uint8_t>& data) {
	std::invoke(m_command_handlers[static_cast<uint8_t>(command)], *this, source, data);
}

// This method exist because it is not possible to implement peer<T>::operator=(const peer<T>&), as
// peer<T> has constant members (id & ip).
template <typename T>
inline void breep::network_manager<T>::replace(peer<T>& ancestor, const peer<T>& successor) {
	ancestor.~peer<T>();
	new(&ancestor) peer<T>(successor);
}

template <typename T>
inline void breep::network_manager<T>::forward_if_needed(const peer<T>& source, commands command, const std::vector<uint8_t>& data) {
	const std::vector<breep::peer<T>>& peers =	m_me.bridging_from_to().at(source.id());
	for (const peer<T>& peer : peers) {
		m_manager.send(command, data, peer);
	}
}

template <typename T>
bool breep::network_manager<T>::connect_sync_impl(const boost::asio::ip::address address) {
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
void breep::network_manager<T>::send_to_handler(const peer<T>& source, const std::vector<uint8_t>& data) {
	std::deque<uint8_t> processed_data = detail::littleendian2<std::deque<uint8_t>>(
			detail::fake_mini_container(data.data() + 1, data.size() - 1));
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(processed_data.size() - data.size()) ; trailing-- ;) {
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

	if (m_me.id_as_string() == id) {
		std::lock_guard<std::mutex> lock_guard(m_data_mutex);
		for (auto& l : m_data_r_listener) {
			l.second(*this, source, processed_data, false);
		}
	} else {
		m_manager.send(commands::send_to, data, m_me.path_to(m_peers.at(m_uuid_gen(id))));
	}
}

template <typename T>
void breep::network_manager<T>::send_to_all_handler(const peer<T>& source, const std::vector<uint8_t>& data) {

	forward_if_needed(source, commands::send_to_all, data);

	std::deque<uint8_t> processed_data = detail::littleendian2<std::deque<uint8_t>>(
			detail::fake_mini_container(data.data() + 1, data.size() - 1));
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(processed_data.size() - data.size()) ; trailing-- ;) {
		processed_data.pop_back();
	}

	std::lock_guard<std::mutex> lock_guard(m_data_mutex);
	for (auto& l : m_data_r_listener) {
		l.second(*this, source, processed_data, true);
	}
}

template <typename T>
void breep::network_manager<T>::forward_to_handler(const peer<T>& source, const std::vector<uint8_t>& data) {

	std::string id = detail::littleendian2<std::string>(data);
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(id.size() - data.size()) ; trailing-- ;) {
		id.pop_back();
	}
	boost::uuids::uuid uuid = m_uuid_gen(id);

	peer<T>& target = m_peers.at(uuid);
	m_me.bridging_from_to()[uuid].push_back(source);
	m_me.bridging_from_to()[source.id()].push_back(target);


	std::string id_as_str = boost::uuids::to_string(source.id());
	std::deque<uint8_t> d = detail::littleendian2<std::deque<uint8_t>>(std::to_string(source.distance()) + id_as_str);
	d.push_front(static_cast<uint8_t>(d.size() - id_as_str.size() - 1));
	m_manager.send(
			commands::forwarding_to,
			d,
			target
	);

	id_as_str = boost::uuids::to_string(target.id());
	d = detail::littleendian2<std::deque<uint8_t>>(std::to_string(target.distance()) + id_as_str);
	d.push_front(static_cast<uint8_t>(d.size() - id_as_str.size() - 1));
	m_manager.send(
			commands::forwarding_to,
			d,
			source
	);
}

template <typename T>
void breep::network_manager<T>::stop_forwarding_handler(const peer<T>& source, const std::vector<uint8_t>& data) {

	std::string data_str = detail::littleendian2<std::string>(detail::fake_mini_container(data.data() + 1, data.size() - 1));
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(data_str.size() - data.size()) ; trailing-- ;) {
		data_str.pop_back();
	}
	boost::uuids::uuid id = m_uuid_gen(data_str);
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
}

template <typename T>
void breep::network_manager<T>::forwarding_to_handler(const peer<T>& source, const std::vector<uint8_t>& data) {
	std::string str = detail::littleendian2<std::string>(detail::fake_mini_container(data.data() + 1, data.size() - 1));
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(str.size() - data.size()) ; trailing-- ;) {
		str.pop_back();
	}

	unsigned char distance = static_cast<unsigned char>(str[0]);
	peer<T>& target = m_peers.at(m_uuid_gen(str.substr(1)));

	replace(m_me.path_to(target), source);
	replace(m_me.path_to(source), target);
	peer_connected(peer<T>(target), static_cast<unsigned char>(distance + 1));
}

template <typename T>
void breep::network_manager<T>::connect_to_handler(const peer<T>& source, const std::vector<uint8_t>& data) {
	std::vector<uint8_t> ldata = detail::littleendian1(detail::fake_mini_container(data.data() + 1, data.size() - 1));
	for (uint_fast8_t trailing = data[0] + static_cast<uint_fast8_t>(ldata.size() - data.size()) ; trailing-- ;) {
		ldata.pop_back();
	}
	unsigned short port = static_cast<unsigned short>(ldata[0] << 8 | ldata[1]);

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

	peer<T> p(m_manager.connect(boost::asio::ip::address::from_string(buff2), port));

	auto local_data = detail::littleendian2<std::deque<uint8_t>>(buff);
	local_data.push_front(static_cast<uint8_t>(local_data.size() - buff.size()));

	if (p.id() == id) {
		peer_connected(std::move(p), 0);
		m_manager.send(commands::successfully_connected, local_data, source);
	} else {
		m_manager.send(commands::forward_to, local_data, source);
	}
}

template <typename T>
void breep::network_manager<T>::cant_connect_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::successfully_connected_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::update_distance_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::retrieve_distance_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::peers_list_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::new_peer_handler(const peer<T>& /*source*/, const std::vector<uint8_t>& /*data*/) {
	// todo : extract trailing 0
	// todo
}

template <typename T>
void breep::network_manager<T>::peer_disconnection_handler(const peer<T>& source, const std::vector<uint8_t>& data) {
	// todo : extract trailing 0
	// todo
	// todo: check for forwarding and delete that guy everywhere
	forward_if_needed(source, commands::peer_disconnection, data);
}