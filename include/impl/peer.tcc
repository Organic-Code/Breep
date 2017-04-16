///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "peer.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

template <typename T>
inline const boost::uuids::uuid& breep::peer<T>::id() const noexcept {
	return m_id;
}

template <typename T>
inline const boost::asio::ip::address& breep::peer<T>::address() const noexcept {
	return m_address;
}

template <typename T>
inline bool breep::peer<T>::operator==(const peer<T>& lhs) const {
	return this->m_address == lhs.m_address && this->m_id == lhs.m_id;
}

template <typename T>
inline bool breep::peer<T>::operator!=(const peer<T>& lhs) const {
	return this->m_address != lhs.m_address || this->m_id != lhs.m_id;
}