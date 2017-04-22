#ifndef BREEP_COMMANDS_HPP
#define BREEP_COMMANDS_HPP

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
 * @file commands.hpp
 * @author Lucas Lazare
 */


#include <cstdint>

namespace breep {

	/**
	 * @enum List of available commands
	 *       for the network
	 *
	 * @since 0.1.0
	 */
	enum class commands : uint8_t {
		/**
		 * @brief     Command for sending data to a specific peer.
		 * @details   Must be followed by some data
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
		 * @details   Must be followed your own distance to the peer (1 octet) + by a peer (id)
		 * @sa        commands::forward_to
		 * @sa        commands::stop_forwarding
		 *
		 * @since     0.1.0
		 */
		forwarding_to,
		/**
		 * @brief     Command to tell a peer to connect to another buddy.
		 * @details   Must be followed by a target peer (size of id (in octets, on 1 octet) + id + ip).
		 *
		 * @since 0.1.0
		 */
		connect_to,
		/**
		 * @brief     Command to indicate a peer that a connection request failed.
		 * @details   Possibly an answer to \em connect_to. Must be followed by
		 *            a peer (id).
		 *
		 * @since 0.1.0
		 */
		cant_connect,
		/**
		 * @brief     Command to indicate a peer that a connection request succeeded.
		 * @details   Possible answer to \em connect_to. Must be followed by a peer
		 *            (id).
		 *
		 * @since 0.1.0
		 */
		successfully_connected,
		/**
		 * @brief     Command to update its distances (number of links) between
		 *            a/some peers.
		 * @details   Format : [update_distance],[number of updated peers],[peer id],[new distance],[peer id],...
		 *
		 * @since 0.1.0
		 */
		update_distance,
		/**
		 * @brief     Command to ask a peer to send its distances (number of links)
		 *            between him and all other peers.
		 * @sa        commands::update_distance
		 * @sa        commands::peers_list
		 *
		 * @since 0.1.0
		 */
		retrieve_distance,
		/**
		 * @brief     Command to send peers list.
		 * @details   Answer to \em retrieve_distance.
		 *            Format : [peers_list],[number of peers],[[peer1 ip],[peer1 id]],[[peer2 ip],[peer2 id]],...
		 *
		 * @sa        commands::retrieve_distance.
		 *
		 * @since 0.1.0
		 */
		peers_list,
		/**
		 * @brief     Command to indicate the connection of a new peer
		 * @details   Must be followed by the new peer (id + ip).
		 *
		 * @sa        commands::peer_disconnection
		 *
		 * @since 0.1.0
		 */
		new_peer,
		/**
		 * @brief     Command to indicate the disconnection of a peer
		 * @details   Must be followed by the peer (id).
		 *
		 * @sa        commands::peers_list
		 *
		 * @since 0.1.0
		 */
		peer_disconnection,
	};
}

#endif //BREEP_COMMANDS_HPP
