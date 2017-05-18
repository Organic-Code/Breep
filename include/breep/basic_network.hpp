#ifndef BREEP_BASIC_NETWORK_HPP
#define BREEP_BASIC_NETWORK_HPP

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

#include <algorithm>
#include <sstream>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#include "breep/detail/utils.hpp"
#include "breep/object_builder.hpp"
#include "breep/networking_traits.hpp"
#include "breep/basic_peer.hpp"
#include "breep/basic_peer_manager.hpp"

#include <bitset>
namespace breep {

	namespace detail {
		template <typename T, typename U>
		auto make_obj_pair(std::shared_ptr<object_builder<T,U>> ptr);
	}

	template <typename io_manager>
	class basic_network {
	public:

		using peer = basic_peer<io_manager>;
		using peer_manager = basic_peer_manager<io_manager>;
		using network = basic_network<io_manager>;


		/**
		 * Type representing a connection listener
		 * The function should take \em this instance of \em network<network_manager>, the newly connected peer,
		 * and the distance (number of peers bridging from the \em user to the newly connected peer) as parameters.
		 *
	 	 * @since 0.1.0
		 */
		using connection_listener = std::function<void(network& network, const peer& new_peer)>;


		/**
		 * Type representing a data listener.
		 * The function should take an instance of \em network<network_manager>, the peer that
		 * sent the data, the data itself, and a boolean (set to true if the data was sent to
		 * all the network and false if it was sent only to you) as parameter.
		 *
	 	 * @since 0.1.0
		 */
		template <typename T>
		using data_received_listener = std::function<void(network& network, const network::peer& received_from, const T& data, bool sent_to_all)>;

		/**
		 * Type representing a disconnection listener.
		 * The function should take \em this instance of \em network<network_manager> and the
		 * disconnected peer as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using disconnection_listener = std::function<void(network& network, const peer& disconnected_peer)>;

		/**
		 * Type representing an unlistened type listener.
		 * The type_hash is the hash returned by networking_traits<Unlistened_type>::hash_code()
		 * @since 0.1.0
		 */
		using unlistened_type_listener = std::function<void(network& network, const peer& source, boost::archive::binary_iarchive& data, bool sent_to_all, uint64_t type_hash)>;

		using object_builder_caller = std::function<bool(network& network, const peer& received_from, boost::archive::binary_iarchive& data, bool sent_to_all)>;

		explicit basic_network(unsigned short port_ = peer_manager::default_port)
				: m_manager(port_)
				, m_id_count{}
				, m_co_listeners{}
				, m_dc_listeners{}
				, m_data_listeners{}
				, m_connection_mutex{}
				, m_disconnection_mutex{}
		{
			m_manager.add_connection_listener([this] (peer_manager&, const peer& np) {
				std::lock_guard<std::mutex> lock_guard(m_connection_mutex);
				for (auto& listeners : m_co_listeners) {
					listeners.second(*this, np);
				}
			});
			m_manager.add_disconnection_listener([this] (peer_manager&, const peer& op) {
				std::lock_guard<std::mutex> lock_guard(m_disconnection_mutex);
				for (auto& listeners : m_dc_listeners) {
					listeners.second(*this, op);
				}
			});

			detail::peer_manager_master_listener<io_manager>::set_master_listener(m_manager, [this] (peer_manager&, const peer& src, char* it, size_t size, bool b) -> void {
				this->network_data_listener(src, it, size, b);
			});
		}


		/**
		 * @brief Sends data to all members of the network
		 * @tparam data_container Type representing data. Exact definition
		 *                        is to be defined by \em network_manager::send_to
		 * @param data Data to be sent
		 *
		 * @sa basic_network::send_raw_to(const peer&, const data_container&) const
		 * @sa basic_network::send_object(const Serialiseable&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename data_container>
		void send_raw(const data_container& data) const {
			m_manager.send_to_all(data);
		}

		/**
		 * Sends data to a specific member of the network
		 * @tparam data_container Type representing data. Exact definition
		 *                        is to be defined by \em network_manager_base::send
		 * @param p Target peer
		 * @param data Data to be sent
		 *
		 * @sa basic_network::send_raw(const data_container&) const
		 * @sa basic_network::send_object_to(const Serialiseable&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename data_container>
		void send_raw_to(const peer& p, const data_container& data) const {
			m_manager.send_to(p, data);
		}

		/**
		 * @brief Sends an object to all members of the network
		 *
		 * @sa basic_network::send_object_to(const peer&, const data_container&) const
		 * @sa basic_network::send_raw(const data_container&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename Serialiseable>
//todo: why no const ref (explain)
		void send_object(Serialiseable data) const {
			std::ostringstream oss;
			uint64_t hash_code = networking_traits<Serialiseable>::hash_code();
			uint8_t* hash_8b = reinterpret_cast<uint8_t*>(&hash_code);

			for (uint_fast8_t i{sizeof(uint64_t) / sizeof(uint8_t)} ; i-- ;) {
				oss << *(hash_8b + i);
			}
			boost::archive::binary_oarchive ar(oss, boost::archive::no_header);
			ar << data;
			m_manager.send_to_all(oss.str());
		}

		/**
		 * Sends an object to a specific member of the network
		 *
		 * @param p Target peer
		 *
		 * @sa basic_network::send_object(const data_container&) const
		 * @sa basic_network::send_raw_to(const peer&, const data_container&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename Serialiseable>
//todo: why no const ref (explain)
		void send_object_to(const peer& p, Serialiseable data) const {
			std::ostringstream oss;
			boost::archive::binary_oarchive ar(oss, boost::archive::no_header);
			ar << data;
			m_manager.send_to(p, oss.str());
		}

		/**
		 * Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 */
		void awake() {
			m_manager.run();
		}

		/**
		 * Starts a new network. Same as run(), excepts it is a blocking method.
		 */
		void sync_awake() {
			m_manager.sync_run();
		}

		/**
		 * @brief asynchronically connects to a peer to peer network, given the ip of one peer
		 * @note  it is not possible to be connected to more than one network at the same time.
		 *
		 * @param address Address of a member
		 * @param port Target port. Defaults to the local listening port.
		 *
		 * @throws invalid_state thrown when trying to connect twice to a network.
		 *
		 * @sa network::connect_sync(const boost::asio::ip::address&)
		 *
	 	 * @since 0.1.0
		 */
		void connect(boost::asio::ip::address address, unsigned short port) {
			m_manager.connect(std::move(address), port);
		}

		void connect(const boost::asio::ip::address& address) {
			m_manager.connect(address);
		}

		/**
		 * @brief Similar to \em network::connect(const boost::asio::ip::address&), but blocks until disconnected from all the network or the connection was not successful.
		 * @return true if connection was successful, false otherwise
		 *
		 * @throws invalid_state thrown when trying to connect twice to a network.
		 *
		 * @sa network::connect(const boost::asio::ip::address& address)
		 *
		 * @since 0.1.0
		 */
		bool sync_connect(const boost::asio::ip::address& address, unsigned short port) {
			return m_manager.sync_connect(address, port);
		}

		bool sync_connect(const boost::asio::ip::address& address) {
			return m_manager.sync_connect(address);
		}

		/**
		 * @brief disconnects from the network
		 *
		 * @sa network::disconnect_sync()
		 *
	 	 * @since 0.1.0
	 	 */
		void disconnect() {
			m_manager.disconnect();
		}

		/**
		 * @brief Adds a listener for incoming connections
		 * @details Each time a new peer connects to the network,
		 *          the method passed as a parameter is called, with
		 *          parameters specified in \em network::connection_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa network::connection_listener
		 * @sa network::remove_connection_listener(listener_id)
		 *
	 	 * @since 0.1.0
		 */
		listener_id add_connection_listener(connection_listener listener) {
			m_co_listeners.emplace(m_id_count, listener);
			return m_id_count++;
		}

		/**
		 * @brief Adds a listener for disconnections
		 * @details Each time a peer sends you data, the method
		 *          passed as a parameter is called, with parameters
		 *          specified in \em network::disconnection_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa network::data_received_listener
		 * @sa network::remove_disconnection_listener(listener_id)
		 *
		 * @since 0.1.0
		 */
		listener_id add_disconnection_listener(disconnection_listener listener) {
			m_dc_listeners.emplace(m_id_count, listener);
			return m_id_count++;
		}

		/**
		 * @brief Removes a listener
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @since 0.1.0
		 */
		bool remove_connection_listener(listener_id id) {
			return m_co_listeners.erase(id) > 0;
		}

		/**
		 * @brief Removes a listener
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @since 0.1.0
		 */
		bool remove_disconnection_listener(listener_id id) {
			return m_dc_listeners.erase(id) > 0;
		}

		/**
		 * @return The list of connected peers
		 *
		 * @details Returned by copy to avoid data race.
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, peer, boost::hash<boost::uuids::uuid>> peers() const {
			return m_manager.peers();
		}

		/**
		 * @return The port to which the object is currently mapped to.
		 *
		 * @since 0.1.0
		 */
		unsigned short port() const {
			return m_manager.port();
		}

		/**
		 * @brief Changes the port to which the object is mapped
		 * @param port the new port
		 * @attention If the port is changed while there are ongoing connections, breep::invalid_state exception is raised.
		 */
		void port(unsigned short port) {
			m_manager.port(port);
		}

		const local_peer<io_manager>& self() const {
			return m_manager.self();
		}

		template <typename T>
		listener_id add_data_listener(data_received_listener<T> listener) {

			auto associated_listener = m_data_listeners.find(networking_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				std::shared_ptr<detail::object_builder<io_manager,T>> builder_ptr = std::make_shared<detail::object_builder<io_manager, T>>();
				m_data_listeners.emplace(networking_traits<T>::hash_code(), detail::make_obj_pair(builder_ptr));

				return builder_ptr->add_listener(m_id_count++, listener);
			} else {
				auto builder_ptr = detail::any_cast<detail::object_builder<io_manager,T>*>(associated_listener->second.second);
				return builder_ptr->add_listener(m_id_count++, listener);
			}
		}



		template <typename T>
		bool remove_data_listener(listener_id id) {
			auto associated_listener = m_data_listeners.find(networking_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				return false;
			} else {
				auto builder = detail::any_cast<detail::object_builder<io_manager,T>*>(associated_listener->second.second);
				return builder->remove_listener(id);
			}
		}

		void set_unlistened_type_listener(unlistened_type_listener listener) {
			m_unlistened_listener = listener;
		}

	private:

		void network_data_listener(const peer& source, char* data, size_t data_size, bool sent_to_all) {
			uint64_t hash_code{0};
			for (int i = {sizeof(uint64_t) / sizeof(uint8_t)} ; i--;) {
				hash_code = (hash_code << 8) | static_cast<unsigned char>(*data++);
			}

			boost::iostreams::stream_buffer<boost::iostreams::basic_array_source<char>> buffer(data, data_size - 4);
			boost::archive::binary_iarchive archive(buffer, boost::archive::no_header);

			auto associated_listener = m_data_listeners.find(hash_code);
			if (associated_listener != m_data_listeners.end()) {
				if (!associated_listener->second.first(*this, source, archive, sent_to_all)) {
					m_unlistened_listener(*this, source, archive, sent_to_all, hash_code);
				}
			} else if (m_unlistened_listener){
				m_unlistened_listener(*this, source, archive, sent_to_all, hash_code);
			}
		}

		peer_manager m_manager;

		listener_id m_id_count;

		unlistened_type_listener m_unlistened_listener;

		std::unordered_map<listener_id, connection_listener> m_co_listeners;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listeners;
		std::unordered_map<uint64_t, std::pair<object_builder_caller, detail::any>> m_data_listeners;

		std::mutex m_connection_mutex;
		std::mutex m_disconnection_mutex;
	};

	template <typename T, typename U>
	auto detail::make_obj_pair(std::shared_ptr<object_builder<T,U>> ptr) {
		return std::make_pair<typename basic_network<T>::object_builder_caller, detail::any>(
				[ptr](basic_network<T>& net, const typename basic_network<T>::peer& p, boost::archive::binary_iarchive& ar, bool sta) -> bool {
					return ptr->build_and_call(net, p, ar, sta);
				}
				, ptr.get()
		);
	};
}


#endif //BREEP_BASIC_NETWORK_HPP
