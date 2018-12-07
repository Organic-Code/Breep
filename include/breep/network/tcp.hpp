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

	namespace details {
		struct empty {};
	}

	template <typename data_structure>
	using io_manager_ds = basic_io_manager<data_structure, 1024, 5000, 120000, 54000>;

	template <typename data_structure>
	using peer_ds = basic_peer<io_manager_ds<data_structure>, data_structure>;

	template <typename data_structure>
	using peer_manager_ds = basic_peer_manager<breep::tcp::io_manager_ds<data_structure>>;

	template <typename data_structure>
	using network_ds = basic_network<breep::tcp::io_manager_ds<data_structure>>;

	template <typename T, typename data_structure>
	using netdata_wrapper_ds = basic_netdata_wrapper<breep::tcp::io_manager_ds<data_structure>, T>;

	using io_manager = io_manager_ds<details::empty>;
	using peer = peer_ds<details::empty>;
	using peer_manager = peer_manager_ds<details::empty>;
	using network = network_ds<details::empty>;
	template <typename T>
	using netdata_wrapper = netdata_wrapper_ds<T, details::empty>;
}}

BREEP_DECLARE_TEMPLATE(breep::tcp::io_manager_ds)
BREEP_DECLARE_TEMPLATE(breep::tcp::peer_ds)
BREEP_DECLARE_TEMPLATE(breep::tcp::peer_manager_ds)
BREEP_DECLARE_TEMPLATE(breep::tcp::network_ds)

BREEP_DECLARE_TYPE(breep::tcp::io_manager)
BREEP_DECLARE_TYPE(breep::tcp::peer)
BREEP_DECLARE_TYPE(breep::tcp::peer_manager)
BREEP_DECLARE_TYPE(breep::tcp::network)

#endif //BREEP_NETWORK_TCP_HPP
