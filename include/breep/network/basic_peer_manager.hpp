#ifndef BREEP_NETWORK_BASIC_PEER_MANAGER_HPP
#define BREEP_NETWORK_BASIC_PEER_MANAGER_HPP

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
 * @file network.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */

#include <atomic>
#include <utility>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include <boost/functional/hash.hpp>

#include "breep/util/exceptions.hpp"
#include "breep/util/type_traits.hpp"
#include "breep/util/logger.hpp"
#include "breep/network/typedefs.hpp"
#include "breep/network/detail/commands.hpp"
#include "breep/network/basic_peer.hpp"
#include "breep/network/local_peer.hpp"
#include "breep/network/io_manager_base.hpp"

namespace breep {

	namespace detail {
		template<typename T>
		class peer_manager_attorney;
	}

	/**
	 * @class basic_peer_manager basic_peer_manager.hpp
	 * @brief                  This class is used to manage basic interactions with peers.
	 * @tparam io_manager      Manager used to manage input and ouput operations (including connection & disconection) for the network
	 *                         This class should inherit from \em breep::network_manager_base
	 *                                see \em breep::tcp_nmanager and \em breep::udp_nmanager for examples of implementation.
	 *                                network_manager::socket_type must also be defined.
	 *
	 * @note A \em const \em basic_peer_manager is a basic_peer_manager with whom you can only send datas,
	 *       and you can't proceed to a connection / disconnection.
	 *
	 *
	 * @note Upon (connection)/(disconnection)/(data input) events, the corresponding detail::peer_manager_attorney
	 *       method should be called (see a the end of this file). Upon "successful" connection, the peer_manager
	 *       still has the authority on whether or not to keep the connection. If the connection is dropped, the callback
	 *       io_manager::process_connection_denial will be called. If the connection is kept,
	 *       the callback io_manager::process_connected_peer will be called
	 *
	 * @sa breep::io_manager_base
	 * @sa breep::tcp_network
	 * @sa breep::udp_network
	 *
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class basic_peer_manager {
	public:

		using inner_io_manager = io_manager;
		using peer = typename io_manager::peer;
		using peer_manager = basic_peer_manager<io_manager>;

		static const unsigned short default_port = 3479;
		using network_command_handler = void (peer_manager::*)(const peer&, const std::vector<uint8_t>&);

		/**
		 * Type representing a connection listener
		 * The function should take \em this instance of \em peer_manager,
		 * and the newly connected peer as parameters.
		 *
	 	 * @since 0.1.0
		 */
		using connection_listener = std::function<void(peer_manager& network, const peer& new_peer)>;

		/**
		 * Type representing a data listener.
		 * The function should take \em this instance of \em peer_manager, the peer that
		 * sent the data, the data itself, and a boolean (set to true if the data was sent to
		 * all the network and false if it was sent only to you) as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using data_received_listener = std::function<void(peer_manager& network, const peer& received_from, cuint8_random_iterator random_iterator, size_t data_size, bool sent_to_all)>;

		/**
		 * Type representing a disconnection listener.
		 * The function should take \em this instance of \em peer_manager and the
		 * disconnected peer as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using disconnection_listener = std::function<void(peer_manager& network, const peer& disconnected_peer)>;

		/**
		 * @since 0.1.0
		 */
		explicit basic_peer_manager(unsigned short port = default_port) noexcept
				: basic_peer_manager(io_manager{port}, port)
		{}

		/**
		 * @since 0.1.0
		 */
		explicit basic_peer_manager(const io_manager& manager, unsigned short port = default_port) noexcept
				: basic_peer_manager(io_manager(manager), port)
		{}

		/**
		 * @since 0.1.0
		 */
		explicit basic_peer_manager(io_manager&& manager, unsigned short port = default_port) noexcept;

		~basic_peer_manager() {
			disconnect();
			join();
		}

		/**
		 * @brief Sends data to all members of the network
		 * @tparam data_container Type representing data. Exact definition
		 *                        is to be defined by \em network_manager::send_to
		 * @param data Data to be sent
		 *
		 * @sa basic_peer_manager::send_to(const peer&, const data_container&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename data_container>
		void send_to_all(const data_container& data) const;

		/**
		 * Sends data to a specific member of the network
		 * @tparam data_container Type representing data. Exact definition
		 *                        is to be defined by \em network_manager_base::send
		 * @param p Target peer
		 * @param data Data to be sent
		 *
		 * @sa basic_peer_manager::send_to_all(const data_container&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename data_container>
		void send_to(const peer& p, const data_container& data) const;

		/**
		 * Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 *
		 * @throws invalid_state if the peer_manager is already running.
		 *
		 * @attention if the network was previously started, shot down, and that ::run() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @since 0.1.0
		 */
		void run();

		/**
		 * Starts a new network. Same as run(), excepts it is a blocking method.
		 *
		 * @throws invalid_state if the peer_manager is already running.
		 *
		 * @attention if the network was previously started, shot down, and that ::sync_run() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @since 0.1.0
		 */
		void sync_run();

		/**
		 * @brief asynchronically connects to a peer to peer network, given the ip of one peer
		 * @note  it is not possible to be connected to more than one network at the same time.
		 *
		 * @param address Address of a member
		 * @param port Target port. Defaults to the local listening port.
		 * @return true if the connection was successful, false otherwise.
		 *
		 * @throws invalid_state when trying to connect to a network if the peer_manager is already running.
		 * @note when \em false is returned, the network's thread is not started.
		 *
		 * @attention if the network was previously started, shot down, and that ::connect() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @sa basic_peer_manager::connect_sync(const boost::asio::ip::address&)
		 *
	 	 * @since 0.1.0
		 */
		bool connect(const boost::asio::ip::address& address, unsigned short port);
		bool connect(const boost::asio::ip::address& address) {
			return connect(address, m_port);
		}

		/**
		 * @brief Similar to \em network::connect(const boost::asio::ip::address&), but blocks until disconnected from all the network or the connection was not successful.
		 * @return true if connection was successful, false otherwise
		 *
		 * @attention if the network was previously started, shot down, and that ::sync_connect() is called before ensuring
		 *            the thread terminated (via ::join(), for example), the behaviour is undefined.
		 *
		 * @throws invalid_state if the peer_manager is already running.
		 *
		 * @sa basic_peer_manager::connect(const boost::asio::ip::address& address)
		 *
		 * @since 0.1.0
		 */
		bool sync_connect(const boost::asio::ip::address& address, unsigned short port);
		bool sync_connect(const boost::asio::ip::address& address) {
			return sync_connect(address, m_port);
		}

		/**
		 * @brief disconnects from the network
		 *
	 	 * @since 0.1.0
	 	 */
		void disconnect();

		/**
		 * @brief Adds a listener for incoming connections
		 * @details Each time a new peer connects to the network,
		 *          the method passed as a parameter is called, with
		 *          parameters specified in \em network::connection_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa basic_peer_manager::connection_listener
		 * @sa basic_peer_manager::remove_connection_listener(listener_id)
		 *
	 	 * @since 0.1.0
		 */
		listener_id add_connection_listener(connection_listener listener);

		/**
		 * @brief Adds a listener for incoming connections
		 * @details Each time a peer sends you data, the method
		 *          passed as a parameter is called, with parameters
		 *          specified in \em network::data_received_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa basic_peer_manager::data_received_listener
		 * @sa basic_peer_manager::remove_data_listener(listener_id)
		 *
		 * @since 0.1.0
		 */
		listener_id add_data_listener(data_received_listener listener);

		/**
		 * @brief Adds a listener for disconnections
		 * @details Each time a peer sends you data, the method
		 *          passed as a parameter is called, with parameters
		 *          specified in \em network::disconnection_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa basic_peer_manager::data_received_listener
		 * @sa basic_peer_manager::remove_disconnection_listener(listener_id)
		 *
		 * @since 0.1.0
		 */
		listener_id add_disconnection_listener(disconnection_listener listener);

		/**
		 * @brief Removes a listener
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @since 0.1.0
		 */
		bool remove_connection_listener(listener_id id);

		/**
		 * @brief Removes a listener
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @since 0.1.0
		 */
		bool remove_data_listener(listener_id id);

		/**
		 * @brief Removes a listener
		 * @param id id of the listener to remove
		 * @return true if a listener was removed, false otherwise
		 *
		 * @since 0.1.0
		 */
		bool remove_disconnection_listener(listener_id id);

		/**
		 * @return The list of connected peers (you excluded)
		 *
		 * @since 0.1.0
		 */
		const std::unordered_map<boost::uuids::uuid, peer, boost::hash<boost::uuids::uuid>>& peers() const {
			return m_peers;
		}

		/**
		 * @return true if the network is launched, false otherwise
		 *
		 * @since 1.0.0
		 */
		bool is_running() const {
			return m_running;
		}

		/**
		 * @return The port to which the object is currently mapped to.
		 *
		 * @since 0.1.0
		 */
		unsigned short port() const {
			return m_port;
		}

		/**
		 * @brief Changes the port to which the object is mapped
		 * @param port the new port
		 * @attention If the port is changed while there are ongoing connections, breep::invalid_state exception is raised.
		 *
		 * @since 0.1.0
		 */
		void port(unsigned short port) {
			if (m_port != port) {
				require_non_running();
				m_port = port;
				static_cast<io_manager_base<io_manager>*>(&m_manager)->port(port);
			}
		}

		/**
		 * @return a peer representing the local computer on the network.
		 *
		 * @since 0.1.0
		 */
		const local_peer<io_manager>& self() const {
			return m_me;
		}

		void set_log_level(log_level ll) const {
			breep::logger<peer_manager>.level(ll);
			m_manager.set_log_level(ll);
		}

		/**
		 * @brief removes all data listeners from the list.
		 *
		 * @since 0.1.0
		 */
		void clear_data_listeners() {
			std::lock_guard<std::mutex> lock_guard(m_data_mutex);
			m_data_r_listener.clear();
		}

		/**
		 * @brief removes all connection listeners from the list.
		 *
		 * @since 0.1.0
		 */
		void clear_connection_listeners() {
			std::lock_guard<std::mutex> lock_guard(m_co_mutex);
			m_co_listener.clear();
		}

		/**
		 * @brief removes all disconnection listeners from the list.
		 *
		 * @since 0.1.0
		 */
		void clear_disconnection_listeners() {
			std::lock_guard<std::mutex> lock_guard(m_dc_mutex);
			m_dc_listener.clear();
		}

		/**
		 * @brief clears all listeners
		 *
		 * @since 0.1.0
		 */
		void clear_any() {
			clear_data_listeners();
			clear_connection_listeners();
			clear_disconnection_listeners();
		}

		/**
		 * @brief Wait until the network stopped
		 * @details If the network is not launched, returns immediately
		 *
		 * @since 0.1.0
		 */
		void join() {
			if (m_thread && m_thread->joinable()) {
				m_thread->join();
				m_thread.reset(nullptr);
			}
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
		 * @sa basic_peer_manager::remove_connection_predicate()
		 */
		void set_connection_predicate(std::function<bool(const peer&)> pred) {
			m_predicate = std::move(pred);
		}


		/**
		 * @brief removes previously set connection predicate and goes back to default: accepting any connection
		 *
		 * @since 1.0.0
		 *
		 * @sa basic_peer_manager::set_connection_predicate(std::function<bool(const peer&)> pred)
		 */
		void remove_connection_predicate() {
			m_predicate = [](const auto&){return true;};
		}

	private:

		bool try_connect(const boost::asio::ip::address& address, unsigned short port);

		void peer_connected(peer&& p);
		void peer_connected(peer&& p, unsigned char distance, peer& bridge);
		void peer_disconnected(peer& p);
		void data_received(const peer& source, commands command, const std::vector<uint8_t>& data);

		void update_distance(const peer& concerned_peer);

		void forward_if_needed(const peer& source, commands command, const std::vector<uint8_t>& data);
		void require_non_running() {
			if (m_running) {
				invalid_state("Already running.");
			}
		}

		/* command handlers */
		void send_to_handler(const peer& source, const std::vector<uint8_t>& data);
		void send_to_all_handler(const peer& source, const std::vector<uint8_t>& data);
		void forward_to_handler(const peer& source, const std::vector<uint8_t>& data);
		void stop_forwarding_handler(const peer& source, const std::vector<uint8_t>& data);
		void forwarding_to_handler(const peer& source, const std::vector<uint8_t>& data);
		void connect_to_handler(const peer& source, const std::vector<uint8_t>& data);
		void cant_connect_handler(const peer& source, const std::vector<uint8_t>& data);
		void update_distance_handler(const peer& source, const std::vector<uint8_t>& data);
		void retrieve_distance_handler(const peer& source, const std::vector<uint8_t>& data);
		void retrieve_peers_handler(const peer& source, const std::vector<uint8_t>& data);
		void peers_list_handler(const peer& source, const std::vector<uint8_t>& data);
		void peer_disconnection_handler(const peer& source, const std::vector<uint8_t>& data);
		void empty_handler(const peer&, const std::vector<uint8_t>&) {
			breep::logger<peer_manager>.warning("Call to empty_handler was made. This is not supposed to happen in normal circonstances.\n");
		}

		void keep_alive_handler(const peer& p, const std::vector<uint8_t>& /* unused */) {
			breep::logger<peer_manager>.trace("Received keep_alive from " + p.id_as_string());
		}

		std::unordered_map<boost::uuids::uuid, peer, boost::hash<boost::uuids::uuid>> m_peers;
		std::unordered_map<listener_id, connection_listener> m_co_listener;
		std::unordered_map<listener_id, data_received_listener> m_data_r_listener;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listener;

		// predicate telling whether a peer should be accepted or not
		// Is ignored when connecting to someone
		std::function<bool(const peer&)> m_predicate;
		bool m_ignore_predicate{false};

		local_peer<io_manager> m_me;
		std::vector<std::unique_ptr<peer>> m_failed_connections;

		io_manager m_manager;

		listener_id m_id_count;

		unsigned short m_port;
        std::atomic<bool> m_running;

		network_command_handler m_command_handlers[static_cast<uint8_t>(commands::null_command)];

        std::mutex m_waitfor_run;
		mutable std::mutex m_co_mutex;
		mutable std::mutex m_dc_mutex;
		mutable std::mutex m_data_mutex;

		friend class detail::peer_manager_attorney<io_manager>;

		std::unique_ptr<std::thread> m_thread;
	};


	template <typename T>
	class basic_network;

	namespace detail {

	template <typename T>
	class peer_manager_attorney {

	public:
		peer_manager_attorney() = delete;

	private:
		inline static void peer_connected(basic_peer_manager<T>& object, basic_peer<T>&& p) {
			object.peer_connected(std::move(p));
		}

		inline static void peer_disconnected(basic_peer_manager<T>& object, basic_peer<T>& p) {
			object.peer_disconnected(p);
		}

		inline static void data_received(basic_peer_manager<T>& object, const basic_peer<T>& source, commands command, const std::vector<uint8_t>& data) {
			object.data_received(source, command, data);
		}

		friend T;
	};

	}
}
BREEP_DECLARE_TEMPLATE(breep::basic_peer_manager)

#include "impl/basic_peer_manager.tcc"

#endif //BREEP_NETWORK_BASIC_PEER_MANAGER_HPP
