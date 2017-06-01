#ifndef BREEP_UTIL_DESERIALIZER_HPP
#define BREEP_UTIL_DESERIALIZER_HPP

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
 * @file derializer.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <istream>
#include <forward_list>
#include <vector>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <stack>
#include <queue>

namespace breep {
	class deserializer {

	public:
		explicit deserializer(const std::basic_string<uint8_t>& bs): m_is(bs) {}

		bool empty() const {
			return m_is.rdbuf()->in_avail() == 0;
		}

	private:
		std::basic_istringstream<uint8_t, std::char_traits<uint8_t>> m_is;

		// deserializing fundamental uint8_t
		friend deserializer& operator>>(deserializer&, uint8_t&);
	};

	/**
	 * If you wish to read the size of your custom container, which was written
	 * using breep::write_size(serializer&,SizeType), use this method.
	 */
	void read_size(deserializer& s, uint64_t& size);

	// deserializing fundamental types
	deserializer& operator>>(deserializer&, bool&);
	deserializer& operator>>(deserializer&, char&);
	deserializer& operator>>(deserializer&, uint16_t&);
	deserializer& operator>>(deserializer&, uint32_t&);
	deserializer& operator>>(deserializer&, uint64_t&);
	deserializer& operator>>(deserializer&, int8_t&);
	deserializer& operator>>(deserializer&, int16_t&);
	deserializer& operator>>(deserializer&, int32_t&);
	deserializer& operator>>(deserializer&, int64_t&);
	deserializer& operator>>(deserializer&, float&);
	deserializer& operator>>(deserializer&, double&);


	// generic method deserializing containers that support .push_back(value_type)
	template <typename PushableContainer>
	deserializer& operator>>(deserializer&, PushableContainer&);

	// STL containers
	template <typename T>
	deserializer& operator>>(deserializer&, std::forward_list<T>&);

	template <typename T>
	deserializer& operator>>(deserializer&, std::vector<T>&);

	template <typename T, std::size_t N>
	deserializer& operator>>(deserializer&, std::array<T, N>&);

	template <typename T>
	deserializer& operator>>(deserializer&, std::deque<T>&);

	template <typename T>
	deserializer& operator>>(deserializer&, std::list<T>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::map<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::multimap<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer&, std::unordered_map<T,U,V,W,X>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer&, std::unordered_multimap<T,U,V,W,X>&);

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer&, std::set<T,U,V>&);


	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer&, std::multiset<T,U,V>&);


	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::unordered_set<T,U,V,W>&);


	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::unordered_multiset<T,U,V,W>&);

	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::stack<T,U>&);

	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::queue<T,U>&);

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer&, std::priority_queue<T,U,V>&);

	// deserializing std::pair
	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::pair<T,U>&);

	// deserializing std::tuple with up to 7 elements
	template <typename T>
	deserializer& operator>>(deserializer&, std::tuple<T>&);

	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::tuple<T, U>&);

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer&, std::tuple<T, U, V>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X,Y>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X,Y>&);

	// deserializing durations
	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::chrono::duration<T,U>&);
}

#include "impl/deserializer.tcc"

#endif //BREEP_UTIL_DESERIALIZER_HPP
