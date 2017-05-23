///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "breep/network/basic_peer.hpp" // allows my IDE to work

/**
 * @file basic_peer.tcc
 * @author Lucas Lazare
 * @since 0.1.0
 */

template <typename T>
inline const boost::uuids::uuid& breep::basic_peer<T>::id() const noexcept {
	return m_id;
}

template <typename T>
inline const boost::asio::ip::address& breep::basic_peer<T>::address() const noexcept {
	return m_address;
}

template <typename T>
inline bool breep::basic_peer<T>::operator==(const basic_peer<T>& lhs) const {
	return this->m_address == lhs.m_address && this->m_id == lhs.m_id;
}

template <typename T>
inline bool breep::basic_peer<T>::operator!=(const basic_peer<T>& lhs) const {
	return this->m_address != lhs.m_address || this->m_id != lhs.m_id;
}