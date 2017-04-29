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

#include "detail/utils.hpp" // TODO: remove

template <typename Container, typename OutputIterator>
inline void breep::detail::little_endian(const Container& container, OutputIterator outputIterator) {
	using std::iterator_traits;
	static_assert(sizeof(typename Container::value_type) == 1);
	static_assert(sizeof(typename OutputIterator::container_type::value_type) == 1);
#ifdef BOOST_BIG_ENDIAN
	size_t max = container.size() - container.size() % sizeof(uintmax_t);
	for(size_t i{0} ; i < max ; i += sizeof(uintmax_t)) {
		for (uint_fast8_t j{sizeof(uintmax_t)} ; j-- ;) {
			*out++ = data[i + j];
		}
	}
	if (max < container.size()) {
		for (uint_fast8_t j{static_cast<uint_fast8_t>(max + sizeof(uintmax_t) - container.size() + 1)} ; --j ;) {
			*out++ = 0;
		}
		for (uint_fast8_t i{static_cast<uint_fast8_t>(data.size() - max)} ; i-- ;) {
			*out++ = data[max + i];
		}
	}
#elif defined BOOST_LITTLE_ENDIAN
	std::copy(container.cbegin(), container.cend(), outputIterator);
#else
#error "Unknown endianness (if endianness is known, please manually define BOOST_LITTLE_ENDIAN or BOOST_BIG_ENDIAN)."
#error "Middle endian is not supported."
#endif
};