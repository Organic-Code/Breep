#ifndef BREEP_NETWORK_TCP_HPP
#define BREEP_NETWORK_TCP_HPP

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
 * @file tcp.hpp
 * @author Lucas Lazare
 * @brief convenience header for breep::tcp.
 */

#include <breep/util/type_traits.hpp>
#include <breep/network/basic_netdata_wrapper.hpp>
#include <breep/network/basic_peer.hpp>
#include <breep/network/basic_network.hpp>
#include <breep/network/basic_peer.hpp>
#include <breep/network/tcp/basic_io_manager.hpp>

namespace breep { namespace tcp {
		using io_manager = basic_io_manager<1024, 5000, 120000, 54000>;
		using peer = basic_peer<io_manager>;
		using network = basic_network<io_manager>;
		using peer_manager = basic_peer_manager<io_manager>;

		template <typename T>
		using netdata_wrapper = basic_netdata_wrapper<io_manager, T>;
}}

BREEP_DECLARE_TYPE(breep::tcp::io_manager)
BREEP_DECLARE_TYPE(breep::tcp::peer)
BREEP_DECLARE_TYPE(breep::tcp::peer_manager)
BREEP_DECLARE_TYPE(breep::tcp::network)

#endif //BREEP_NETWORK_TCP_HPP
