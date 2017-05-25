#ifndef BREEP_UTIL_SERIALIZER_HPP
#define BREEP_UTIL_SERIALIZER_HPP

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
 * @file serializer.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <sstream>

namespace breep {
	class serializer {

	public:
		serializer(): m_os() {}

		std::basic_string<uint8_t> str() {
			m_os.flush();
			return m_os.str();
		}

	private:
		std::basic_ostringstream<uint8_t> m_os;

		friend serializer& operator<<(serializer&, bool);
		friend serializer& operator<<(serializer&, uint8_t);
		friend serializer& operator<<(serializer&, uint16_t);
		friend serializer& operator<<(serializer&, uint32_t);
		friend serializer& operator<<(serializer&, uint64_t);
		friend serializer& operator<<(serializer&, int8_t);
		friend serializer& operator<<(serializer&, int16_t);
		friend serializer& operator<<(serializer&, int32_t);
		friend serializer& operator<<(serializer&, int64_t);
		friend serializer& operator<<(serializer&, float);
		friend serializer& operator<<(serializer&, double);
	};
}

#include "impl/serializer.tcc"

#endif //BREEP_UTIL_SERIALIZER_HPP
