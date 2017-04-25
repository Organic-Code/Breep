#ifndef BREEP_NETWORK_HPP
#define BREEP_NETWORK_HPP

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
 * @file network.hpp
 * @author Lucas Lazare
 */

#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include <boost/functional/hash.hpp>

#include "commands.hpp"
#include "peer.hpp"
#include "local_peer.hpp"
#include "network_manager_base.hpp"
#include "invalid_state.hpp"

namespace breep {

	typedef unsigned long listener_id;

	namespace detail {
		template<typename T>
		class network_attorney_client;
	}

	/**
	 * @class network network.hpp
	 * @brief                  This class is used to manage a peer to peer network.
	 * @tparam network_manager Manager used to manage the network
	 *                         This class should inherit from \em breep::network_manager_base
	 *                         see \em breep::tcp_nmanager and \em breep::udp_nmanager for examples of implementation.
	 *                         network_manager::socket_type must also be defined.
	 *
	 * @note A \em const \em network is a network with whom you can only send datas,
	 *       and you can't proceed to a connection / disconnection.
	 *
	 * @sa breep::tcp_network
	 * @sa breep::udp_network
	 *
	 * @since 0.1.0
	 */
	template <typename network_manager>
	class network {
	public:
		static const unsigned short default_port = 3479;
		using network_command_handler = void (network<network_manager>::*)(const peer<network_manager>&, const std::vector<uint8_t>&);

		/**
		 * Type representing a connection listener
		 * The function should take \em this instance of \em network<network_manager>, the newly connected peer,
		 * and the distance (number of peers bridging from the \em user to the newly connected peer) as parameters.
		 *
	 	 * @since 0.1.0
		 */
		using connection_listener = std::function<void(breep::network<network_manager>& network, const breep::peer<network_manager>& new_peer, unsigned short distance)>;

		/**
		 * Type representing a data listener.
		 * The function should take \em this instance of \em network<network_manager>, the peer that
		 * sent the data, the data itself, and a boolean (set to true if the data was sent to
		 * all the network and false if it was sent only to you) as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using data_received_listener = std::function<void(breep::network<network_manager>& network, const breep::peer<network_manager>& received_from, const std::deque<uint8_t>& data, bool sent_to_all)>;

		/**
		 * Type representing a disconnection listener.
		 * The function should take \em this instance of \em network<network_manager> and the
		 * disconnected peer as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using disconnection_listener = std::function<void(breep::network<network_manager>& network, const breep::peer<network_manager>& disconnected_peer)>;

		/**
		 * @since 0.1.0
		 */
		explicit network(unsigned short port = default_port) noexcept
				: network(network_manager{port}, port)
		{}

		/**
		 * @since 0.1.0
		 */
		explicit network(const network_manager& manager, unsigned short port = default_port) noexcept
				: network(network_manager(manager), port)
		{}

		/**
		 * @since 0.1.0
		 */
		explicit network(network_manager&& manager, unsigned short port = default_port) noexcept;

		/**
		 * @brief Sends data to all members of the network
		 * @tparam data_container Type representing data. Exact definition
		 *                        is to be defined by \em network_manager::send_to
		 * @param data Data to be sent
		 *
		 * @sa network::send_to(const peer&, const data_container&) const
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
		 * @sa network::send_to_all(const data_container&) const
		 *
	 	 * @since 0.1.0
		 */
		template <typename data_container>
		void send_to(const peer<network_manager>& p, const data_container& data) const;

		/**
		 * @copydoc network::send_to(const peer&, const data_container&) const;
		 */
		template <typename data_container>
		void send_to(const peer<network_manager>& p, data_container&& data) const;

		/**
		 * Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 */
		void run();

		/**
		 * Starts a new network. Same as run(), excepts it is a blocking method.
		 */
		void run_sync();

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
		void connect(boost::asio::ip::address address, unsigned short port);
		void connect(const boost::asio::ip::address& address) {
			connect(address, m_port);
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
		bool connect_sync(const boost::asio::ip::address& address, unsigned short port);

		/**
		 * @brief asynchronically disconnects from the network
		 *
		 * @sa network::disconnect_sync()
		 *
	 	 * @since 0.1.0
	 	 */
		void disconnect();

		/**
		 * @brief same as disconnect, but blocks until disconnected from the network
		 *
		 * @sa network::disconnect()
		 *
		 * @since 0.1.0
		 */
		void disconnect_sync();

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
		listener_id add_connection_listener(connection_listener listener);

		/**
		 * @brief Adds a listener for incoming connections
		 * @details Each time a peer sends you data, the method
		 *          passed as a parameter is called, with parameters
		 *          specified in \em network::data_received_listener
		 * @param listener The new listener
		 * @return An id used to remove the listener
		 *
		 * @sa network::data_received_listener
		 * @sa network::remove_data_listener(listener_id)
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
		 * @sa network::data_received_listener
		 * @sa network::remove_disconnection_listener(listener_id)
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
		 * @return The list of connected peers
		 *
		 * @details Returned by copy to avoid data race.
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, peer<network_manager>, boost::hash<boost::uuids::uuid>> peers() const {
			std::lock_guard<std::mutex> lock_guard(m_peers_mutex);
			return m_peers;
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
		 * @attention If the port is changed while there are ongoing connections,
		 *            the behavior is undefined and possibly depends on the underlying \em network_manager.
		 */
		void port(unsigned short port) {
			m_port = port;
		}

		const local_peer<network_manager>& self() const {
			return m_me;
		}

	private:

		void peer_connected(peer<network_manager>&& p, unsigned char distance);
		void peer_disconnected(const peer<network_manager>& p);
		void data_received(const peer<network_manager>& source, commands command, const std::vector<uint8_t>& data);


		void replace(peer<network_manager>& ancestor, const peer<network_manager>& successor);
		void forward_if_needed(const peer<network_manager>& source, commands command, const std::vector<uint8_t>& data);
		void require_non_running() {
			if (m_running)
				invalid_state("Already running.");
		}

		/* command handlers */
		void send_to_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void send_to_all_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void forward_to_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void stop_forwarding_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void forwarding_to_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void connect_to_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void cant_connect_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void successfully_connected_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void update_distance_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void retrieve_distance_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void peers_list_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void new_peer_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);
		void peer_disconnection_handler(const peer<network_manager>& peer, const std::vector<uint8_t>& data);

		std::unordered_map<boost::uuids::uuid, peer<network_manager>, boost::hash<boost::uuids::uuid>> m_peers;
		std::unordered_map<listener_id, connection_listener> m_co_listener;
		std::unordered_map<listener_id, data_received_listener> m_data_r_listener;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listener;
		local_peer<network_manager> m_me;

		network_manager m_manager;

		listener_id m_id_count;

		unsigned short m_port;
		bool m_running;

		network_command_handler m_command_handlers[static_cast<uint8_t>(commands::null_command)];

		mutable std::mutex m_co_mutex;
		mutable std::mutex m_dc_mutex;
		mutable std::mutex m_data_mutex;
		mutable std::mutex m_peers_mutex;

		friend class detail::network_attorney_client<network_manager>;
	};

	namespace detail {

	template <typename T>
	class network_attorney_client {

		network_attorney_client() = delete;

		inline static void peer_connected(network<T>& object, peer<T>&& p) {
			object.peer_connected(std::move(p), 0);
		}

		inline static void peer_disconnected(network<T>& object, const peer<T>& p) {
			object.peer_disconnected(p);
		}

		inline static void data_received(network<T>& object, const peer<T>& source, commands command, const std::vector<uint8_t>& data) {
			object.data_received(source, command, data);
		}

		friend T;
	};

	}
}

#include "impl/network.tcc"

#endif //BREEP_NETWORK_HPP
