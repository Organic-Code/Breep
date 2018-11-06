#ifndef BREEP_NETWORK_BASIC_PEER_HPP
#define BREEP_NETWORK_BASIC_PEER_HPP

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
 * @file peer.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */

#include <utility>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "breep/network/detail/commands.hpp"

namespace breep {

	/**
	 * @class basic_peer basic_peer.hpp
	 *
	 * @brief This class represents a network's member
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class basic_peer {
	public:

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(const boost::uuids::uuid& id, const boost::asio::ip::address& address, unsigned short port = 0,
		     const typename io_manager::data_type& data = typename io_manager::data_type())
			: basic_peer(
				boost::uuids::uuid(id),
				boost::asio::ip::address(address),
				port,
			    typename io_manager::data_type(data)
			)
		{}

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(boost::uuids::uuid&& id, boost::asio::ip::address&& address, unsigned short port,
		     typename io_manager::data_type&& data)
				: m_id(std::move(id))
				, m_address(std::move(address))
				, m_port(port)
				, m_distance()
				, m_id_as_string(boost::uuids::to_string(id))
				, io_data(std::move(data))
		{}

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(const basic_peer<io_manager>& p) = default;

		/**
		 * @since 0.1.0
		 */
		basic_peer(basic_peer<io_manager>&&) = default;

		/**
		 * @since 1.0.0
		 */
		basic_peer<io_manager>& operator=(const basic_peer<io_manager>&) = default;

		/**
		 * @since 1.0.0
		 */
		basic_peer<io_manager>& operator=(basic_peer<io_manager>&&) = default;

		/**
		 * @return the id of the peer
	 	 * @since 0.1.0
		 */
		const boost::uuids::uuid& id() const noexcept;

		/**
		 * @return the address
	 	 * @since 0.1.0
		 */
		const boost::asio::ip::address& address() const noexcept;

		bool operator==(const basic_peer<io_manager>& lhs) const;

		bool operator!=(const basic_peer<io_manager>& lhs) const;

		/**
		 * @return The distance from you to the peer.
		 * @since 0.1.0
		 */
		unsigned char distance() const {
			return m_distance;
		}

		void distance(unsigned char d) {
			m_distance = d;
		}

		void connection_port(unsigned short port) {
			m_port = port;
		}

		/**
		 * @return the remote port
		 * @since 0.1.0
		 */
		unsigned short connection_port() const {
			return m_port;
		}

		/**
		 * @return true if the peer is connected, false otherwise
		 * @since 0.1.0
		 */
		bool is_connected() const {
			return m_distance != std::numeric_limits<unsigned char>::max();
		}

		/**
		 * @return a string representation of the id
		 * @since 0.1.0
		 */
		const std::string& id_as_string() const {
			return m_id_as_string;
		}

	private:
		const boost::uuids::uuid m_id;
		const boost::asio::ip::address m_address;

		unsigned short m_port;

		/**
		 * Distance from here to the other peers.
		 */
		unsigned char m_distance;

		const std::string m_id_as_string;

		/**
		 * Datas to be used by the io_manager class
		 */
		typename io_manager::data_type io_data;

		friend io_manager;
	};

#include "impl/basic_peer.tcc"
} // namespace breep

BREEP_DECLARE_TEMPLATE(breep::basic_peer)

#endif //BREEP_NETWORK_BASIC_PEER_HPP
