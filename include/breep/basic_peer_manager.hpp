#ifndef BREEP_BASIC_PEER_MANAGER_HPP
#define BREEP_BASIC_PEER_MANAGER_HPP

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

#include "breep/commands.hpp"
#include "breep/basic_peer.hpp"
#include "breep/local_peer.hpp"
#include "breep/io_manager_base.hpp"
#include "breep/exceptions.hpp"

namespace breep {

	using uint8_random_iterator = const uint8_t*;

	typedef unsigned long listener_id;

	namespace detail {
		template<typename T>
		class peer_manager_attorney;
	}

	/**
	 * @class network network.hpp
	 * @brief                  This class is used to manage a peer to peer network.
	 * @tparam io_manager      Manager used to manage input and ouput operations of (including connection & disconection) the network
	 *                         This class should inherit from \em breep::network_manager_base
	 *                                see \em breep::tcp_nmanager and \em breep::udp_nmanager for examples of implementation.
	 *                                network_manager::socket_type must also be defined.
	 *
	 * @note A \em const \em network is a network with whom you can only send datas,
	 *       and you can't proceed to a connection / disconnection.
	 *
	 * @sa breep::tcp_network
	 * @sa breep::udp_network
	 *
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class basic_peer_manager {
	public:
		/**
		 * Peer type used by this class
		 */
		using peernm = basic_peer<io_manager>;

		static const unsigned short default_port = 3479;
		using network_command_handler = void (basic_peer_manager<io_manager>::*)(const peernm&, const std::vector<uint8_t>&);

		/**
		 * Type representing a connection listener
		 * The function should take \em this instance of \em basic_peer_manager<io_manager>,
		 * and the newly connected peer as parameters.
		 *
	 	 * @since 0.1.0
		 */
		using connection_listener = std::function<void(breep::basic_peer_manager<io_manager>& network, const peernm& new_peer)>;

		/**
		 * Type representing a data listener.
		 * The function should take \em this instance of \em basic_peer_manager<io_manager>, the peer that
		 * sent the data, the data itself, and a boolean (set to true if the data was sent to
		 * all the network and false if it was sent only to you) as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using data_received_listener = std::function<void(breep::basic_peer_manager<io_manager>& network, const peernm& received_from, uint8_random_iterator random_iterator, size_t data_size, bool sent_to_all)>;

		/**
		 * Type representing a disconnection listener.
		 * The function should take \em this instance of \em basic_peer_manager<io_manager> and the
		 * disconnected peer as parameter.
		 *
	 	 * @since 0.1.0
		 */
		using disconnection_listener = std::function<void(breep::basic_peer_manager<io_manager>& network, const peernm& disconnected_peer)>;

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
		void send_to(const peernm& p, const data_container& data) const;

		/**
		 * @copydoc network::send_to(const peer&, const data_container&) const;
		 */
		template <typename data_container>
		void send_to(const peernm& p, data_container&& data) const;

		/**
		 * Starts a new network on background. It is considered as a network connection (ie: you can't call connect(ip::address)).
		 */
		void run();

		/**
		 * Starts a new network. Same as run(), excepts it is a blocking method.
		 */
		void sync_run();

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
		const std::unordered_map<boost::uuids::uuid, peernm, boost::hash<boost::uuids::uuid>>& peers() const {
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
		 * @attention If the port is changed while there are ongoing connections, breep::invalid_state exception is raised.
		 */
		void port(unsigned short port) {
			if (m_port != port) {
				require_non_running();
				m_port = port;
				static_cast<io_manager_base<io_manager>*>(&m_manager)->port(port);
			}
		}

		/**
		 * @return a peer represeting the local computer on the network.
		 */
		const local_peer<io_manager>& self() const {
			return m_me;
		}

	private:

		void peer_connected(peernm&& p);
		void peer_connected(peernm&& p, unsigned char distance, peernm& bridge);
		void peer_disconnected(peernm& p);
		void data_received(const peernm& source, commands command, const std::vector<uint8_t>& data);

		void update_distance(const peernm& concerned_peer);

		void forward_if_needed(const peernm& source, commands command, const std::vector<uint8_t>& data);
		void require_non_running() {
			if (m_running)
				invalid_state("Already running.");
		}

		bool sync_connect_impl(const boost::asio::ip::address address, unsigned short port);

		/* command handlers */
		void send_to_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void send_to_all_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void forward_to_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void stop_forwarding_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void forwarding_to_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void connect_to_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void cant_connect_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void update_distance_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void retrieve_distance_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void retrieve_peers_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void peers_list_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void peer_disconnection_handler(const peernm& peer, const std::vector<uint8_t>& data);
		void keep_alive_handler(const peernm&, const std::vector<uint8_t>&) {
		}

		std::unordered_map<boost::uuids::uuid, peernm, boost::hash<boost::uuids::uuid>> m_peers;
		std::unordered_map<listener_id, connection_listener> m_co_listener;
		std::unordered_map<listener_id, data_received_listener> m_data_r_listener;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listener;

		local_peer<io_manager> m_me;
		// todo: use .data instead of stringifying it.
		boost::uuids::string_generator m_uuid_gen;
		std::vector<std::unique_ptr<peernm>> m_failed_connections;

		io_manager m_manager;

		listener_id m_id_count;

		unsigned short m_port;
		bool m_running;

		network_command_handler m_command_handlers[static_cast<uint8_t>(commands::null_command)];

		mutable std::mutex m_co_mutex;
		mutable std::mutex m_dc_mutex;
		mutable std::mutex m_data_mutex;

		friend class detail::peer_manager_attorney<io_manager>;
	};

	namespace detail {

	template <typename T>
	class peer_manager_attorney {

		peer_manager_attorney() = delete;

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

#include "impl/basic_peer_manager.tcc"

#endif //BREEP_BASIC_PEER_MANAGER_HPP
