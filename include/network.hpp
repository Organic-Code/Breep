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
#include <boost/uuid/uuid.hpp>
#include <boost/functional/hash.hpp>

#include "commands.hpp"
#include "peer.hpp"
#include "local_peer.hpp"

namespace breep {

	typedef unsigned long listener_id;

	class network_manager_base;

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
		using data_received_listener = std::function<void(breep::network<network_manager>& network, const breep::peer<network_manager>& received_from, const std::vector<char>& data, bool sent_to_all)>;

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
				: m_peers{}
				, m_co_listener{}
				, m_data_r_listener{}
				, m_dc_listener{}
				, m_me{}
				, m_manager{}
				, m_id_count{0}
				, m_port{port}
		{
			static_assert(std::is_base_of<network_manager_base, network_manager>::value, "Specified type not derived from breep::network_manager_base");
			m_manager.owner(this);
		}

		/**
		 * @since 0.1.0
		 */
		explicit network(const network_manager& manager, unsigned short port = default_port) noexcept
				: m_peers{}
				, m_co_listener{}
				, m_data_r_listener{}
				, m_dc_listener{}
				, m_me{}
				, m_manager(manager)
				, m_id_count{0}
				, m_port{port}
		{
			static_assert(std::is_base_of<network_manager_base, network_manager>::value, "Specified type not derived from breep::network_manager_base");
			m_manager.owner(this);
		}

		/**
		 * @since 0.1.0
		 */
		explicit network(network_manager&& manager, unsigned short port = default_port) noexcept
				: m_peers{}
				, m_co_listener{}
				, m_data_r_listener{}
				, m_dc_listener{}
				, m_me{}
				, m_manager(manager)
				, m_id_count{0}
				, m_port{port}
		{
			static_assert(std::is_base_of<network_manager_base, network_manager>::value, "Specified type not derived from breep::network_manager_base");
			m_manager.owner(this);
		}

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
		 * @copydoc network::send_to_all(const data_container&) const
		 */
		template <typename data_container>
		void send_to_all(data_container&& data) const;


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
		 * @brief asynchronically connects to a peer to peer network, given the ip of one peer
		 * @note  it is not possible to be connected to more than one network at the same time.
		 *
		 * @param address Address of a member
		 *
		 * @throws invalid_state thrown when trying to connect twice to a network.
		 *
		 * @sa network::connect_sync(const boost::asio::ip::address&)
		 *
	 	 * @since 0.1.0
		 */
		void connect(const boost::asio::ip::address& address);
		/**
		 * @copydoc network::connect(const boost::asio::ip::address&)
		 */
		void connect(boost::asio::ip::address&& address);

		/**
		 * @brief Similar to \em network::connect(const boost::asio::ip::address&), but blocks until connected to the network
		 * @return true if connection was successful, false otherwise
		 *
		 * @throws invalid_state thrown when trying to connect twice to a network.
		 *
		 * @sa network::connect(const boost::asio::ip::address& address)
		 *
		 * @since 0.1.0
		 */
		bool connect_sync(const boost::asio::ip::address& address);

		/**
		 * @copydoc network::connect_sync(const boost::asio::ip::address&)
		 */
		bool connect_sync(boost::asio::ip::address&& address);

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
		listener_id add_listener(connection_listener listener);
		/**
		 * @copydoc network::add_listener(connection_listener)
		 */
		listener_id add_listener(connection_listener&& listener);

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
		listener_id add_listener(data_received_listener listener);
		/**
		 * @copydoc network::add_listener(data_received_listener)
		 */
		listener_id add_listener(data_received_listener&& listener);

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
		listener_id add_listener(disconnection_listener listener);
		/**
		 * @copydoc network::add_listener(disconnection_listener)
		 */
		listener_id add_listener(disconnection_listener&& listener);

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
		 * @since 0.1.0
		 */
		const std::unordered_map<boost::uuids::uuid, peer<network_manager>, boost::hash<boost::uuids::uuid>>& peers() const {
			return m_peers;
		}

		/**
		 * @return The port to which the object is currently mapped to.
		 *
		 * @since 0.1.0
		 */
		unsigned short port() const {
			return m_port
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

		std::unordered_map<boost::uuids::uuid, peer<network_manager>, boost::hash<boost::uuids::uuid>> m_peers;
		std::unordered_map<listener_id, connection_listener> m_co_listener;
		std::unordered_map<listener_id, data_received_listener> m_data_r_listener;
		std::unordered_map<listener_id, disconnection_listener> m_dc_listener;
		local_peer<network_manager> m_me;

		network_manager m_manager;

		listener_id m_id_count;

		unsigned short m_port;
	};
}

#include "network.tpp"

#endif //BREEP_NETWORK_HPP
