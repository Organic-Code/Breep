#ifndef BREEP_UTILS_HPP
#define BREEP_UTILS_HPP

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
 * @file utils.hpp
 * @author Lucas Lazare
 */


namespace breep::detail {
	/**
	 * @brief Converts from local endianness to big endian and the reverse.
	 * @tparam data_container Using operator[](int) and size() methods, and ::type for underlying type (if no output container is given).
	 */
	template<typename data_container, typename output_container = std::vector<data_container::value_type>>
	output_container to_bigendian1(const data_container& data);

	template<typename output_container, typename data_container>
	inline output_container to_bigendian2(const data_container& data) {
		return to_bigendian1<data_container, output_container>(data);
	};

	template <typename container>
	inline container to_bigendian3(const container& data) {
		return to_bigendian1<container, container>(data);
	}
}

#endif //BREEP_UTILS_HPP
