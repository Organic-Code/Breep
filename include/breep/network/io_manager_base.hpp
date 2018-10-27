#ifndef BREEP_NETWORK_IO_MANAGER_BASE_HPP
#define BREEP_NETWORK_IO_MANAGER_BASE_HPP


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
 * @file io_manager_base.hpp
 * @author Lucas Lazare
 *
 * @since 0.1.0
 */

#include <boost/asio/ip/address.hpp>

#include "breep/util/type_traits.hpp"
#include "breep/network/detail/commands.hpp"
#include "breep/network/detail/utils.hpp"

namespace boost { namespace asio { namespace ip {
	class address;
}}}

namespace breep {

	enum class log_level;

	template <typename T>
	class basic_peer_manager;

	template <typename T>
	class basic_peer;

	/**
	 * @class io_manager_base io_manager_base.hpp
	 * @brief base class for network managers, used by \em network<typename network_manager>.
	 * @details Classes inheriting from io_manager_base should specify a data_type type, for its personal use via the peer<>.io_data member.
	 * The object should open ports and start listening for incoming connections as soon as they get an owner.
	 * The constructor should take an unsigned short [port] as parameter if instantiated from the class breep::network.
	 *
	 * @sa breep::network
	 *
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class io_manager_base {
	public:

		virtual ~io_manager_base() = default;

		/**
		 * @brief Sends data to a peer
		 *
		 * @tparam data_container Any data container type you want to support. In the case of \em tcp_nmanager
		 *                        and \em udp_nmanager, the data_container type should respect the \em Container concept
		 *                        (for \em uint8_t).
		 *
		 * @param command command of the packet (considered as data)
		 * @param data data to be sent. It is uppon your responsability to check for endianness.
		 * @param peer the peer to whom to send the data.
		 *
		 * @since 0.1.0
		 */
		template <typename data_container>
		void send(commands /*command*/, const data_container& /*data*/, const basic_peer<io_manager>& /*peer*/) const {
			static_assert(detail::dependent_false<io_manager_base<io_manager>, data_container>::value, "Send called without specialisation.");
		}

		/**
		 * @brief Sends data to a peer
		 *
		 * @tparam data_iterator Any data iterator type you want to support. In the case of \em tcp_nmanager
		 *                        and \em udp_nmanager, the data_iterator type should respect the \em InputIterator concept.
		 *                        (for \em uint8_t).
		 * @tparam size_type
		 *
		 * @param command command of the packet (considered as data)
		 * @param begin iterator to the data to be sent. It is uppon your responsability to check for endianness.
		 * @param size quantity of data to be sent
		 * @param peer of the peer to whom to send the data.
		 *
		 * @since 0.1.0
		 */
		template <typename data_iterator, typename  size_type>
		void send(commands command, data_iterator /*begin*/, size_type /*size*/, const basic_peer<io_manager>& /*peer*/) const {
			static_assert(detail::dependent_false<io_manager_base<io_manager>, data_iterator>::value, "Send called without specialisation.");
		}

		/**
		 * @brief connects to a peer
		 *
		 * @return the newly connected peer or an empty optional if the connection wasn't successful.
		 *
		 * @since 0.1.0
		 */
		virtual detail::optional<basic_peer<io_manager>> connect(const boost::asio::ip::address&, unsigned short port) = 0;

		/**
		 * @brief performs any required action after a peer connection.
		 *
		 * @since 0.1.0
		 */
		virtual void process_connected_peer(basic_peer<io_manager>& peer) = 0;

		/**
		 * @brief performs any required action when a peer connection was denied.
		 *
		 * @since 1.0.0
		 */
		 virtual void process_connection_denial(basic_peer<io_manager>& peer) = 0;

		/**
		 * @brief disconnects from the network
		 *
		 * @since 0.1.0
		 */
		virtual void disconnect() = 0;

		/**
		 * @brief disconnects a peer
		 */
		virtual void disconnect(basic_peer<io_manager>& peer) = 0;

		/**
		 * @brief Network's main thread entry point
		 *
		 * @since 0.1.0
		 */
		virtual void run() = 0;

		/**
		 * @brief sets the logging level.
		 *
		 * @since 0.1.0
		 */
		virtual void set_log_level(log_level) const = 0;

	protected:

		/**
		 * @brief sets the port
		 * 			Will never be called from the owner if the network is active.
		 * @param port the new port.
		 *
		 * @since 0.1.0
		 */
		virtual void port(unsigned short port) = 0;

		/**
		 * @brief sets the owner of the network_manager, ie the object to whom received datas should be redirected.
		 *
		 * @since 0.1.0
		 */
		virtual void owner(basic_peer_manager<io_manager>* owner) = 0;

		friend class basic_peer_manager<io_manager>;
	};
}  // namespace breep

BREEP_DECLARE_TEMPLATE(breep::io_manager_base)

#endif //BREEP_NETWORK_IO_MANAGER_BASE_HPP
