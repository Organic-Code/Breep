#ifndef BREEP_NETWORK_COMMANDS_HPP
#define BREEP_NETWORK_COMMANDS_HPP

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
 * @file commands.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */


#include <cstdint>

namespace breep {

	/**
	 * @enum List of available commands for the network
	 *
	 * @note When updating this table, update the jump table on basic_peer_manager.
	 *
	 * @since 0.1.0
	 */
	enum class commands : uint8_t {
		/**
		 * @brief     Command for sending data to a specific peer.
		 * @details   Must be followed by: size of 1 id (in octet, in one octet) + sender id + target peer (id) + some data
		 * @attention Keep in mind that other peers may
		 *            still intercept the message, if there is no
		 *            direct link to the target buddy.
		 *
		 * @since 0.1.0
		 */
		send_to,
		/**
		 * @brief     Command for sending data to all peers.
		 * @details   Must be followed by some data
		 *
		 * @since 0.1.0
		 */
		send_to_all,
		/**
		 * @brief     Command to indicate a peer he must bridge for you
		 * @details   Must be followed by a target peer (id).
		 * @sa        commands::stop_forwarding
		 * @sa        commands::forwarding_to
		 *
		 * @since 0.1.0
		 */
		forward_to,
		/**
		 * @brief     Command to tell a peer to stop bridging for you.
		 * @details   Must be followed by a peer (id)
		 * @sa        commands::forward_to
		 * @sa        commands::forwarding_to
		 *
		 * @since 0.1.0
		 */
		stop_forwarding,
		/**
		 * @brief     Command to tell a peer you are bridging for him
		 * @details   Must be followed by your own distance to the peer (1 octet) + by a peer (id)
		 * @sa        commands::forward_to
		 * @sa        commands::stop_forwarding
		 *
		 * @since     0.1.0
		 */
		forwarding_to,
		/**
		 * @brief     Command to tell a peer to connect to another buddy.
		 * @details   Must be followed by a target peer (target port (2 octets) + size of id (in octets, on 1 octet) + id + ip).
		 *
		 * @since 0.1.0
		 */
		connect_to,
		/**
		 * @brief     Command to indicate a peer that a connection request failed.
		 * @details   Must be followed by a peer (id).
		 *
		 * @since 0.1.0
		 */
		cant_connect,
		/**
		 * @brief     Command to update its distances (number of links) between a peer.
		 * @details   Format : [new distance (on 1 octet)],[peer id]
		 *
		 * @since 0.1.0
		 */
		update_distance,
		/**
		 * @brief     Command to ask a peer to send its distance from another peer.
		 * @details   Must be followed by a peer (id).
		 * @sa        commands::update_distance
		 *
		 * @since 0.1.0
		 */
		retrieve_distance,
		/**
		 * @brief     Command to ask a peer to send the list of peers.
		 * @sa        commands::update_distance
		 * @sa        commands::peers_list
		 *
		 * @since 0.1.0
		 */
		retrieve_peers,
		/**
		 * @brief     Command to send peers list (omitting yourself).
		 * @details   Answer to \em retrieve_peers.
		 *            Format : [number of peers (2octets)],[peer1],[peer2],...<br>
		 *            with: peerN = [peerN port (2octets)], [peerN id size (in octets, on 1 octet)], [peerN id],
		 *                          [peerN address size (in octets, on 1 octet)], [peerN address]
		 *
		 * @sa        commands::retrieve_peers.
		 *
		 * @since 0.1.0
		 */
		peers_list,
		/**
		 * @brief     Command to indicate the disconnection of a peer
		 * @details   Must be followed by the peer (id).
		 *
		 * @sa        commands::peers_list
		 *
		 * @since 0.1.0
		 */
		peer_disconnection,
		/**
		 * @brief ignored by basic_peer_manager [only logging it]
		 *
		 * @since 0.1.0
		 */
		keep_alive,
		/**
		 * @brief Accepting or refusing incomming connection, used by io_managers for process_connected_peer and process_connection_denial.
		 *
		 * @since 1.0.0
		 */
		connection_accepted,
		connection_refused,

		/**
		 * @brief never sent
		 * @since 0.1.0
		 */
		null_command
	};
}

#endif //BREEP_NETWORK_COMMANDS_HPP
