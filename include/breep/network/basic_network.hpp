#ifndef BREEP_NETWORK_BASIC_NETWORK_HPP
#define BREEP_NETWORK_BASIC_NETWORK_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
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

#include "breep/util/serializer.hpp"
#include "breep/util/deserializer.hpp"
#include "breep/util/type_traits.hpp"
#include "breep/network/typedefs.hpp"
#include "breep/network/detail/utils.hpp"
#include "breep/network/detail/object_builder.hpp"
#include "breep/network/basic_netdata_wrapper.hpp"
#include "breep/network/basic_peer.hpp"
#include "breep/network/basic_peer_manager.hpp"
#include "breep/network/packet.hpp"

namespace breep {

	namespace detail {
		namespace network_cst {
			constexpr int object_builder_idx = 0;
			constexpr int caller = 1;
			constexpr int log_setter = 2;
			constexpr int listener_rmer = 3;
		}
	}

	namespace detail {
		template <typename T, typename U>
		std::tuple<detail::any, typename basic_network<T>::ob_caller, typename basic_network<T>::ob_log_level, typename basic_network<T>::ob_rm_listener>
		make_obj_tuple(std::shared_ptr<object_builder<T, U>> ptr);
	}


	/**
	 * @class basic_network basic_network.hpp
	 * @brief A advanced basic_peer_manager.
	 * @details This class represents and manages the network. It is an advanced version of the basic_peer_manager that
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

		/**
		 * convenience typedefs
		 * @since 0.1.0
		 */
		using inner_io_manager = io_manager;
		using peer = basic_peer<io_manager>;
		using peer_manager = basic_peer_manager<io_manager>;
		using network = basic_network<io_manager>;

		template <typename T>
		using netdata_wrapper = basic_netdata_wrapper<io_manager, T>;


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
		 * This function should take a netdata_wrapper as parameter (by reference).
		 * A netdata_wrappers contains:
		 *        \em this instance of basic_network<io_manager> (::network)
		 *        The peer that sent the class (::source)
		 *        The data itself (::data)
		 *        A boolean indicating if the data was sent only to you or not (::is_private)
		 *        The currently called listener id (::listener_id)
		 *
		 * @since 0.1.0
		 */
		template <typename T>
		using data_received_listener = std::function<void(netdata_wrapper<T>& data)>;

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
		using unlistened_type_listener = std::function<void(network& network, const peer& source, breep::deserializer& data, bool sent_to_all, uint64_t type_hash)>;

		/**
		 * internally used.
		 */
		using ob_caller = std::function<bool(network& network, const peer& received_from, breep::deserializer& data, bool sent_to_all)>;
		using ob_rm_listener = std::function<bool(listener_id)>;
		using ob_log_level = std::function<void(log_level ll)>;

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
				, m_object_builder_mutex{}
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

			m_manager.add_data_listener([this] (peer_manager&, const peer& src, cuint8_random_iterator it, size_t size, bool b) -> void {
				this->network_data_listener(src, it, size, b);
			});
		}

		/**
		 * @brief Sends an object to all members of the network
		 *
		 * @tparam Serialiseable  Serialisable must meet the requirements of boost serialization (for now)
		 *
		 * @sa basic_network::send_object_to(const peer&, const Serialiseable&) const
		 * @sa basic_network::send_packet(const packet& pack)
		 *
		 * @until 1.0.0: void send_object(Serialiseable);
	 	 * @since 1.0.0: void send_object(const Serialiseable&);
		 */
		template <typename Serialiseable>
		void send_object(const Serialiseable& data) const {
			breep::serializer s;
			s << type_traits<Serialiseable>::hash_code();
			s << data;
			breep::logger<network>.debug("Sending " + type_traits<Serialiseable>::universal_name());
			m_manager.send_to_all(s.str());
		}

		/**
		 * @brief Sends an object to a specific member of the network
		 *
		 * @param p Target peer
		 *
		 * @sa basic_network::send_object(const Serialiseable&) const
		 * @sa basic_network::send_object_to_self(const T& data) const
		 * @sa basic_network::send_packet_to(const peer&, const packet&) const
		 *
		 * @until 1.0.0: void send_object_to(const peer&, Serialiseable);
	 	 * @since 1.0.0: void send_object_to(const peer&, const Serialiseable&);
		 */
		template <typename Serialiseable>
		void send_object_to(const peer& p, const Serialiseable& data) const {

			breep::serializer s;
			s << type_traits<Serialiseable>::hash_code();
			s << data;
			breep::logger<network>.debug("Sending private " + type_traits<Serialiseable>::universal_name() + " to " + p.id_as_string());
			m_manager.send_to(p, s.str());
		}

		/**
		 * @brief Calls listeners as if some data was received from the network, but originating from localhost
		 * @details Does NOT call the default listener if the type is not listened to.
		 *
		 * @param data         data to be sent
		 * @param is_private   should the 'private' flag be set or not ? (false by default)
		 *
		 * @since 1.0.0
		 */
		template <typename T>
		void send_object_to_self(const T& data, bool is_private = false) {
			std::lock_guard<std::mutex> lock_guard(m_object_builder_mutex);

			auto associated_listener = m_data_listeners.find(breep::type_traits<T>::hash_code());
			if (associated_listener != m_data_listeners.end()) {
				auto ob = detail::any_cast<detail::object_builder<io_manager, T>*>(std::get<detail::network_cst::object_builder_idx>(associated_listener->second));
				breep::logger<network>.debug("Self sending " + breep::type_traits<T>::universal_name() + ".");
				ob->flush_listeners();
				ob->fire(breep::basic_netdata_wrapper<io_manager, T>(*this, m_manager.self(), data, is_private));
			} else {
				breep::logger<network>.warning("Unregistered type self-sent: " + breep::type_traits<T>::universal_name());
			}
		}

		/**
		 * @brief Sends a packet containing 0 or more classes
		 * @param pack The packet to be sent.
		 *
		 * @sa basic_network::send_packet_to(const peer&, const packet&)
		 * @sa basic_network::send_object(const Serialiseable&)
		 *
		 * @since 1.0.0
		 */
		void send_packet(const packet& pack) const {
			breep::logger<network>.debug("Sending a packet");
			m_manager.send_to_all(pack.m_s.str());
		}

		/**
		 * @brief Sends a packet containing 0 or more classes
		 * @param pack The packet to be sent.
		 * @param target Target peer
		 *
		 * @sa basic_network::send_packet(const packet&)
		 * @sa basic_network::send_object_to(const peer& const Serialiseable&)
		 *
		 * @since 1.0.0
		 */
		void send_packet_to(const peer& target, const packet& pack) const {
			breep::logger<network>.debug("Sending a private packet");
			m_manager.send_to(target, pack.m_s.str());
		}

		/**
		 * @brief Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 *
		 * @attention if the network was previously awekened, shot down, and that ::awaken() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @throws invalid_state if the peer_manager is already running.
		 *
		 * @since 0.1.0
		 */
		void awake() {
			m_manager.run();
		}

		/**
		 * @brief Starts a new network. Same as run(), excepts it a will blocks until the network is closed.
		 *
		 * @since 0.1.0
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
		 * @return true if the connection was successful, false otherwise.
		 *
		 * @attention if the network was previously awekened, shot down, and that ::connect(...) is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @throws invalid_state if the peer_manager is already running.
		 *
		 * @sa network::connect_sync(const boost::asio::ip::address&)
		 *
	 	 * @since 0.1.0
		 */
		bool connect(boost::asio::ip::address address, unsigned short port) {
			return m_manager.connect(std::move(address), port);
		}

		/**
		 * @since 0.1.0
		 */
		bool connect(const boost::asio::ip::address& address) {
			return m_manager.connect(address);
		}

		/**
		 * @brief Similar to \em network::connect(const boost::asio::ip::address&), but blocks until disconnected from all the network or the connection was not successful.
		 * @return true if connection was successful, false otherwise
		 *
		 * @throws invalid_state thrown when trying to connect twice to a network.
		 *
		 * @attention if the network was previously started, shot down, and that ::sync_connect() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @sa network::connect(const boost::asio::ip::address& address)
		 *
		 * @since 0.1.0
		 */
		bool sync_connect(const boost::asio::ip::address& address, unsigned short port) {
			return m_manager.sync_connect(address, port);
		}

		/**
		 * @since 0.1.0
		 */
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
		 * @return true if the network is launched, false otherwise
		 *
		 * @since 1.0.0
		 */
		bool is_running() const {
			return m_manager.is_running();
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
		 *
		 * @since 0.1.0
		 */
		void port(unsigned short port) {
			m_manager.port(port);
		}

		/**
		 * @return A peer representing the local instance on the global network
		 *
		 * @since 0.1.0
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
		 *
		 * @since 0.1.0
		 */
		template <typename T>
		type_listener_id add_data_listener(data_received_listener<T> listener) {

			auto associated_listener = m_data_listeners.find(type_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				std::lock_guard<std::mutex> lock_guard(m_object_builder_mutex);
				breep::logger<network>.debug("New type being registered for listening: " + type_traits<T>::universal_name());
				std::shared_ptr<detail::object_builder<io_manager,T>> builder_ptr = std::make_shared<detail::object_builder<io_manager, T>>();
				m_data_listeners.emplace(type_traits<T>::hash_code(), detail::make_obj_tuple(builder_ptr));
				builder_ptr->set_log_level(logger<network>.level());
				return builder_ptr->add_listener(m_id_count++, listener);
			} else {
				auto builder_ptr = detail::any_cast<detail::object_builder<io_manager,T>*>(std::get<detail::network_cst::object_builder_idx>(associated_listener->second));
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
		 *
		 * @since 0.1.0
		 */
		template <typename T>
		bool remove_data_listener(listener_id id) {
			auto associated_listener = m_data_listeners.find(type_traits<T>::hash_code());
			if (associated_listener == m_data_listeners.end()) {
				breep::logger<network>.warning
						("Trying to remove a listener of type " + type_traits<T>::universal_name() + " that was not registered. (listener id: " + std::to_string(id) + ")");
				return false;
			} else {
				return std::get<detail::network_cst::listener_rmer>(associated_listener->second)(id);
			}
		}

		/**
		 * @brief Removes a listener
		 * @details Stops the listener from being called
		 *
		 * @param id type_listener_id of the concerned listener
		 * @return true if a listener was removed, false otherwise
		 *
		 * @note Unlike ::remove_data_listener(listener_id), this method cannot print the class' name in case of warning.
		 *       Apart from that, there is no difference.
		 *
		 * @note contrary to remove_connection_listener and remove_disconnetion_listener,
		 *       does NOT result in a dead lock if called from a data_listener in the
		 *       network's thread.
		 *
		 * @since 0.1.0
		 */
		bool remove_data_listener(const type_listener_id& id) {
			auto associated_listener = m_data_listeners.find(id.type_hash());
			if (associated_listener == m_data_listeners.end()) {
				breep::logger<network>.warning
						("Trying to remove a listener for an unknown type (hash: " + std::to_string(id.type_hash())
						 + ") that was not registered. (listener id: " + std::to_string(id.id()) + ")");
				return false;
			} else {
				return std::get<detail::network_cst::listener_rmer>(associated_listener->second)(id.id());
			}
		}

		/**
		 * @brief Sets the listener for unlistened types.
		 * @sa unlistened_type_listener
		 *
		 * @since 0.1.0
		 */
		void set_unlistened_type_listener(unlistened_type_listener listener) {
			m_unlistened_listener = listener;
		}

		/**
		 * @brief sets the logging level
		 *
		 * @since 0.1.0
		 */
		void set_log_level(log_level ll) const {
			breep::logger<network>.level(ll);
			m_manager.set_log_level(ll);
			for (auto& pair : m_data_listeners) {
				std::get<detail::network_cst::log_setter>(pair.second)(ll);
			}
		}

		/**
		 * @brief removes all listeners of the given type
		 *
		 * @since 0.1.0
		 */
		template <typename T>
		void clear_all() {
			auto object_builder = m_data_listeners.find(type_traits<T>::hash_code());
			if (object_builder != m_data_listeners.end()) {
				detail::any_cast<detail::object_builder<io_manager,T>*>(std::get<detail::network_cst::object_builder_idx>(object_builder->second))->clear_any();
			} else {
				breep::logger<network>.warning("Trying to clean an listener list for a type that was not registered (" + type_traits<T>::universal_name() + ")");
			}
		}

		/**
		 * @brief removes any data/connection/disconnection listeners
		 * @details Also removes all object builders. As a side effect, all types are considered to never have been
		 *         registered.
		 *
		 * @since 0.1.0
		 */
		void clear_any() {
			breep::logger<network>.debug("Cleaning any listeners");

			m_connection_mutex.lock();
			m_co_listeners.clear();
			m_connection_mutex.unlock();
			m_disconnection_mutex.lock();
			m_dc_listeners.clear();
			m_disconnection_mutex.unlock();
			m_data_listeners.clear();
		}

		/**
		 * @brief Wait until the network stopped
		 * @details If the network is not launched, retunrs immediately
		 *
		 * @since 0.1.0
		 */
		void join() {
			m_manager.join();
		}

		/**
		 * @brief Sets the predicate called on incomming connections
		 *
		 * @details the predicate is called with the new peer candidate as parameter. If it returns true,
		 *          the peer is accepted and marked as connected. Otherwise it is disconnected.
		 *
		 * @param pred should be noexcept
		 *
		 * @note You should not attempt to communicate with the peer before it is marked as connected (ie: when
		 *       the connection handler is called)
		 *
		 * @since 1.0.0
		 *
		 * @sa basic_network::remove_connection_predicate()
		 */
		void set_connection_predicate(std::function<bool(const peer&)> pred) {
			m_manager.set_connection_predicate(pred);
		}

		/**
		 * @brief removes previously set connection predicate and goes back to default: accepting any connection
		 *
		 * @since 1.0.0
		 *
		 * @sa basic_network::set_connection_predicate(std::function<bool(const peer&)> pred)
		 */
		void remove_connection_predicate() {
			m_manager.remove_connection_predicate();
		}

	private:

		void network_data_listener(const peer& source, cuint8_random_iterator data, size_t data_size, bool sent_to_all) {
			uint64_t hash_code{0};
			for (int i = {sizeof(uint64_t)} ; i--;) {
				hash_code = (hash_code << 8) | static_cast<unsigned char>(*data++);
			}

			breep::deserializer d(std::basic_string<uint8_t>(data, data + data_size - sizeof(uint64_t)));
			if (hash_code == type_traits<packet>::hash_code()) {
				breep::logger<network>.trace("Received a packet. Unwrapping it.");
				while(!d.empty()) {
					d >> hash_code;
					class_received(hash_code, source, d, sent_to_all);
				}
			} else {
				class_received(hash_code, source, d, sent_to_all);
			}
		}

		void class_received(uint64_t hash_code, const peer& source, breep::deserializer& d, bool sent_to_all) {
			m_object_builder_mutex.lock();
			auto associated_listener = m_data_listeners.find(hash_code);
			if (associated_listener != m_data_listeners.end()) {
				if (!std::get<detail::network_cst::caller>(associated_listener->second)(*this, source, d, sent_to_all) && m_unlistened_listener) {
					breep::logger<network>.warning("Calling default listener.");
					m_unlistened_listener(*this, source, d, sent_to_all, hash_code);
				}
				m_object_builder_mutex.unlock();
			} else if (m_unlistened_listener){
				m_object_builder_mutex.unlock();
				breep::logger<network>.warning("Unregistered type received: " + std::to_string(hash_code) + ". Calling default listener.");

				try {
					m_unlistened_listener(*this, source, d, sent_to_all, hash_code);

				} catch (const std::exception& e) {
					breep::logger<network>.warning("Exception thrown while calling default data listener");
					breep::logger<network>.warning(e.what());
				} catch (const std::exception* e) {
					breep::logger<network>.warning("Exception thrown while calling default data listener");
					breep::logger<network>.warning(e->what());
					delete e;
				}

			} else {
				m_object_builder_mutex.unlock();
				breep::logger<network>.warning("Unregistered type received: " + std::to_string(hash_code));
			}
		}

		peer_manager m_manager;

		listener_id m_id_count;

		unlistened_type_listener m_unlistened_listener;

		std::unordered_map<listener_id, connection_listener> m_co_listeners;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listeners;
		std::unordered_map<uint64_t, std::tuple<detail::any, ob_caller, ob_log_level, ob_rm_listener>> m_data_listeners;

		std::mutex m_connection_mutex;
		std::mutex m_disconnection_mutex;
		std::mutex m_object_builder_mutex;
	};

	template <typename T, typename U>
	std::tuple<detail::any, typename basic_network<T>::ob_caller, typename basic_network<T>::ob_log_level, typename basic_network<T>::ob_rm_listener>
	detail::make_obj_tuple(std::shared_ptr<object_builder<T, U>> ptr) {
		auto obj_ptr = ptr.get();
		return std::make_tuple<detail::any, typename basic_network<T>::ob_caller, typename basic_network<T>::ob_log_level, typename basic_network<T>::ob_rm_listener>(
				obj_ptr,
				[local = std::move(ptr)](basic_network<T>& net, const typename basic_network<T>::peer& p, breep::deserializer& ar, bool sta) -> bool {
					return local->build_and_call(net, p, ar, sta);
				},
		        [obj_ptr] (log_level ll) -> void {
					obj_ptr->set_log_level(ll);
				},
		        [obj_ptr] (listener_id id) -> bool {
			        return obj_ptr->remove_listener(id);
		        }
		);
	}
} // namespace breep

BREEP_DECLARE_TEMPLATE(breep::basic_network)

#endif //BREEP_NETWORK_BASIC_NETWORK_HPP
