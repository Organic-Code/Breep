///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

inline const boost::uuids::uuid& peer::id() const noexcept {
	return m_id;
}

inline const boost::asio::ip::address& peer::address() const noexcept {
	return m_address;
}

inline const bool peer::operator==(const peer& lhs) const {
	return this->m_address == lhs.m_address && this->m_id == lhs.m_id;
}

inline const bool peer::operator!=(const peer& lhs) const {
	return this->m_address != lhs.m_address || this->m_id != lhs.m_id;
}