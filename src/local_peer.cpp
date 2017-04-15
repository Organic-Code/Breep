

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


#include "local_peer.hpp"

using namespace breep;

inline peer& local_peer::path_to(const peer& p) {
	return m_path_to_passing_by.at(p.id());
}

inline const peer& local_peer::path_to(const peer& p) const {
	return m_path_to_passing_by.at(p.id());
}

inline std::unordered_map<boost::uuids::uuid, breep::peer, boost::hash<boost::uuids::uuid>>& local_peer::path_to_passing_by() noexcept {
	return m_path_to_passing_by;
}

inline const std::unordered_map<boost::uuids::uuid, peer, boost::hash<boost::uuids::uuid>>& breep::local_peer::path_to_passing_by() const noexcept {
	return m_path_to_passing_by;
}
