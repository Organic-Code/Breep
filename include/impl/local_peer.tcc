///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "local_peer.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

template <typename T>
inline const breep::peer<T>*& breep::local_peer<T>::path_to(const peer<T>& p) {
	return m_path_to_passing_by.at(p.id());
}

template <typename T>
inline breep::peer<T> const * const & breep::local_peer<T>::path_to(const peer<T>& p) const {
	return m_path_to_passing_by.at(p.id());
}

template <typename T>
inline std::unordered_map<boost::uuids::uuid, const breep::peer<T>*, boost::hash<boost::uuids::uuid>>& breep::local_peer<T>::path_to_passing_by() noexcept {
	return m_path_to_passing_by;
}

template <typename T>
inline const std::unordered_map<boost::uuids::uuid, const breep::peer<T>*, boost::hash<boost::uuids::uuid>>& breep::local_peer<T>::path_to_passing_by() const noexcept {
	return m_path_to_passing_by;
}

template <typename T>
inline std::unordered_map<boost::uuids::uuid, std::vector<const breep::peer<T>*>, boost::hash<boost::uuids::uuid>>& breep::local_peer<T>::bridging_from_to() noexcept {
	return m_bridging_from_to;
}

template <typename T>
const std::unordered_map<boost::uuids::uuid, std::vector<const breep::peer<T>*>, boost::hash<boost::uuids::uuid>>& breep::local_peer<T>::bridging_from_to() const noexcept {
	return m_bridging_from_to;
}