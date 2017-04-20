///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <cstdint>
#include <boost/detail/endian.hpp>

#include "detail/utils.hpp"

template<typename data_container, typename output_container = std::vector<typename data_container::value_type>>
output_container bigendian1(const data_container& data) {
	static_assert(sizeof(typename data_container::value_type) == 1, "Converting endianness is possible only for 1-byte long types.");
#ifdef BOOST_LITTLE_ENDIAN
	output_container out;
	out.reserve(data.size() - data.size() % sizeof(uintmax_t) + sizeof(uintmax_t));
	size_t max = data.size() - data.size() % sizeof(uintmax_t);
	for(size_t i{0} ; i < max ; i += sizeof(uintmax_t)) {
		for (uint_fast8_t j{sizeof(uintmax_t)} ; j-- ;) {
			out.push_back(data[i + j]);
		}
	}
	if (max < data.size()) {
		for (uint_fast8_t j{static_cast<uint_fast8_t>(max + sizeof(uintmax_t) - data.size())} ; j-- ;) {
			out.push_back(0);
		}
		for (uint_fast8_t i{static_cast<uint_fast8_t>(data.size() - max)} ; i-- ;) {
			out.push_back(data[max + i]);
		}
	}
	return out;
#elif defined BOOST_BIG_ENDIAN
	return data;
#else
#error "Unknown endianness."
#endif
}
