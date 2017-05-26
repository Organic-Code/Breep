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
 * @file serializer.cpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>

#include "../serializer.hpp" // Allows my IDE to work

namespace breep { namespace detail {
	template <typename ReturnType, typename FloatType, unsigned int ExponentBits, unsigned int MantissaBits>
	ReturnType toIEEE(FloatType value) {

		// TODO: check for infinity and NaN

		FloatType fnorm;
		ReturnType shift;
		ReturnType sign, exp, mantissa;

		if (value == FloatType(0.)) { // special value
			return 0;
		} else if (value == FloatType(-0.)) { // special value
			return ReturnType(1) << (ExponentBits + MantissaBits);
		}

		if (value < 0) {
			sign = 1;
			fnorm = -value;
		} else {
			sign = 0;
			fnorm = value;
		}

		// get the normalized form of f and track the exponent
		shift = 0;

		if (fnorm >= FloatType(2.)) {
			while (fnorm >= FloatType(2.)) {
				fnorm /= FloatType(2.);
				shift++;
			}
		} else {
			while (fnorm < FloatType(1.)) {
				fnorm *= FloatType(2.);
				shift--;
			}
		}
		fnorm = fnorm - FloatType(1.);

		mantissa = static_cast<ReturnType>(fnorm * ((ReturnType(1) << MantissaBits) + FloatType(.5)));

		exp = static_cast<ReturnType>(shift + ((ReturnType(1) << (ExponentBits - 1)) - 1));

		return static_cast<ReturnType>((sign << (MantissaBits + ExponentBits)) | (exp << MantissaBits) | mantissa);
	}

	template <typename SizeType>
	void write_size(serializer& s, SizeType size) {
		uint_fast8_t bits_to_write = 0;
		while (size >> bits_to_write) {
			++bits_to_write;
		}
		uint_fast8_t oct_to_write = static_cast<uint_fast8_t>(bits_to_write / 8 + (bits_to_write % 8 == 0 ? 0 : 1));
		s << static_cast<uint8_t>(oct_to_write);
		while (oct_to_write--) {
			s << static_cast<uint8_t>(size >> (oct_to_write * 8));
		}
	}
}}

namespace breep {

	serializer& operator<<(serializer& s, uint8_t val) {
		s.m_os.put(val);
		return s;
	}

	serializer& operator<<(serializer& s, bool val) {
		s << (val ? '1' : '0');
		return s;
	}

	serializer& operator<<(serializer& s, char val) {
		if (std::numeric_limits<char>::min() < 0) {
			unsigned char unsigned_value = static_cast<unsigned char>(val);
			s << unsigned_value;
		} else {
			signed char signed_value = static_cast<signed char>(val);
			s << signed_value;
		}
		return s;
	}

	serializer& operator<<(serializer& s, uint16_t val) {
		s << static_cast<uint8_t>(val >> 8);
		s << static_cast<uint8_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, uint32_t val) {
		s << static_cast<uint16_t>(val >> 16);
		s << static_cast<uint16_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, uint64_t val) {
		s << static_cast<uint32_t>(val >> 32);
		s << static_cast<uint32_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int8_t val) {
		s << static_cast<uint8_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int16_t val) {
		s << static_cast<uint16_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int32_t val) {
		s << static_cast<uint32_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int64_t val) {
		s << static_cast<uint64_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, float val) {
		s << detail::toIEEE<uint32_t, float, 8, 23>(val);
		return s;
	}

	serializer& operator<<(serializer& s, double val) {
		s << detail::toIEEE<uint64_t, double, 11, 52>(val);
		return s;
	}

	template <typename IterableContainer>
	serializer& operator<<(serializer& s, const IterableContainer& val) {
		detail::write_size(s, val.size());
		for (const auto& current_value : val) {
			s << current_value;
		}
		return s;
	}

	// Who doesn't love O(n*n) ?
	template <typename T>
	serializer& operator<<(serializer& s, const std::forward_list<T>& forward_list) {
		uint64_t size = 0;
		for (auto it = forward_list.begin(), end = forward_list.end() ; it != end ; ++it, ++size);
		detail::write_size(s, size);
		for (const T& current_value : forward_list) {
			s << current_value;
		}
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::pair<T,U>& val) {
		s << val.first << val.second;
		return s;
	}

	template <typename T>
	serializer& operator<<(serializer& s, const std::tuple<T>& val) {
		s << std::get<0>(val);
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::tuple<T, U>& val) {
		s << std::get<0>(val) << std::get<1>(val);
		return s;
	}

	template <typename T, typename U, typename V>
	serializer& operator<<(serializer& s, const std::tuple<T, U, V>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X, typename Y>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X,Y>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X,Y>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val) << std::get<6>(val);
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::chrono::duration<T,U>& val) {
		s << static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(val).count());
		return s;
	}
}
