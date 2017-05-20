#ifndef BREEP_NETWORK_BASIC_NETWORK_HPP
#define BREEP_NETWORK_BASIC_NETWORK_HPP

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

#include <sstream>
#include <utility>
#include <tuple>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

#include "breep/network/typedefs.hpp"
#include "breep/network/detail/utils.hpp"
#include "breep/network/detail/object_builder.hpp"
#include "breep/util/type_traits.hpp"
#include "basic_peer.hpp"
#include "basic_peer_manager.hpp"

namespace breep {

	namespace detail {
		template <typename T, typename U>
		std::tuple<typename basic_network<T>::object_builder_caller, typename basic_network<T>::object_builder_log_level, detail::any>
		make_obj_tuple(std::shared_ptr<object_builder<T, U>> ptr);
	}

	/**
	 * @class basic_network basic_network.hpp
	 * @brief A advanced basic_peer_manager.
	 * @detail This class represents and manages the network. It is an advanced version of the basic_peer_manager that
	 *                         allows you to send instanciated objects directly and to register listeners for a specific type.
	 * @tparam io_manager      Manager used to manage input and ouput operations of (including connection & disconection) the network
	 *                         This class should inherit from \em breep::network_manager_base
	 *                                see \em breep::tcp_nmanager and \em breep::udp_nmanager for examples of implementation.
	 *                                network_manager::socket_type must also be defined.
	 *
	 * @note A \em const \em basic_network is a basic_network with whom you can only send data,
	 *       and you can't proceed to a connection / disconnection.
	 *
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class basic_network {
	public:

		using inner_io_manager = io_manager;
		using peer = basic_peer<io_manager>;
		using peer_manager = basic_peer_manager<io_manager>;
		using network = basic_network<io_manager>;


		/**
		 * Type representing a connection listener
		 * The function should take \em this instance of \em basic_network<io_manager>
		 * and the newly connected peer as parameters.
		 *
		 * @note connection_listeners are also eligible to be disconnection_listeners
		 *
		 * @sa data_received_listener
		 * @sa disconnection_listener
		 * @sa unlistened_type_listener
		 *
	 	 * @since 0.1.0
		 */
		using connection_listener = std::function<void(network& network, const peer& new_peer)>;


		/**
		 * Type representing a data listener.
		 * The function should take an instance of \em basic_network<io_manager>, the peer that
		 * sent the data, the data itself, and a boolean (set to true if the data was sent only
		 * to true and set to false if the data was sent to all the network) as parameter.
		 *
		 * @note for now, you may take a T, const T, const T& or T& type as parameter, but this may be removed in the future (letting only const T&)
		 *
		 * @sa connection_listener
		 * @sa disconnection_listener
		 * @sa unlistened_type_listener
		 *
	 	 * @since 0.1.0
		 */
		template <typename T>
		using data_received_listener = std::function<void(network& network, const network::peer& received_from, const T& data, bool is_private)>;

		/**
		 * Type representing a disconnection listener.
		 * The function should take \em this instance of \em network<io_manager> and the
		 * disconnected peer as parameter.
		 *
		 * @note disconnection_listeners are also eligible to be connection_listeners
		 *
		 * @sa data_received_listener
		 * @sa connection_listener
		 * @sa unlistened_type_listener
	 	 * @since 0.1.0
		 */
		using disconnection_listener = std::function<void(network& network, const peer& disconnected_peer)>;

		/**
		 * Type representing an unlistened type listener.
		 * It may be used to set a listener that will get called whenever \em this instance receives a type that is not listened for
		 *
		 * @note The type_hash is the hash returned by networking_traits<Unlistened_type>::hash_code()
		 *
		 * @since 0.1.0
		 */
		using unlistened_type_listener = std::function<void(network& network, const peer& source, boost::archive::binary_iarchive& data, bool sent_to_all, uint64_t type_hash)>;

		/**
		 * internally used.
		 */
		using object_builder_caller = std::function<bool(network& network, const peer& received_from, boost::archive::binary_iarchive& data, bool sent_to_all)>;
		using object_builder_log_level = std::function<void(log_level ll)>;

		/**
		 * @since 0.1.0
		 */
		explicit basic_network(unsigned short port_ = peer_manager::default_port)
				: m_manager(port_)
				, m_id_count{}
				, m_unlistened_listener{}
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
		 * @brief Sends an object to all members of the network
		 *
		 * @tparam Serialiseable  Serialisable must meet the requirements of boost serialization (for now)
		 *
		 * @sa basic_network::send_object_to(const peer&, const data_container&) const
		 * @sa basic_network::send_raw(const data_container&) const
		 *
		 * @note datas are passed by copy and not by const reference because of the way boost serialization works.
		 * 				It might change to a const ref on future updates.
		 *
	 	 * @since 0.1.0
		 */
		template <typename Serialiseable>
		void send_object(Serialiseable data) const {
			std::ostringstream oss;
			uint64_t hash_code = type_traits<Serialiseable>::hash_code();
			uint8_t* hash_8b = reinterpret_cast<uint8_t*>(&hash_code);

			for (uint_fast8_t i{sizeof(uint64_t) / sizeof(uint8_t)} ; i-- ;) {
				oss << *(hash_8b + i);
			}
			boost::archive::binary_oarchive ar(oss, boost::archive::no_header);
			ar << data;
			breep::logger<network>.info("Sending " + type_traits<Serialiseable>::universal_name());
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
		 * @note datas are passed by copy and not by const reference because of the way boost serialization works.
		 * 				It might change to a const ref on future updates.
		 *
	 	 * @since 0.1.0
		 */
		template <typename Serialiseable>
		void send_object_to(const peer& p, Serialiseable data) const {
			std::ostringstream oss;
			uint64_t hash_code = type_traits<Serialiseable>::hash_code();
			uint8_t* hash_8b = reinterpret_cast<uint8_t*>(&hash_code);

			for (uint_fast8_t i{sizeof(uint64_t) / sizeof(uint8_t)} ; i-- ;) {
				oss << *(hash_8b + i);
			}
			boost::archive::binary_oarchive ar(oss, boost::archive::no_header);
			ar << data;
			breep::logger<network>.info("Sending private " + type_traits<Serialiseable>::universal_name() + " to " + p.id_as_string());
			m_manager.send_to(p, oss.str());
		}

		/**
		 * Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 */
		void awake() {
			m_manager.run();
		}

		/**
		 * Starts a new network. Same as run(), excepts it a will blocks until the network is closed.
		 */
		void sync_awake() {
			m_manager.sync_run();
		}

		/**
		 * @brief asynchronically connects to a peer to peer network, given the ip of one peer
		 * @note it is not possible to be connected to more than one network at the same time.
		 * @note Calling this method will trigger \em this instance awakening (if the connection was successful).
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
		 * @return An id that may be used to remove the listener
		 *
		 * @sa network::connection_listener
		 * @sa network::remove_connection_listener(listener_id)
		 * @sa network::add_disconnection_listener(disconnection_listener)
		 * @sa network::add_data_listener(data_listener<T>)
		 *
	 	 * @since 0.1.0
		 */
		listener_id add_connection_listener(connection_listener listener) {
			std::lock_guard<std::mutex> lock_guard(m_connection_mutex);
			breep::logger<network>.debug("Adding connection listener (id: " + std::to_string(m_id_count) + ")");
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
		 * @sa network::remove_disconnection_listener(listener_id)
		 * @sa network::add_connection_listener(connection_listener)
		 * @sa network::remove_disconnection_listener(listener_id)
		 * @sa network::add_data_listener(data_listener<T>)
		 *
		 * @since 0.1.0
		 */
		listener_id add_disconnection_listener(disconnection_listener listener) {
			std::lock_guard<std::mutex> lock_guard(m_disconnection_mutex);
			breep::logger<network>.debug("Adding disconnection listener (id: " + std::to_string(m_id_count) + ")");
			m_dc_listeners.emplace(m_id_count, listener);
			return m_id_count++;
		}

		/**
		 * @brief Removes a listener
		 * @details Stops the listener from being called
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @attention Results in a dead lock if called from a connection listener, from the network's thread
		 *
		 * @since 0.1.0
		 */
		bool remove_connection_listener(listener_id id) {
			std::lock_guard<std::mutex> lock_guard(m_connection_mutex);
			breep::logger<network>.debug("Removing connection listener (id: " + std::to_string(m_id_count) + ")");
			return m_co_listeners.erase(id) > 0;
		}

		/**
		 * @brief Removes a listener
		 * @details Stops the listener from being called
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @attention Results in a dead lock if called from a connection listener, from the network's thread
		 *
		 * @since 0.1.0
		 */
		bool remove_disconnection_listener(listener_id id) {
			std::lock_guard<std::mutex> lock_guard(m_disconnection_mutex);
			breep::logger<network>.debug("Removing disconnection listener (id: " + std::to_string(m_id_count) + ")");
			return m_dc_listeners.erase(id) > 0;
		}

		/**
		 * @return The list of connected peers (you excluded)
		 *
		 * @attention Accessing a value from this map from a thread other that the network's thread (accessible through listeners) is NOT safe.
		 *
		 * @since 0.1.0
		 */
		const std::unordered_map<boost::uuids::uuid, peer, boost::hash<boost::uuids::uuid>>& peers() const {
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
		 * @attention If the port is changed while the network is awake, breep::invalid_state exception is raised.
		 */
		void port(unsigned short port) {
			m_manager.port(port);
		}

		/**
		 * @return A peer representing the local instance on the global network
		 */
		const local_peer<io_manager>& self() const {
			return m_manager.self();
		}

		/**
		 * @brief Adds a data listener
		 *
		 * @tparam T type you want to listen
		 * @param listener Listener to add to the listeners list
		 * @return An id that may be used to remove the listener
		 *
		 * @sa network::data_received_listener
		 * @sa network::remove_data_listener(listener_id)
		 * @sa network::add_disconnection_listener(disconnection_listener)
		 * @sa network::add_connection_listener(connection_listener)
		 */
		template <typename T>
		listener_id add_data_listener(data_received_listener<T> listener) {

			auto associated_listener = m_data_listeners.find(type_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				breep::logger<network>.debug("New type being registered for listening: " + type_traits<T>::universal_name());
				std::shared_ptr<detail::object_builder<io_manager,T>> builder_ptr = std::make_shared<detail::object_builder<io_manager, T>>();
				m_data_listeners.emplace(type_traits<T>::hash_code(), detail::make_obj_tuple(builder_ptr));
				builder_ptr->set_log_level(logger<network>.level());
				return builder_ptr->add_listener(m_id_count++, listener);
			} else {
				auto builder_ptr = detail::any_cast<detail::object_builder<io_manager,T>*>(std::get<2>(associated_listener->second));
				return builder_ptr->add_listener(m_id_count++, listener);
			}
		}

		/**
		 * @brief Removes a listener
		 * @details Stops the listener from being called
		 *
		 * @tparam T Type listened by the concerned listener
		 * @param id ID of the concerned listener
		 * @return true if a listener was removed, false otherwise
		 *
		 * @note contrary to remove_connection_listener and remove_disconnetion_listener,
		 *       does NOT result in a dead lock if called from a data_listener in the
		 *       network's thread.
		 */
		template <typename T>
		bool remove_data_listener(listener_id id) {
			auto associated_listener = m_data_listeners.find(type_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				breep::logger<network>.warning
						("Trying to remove a listener of type " + type_traits<T>::universal_name() + " that was not registered. (listener id: " + std::to_string(id) + ")");
				return false;
			} else {
				auto builder = detail::any_cast<detail::object_builder<io_manager,T>*>(std::get<2>(associated_listener->second));
				return builder->remove_listener(id);
			}
		}

		/**
		 * @brief Sets the listener for unlistened types.
		 * @sa unlistened_type_listener
		 */
		void set_unlistened_type_listener(unlistened_type_listener listener) {
			m_unlistened_listener = listener;
		}

		/**
		 * @brief sets the logging level
		 */
		void set_log_level(log_level ll) const {
			breep::logger<network>.level(ll);
			m_manager.set_log_level(ll);
			for (auto& pair : m_data_listeners) {
				std::get<1>(pair.second)(ll);
			}
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
				if (!std::get<0>(associated_listener->second)(*this, source, archive, sent_to_all) && m_unlistened_listener) {
					breep::logger<network>.trace("calling default listener.");
					m_unlistened_listener(*this, source, archive, sent_to_all, hash_code);
				}
			} else if (m_unlistened_listener){
				breep::logger<network>.warning("Unregistered type received: " + std::to_string(hash_code) + ". Calling default listener.");
				m_unlistened_listener(*this, source, archive, sent_to_all, hash_code);
			} else {
				breep::logger<network>.warning("Unregistered type received: " + std::to_string(hash_code));
			}
		}

		peer_manager m_manager;

		listener_id m_id_count;

		unlistened_type_listener m_unlistened_listener;

		std::unordered_map<listener_id, connection_listener> m_co_listeners;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listeners;
		std::unordered_map<uint64_t, std::tuple<object_builder_caller, object_builder_log_level, detail::any>> m_data_listeners;

		std::mutex m_connection_mutex;
		std::mutex m_disconnection_mutex;
	};

	template <typename T, typename U>
	std::tuple<typename basic_network<T>::object_builder_caller, typename basic_network<T>::object_builder_log_level, detail::any>
	detail::make_obj_tuple(std::shared_ptr<object_builder<T, U>> ptr) {
		return std::make_tuple<typename basic_network<T>::object_builder_caller, typename basic_network<T>::object_builder_log_level, detail::any>(
				[ptr](basic_network<T>& net, const typename basic_network<T>::peer& p, boost::archive::binary_iarchive& ar, bool sta) -> bool {
					return ptr->build_and_call(net, p, ar, sta);
				},
		        [obj_ptr = ptr.get()] (log_level ll) -> void {
					obj_ptr->set_log_level(ll);
				},
				ptr.get());
	}
}
BREEP_DECLARE_TEMPLATE(breep::basic_network);

#endif //BREEP_NETWORK_BASIC_NETWORK_HPP
