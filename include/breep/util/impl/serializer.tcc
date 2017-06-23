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
#include <bitset>


#include "breep/util/tuple_iter.hpp"
#include "../serializer.hpp" // Allows my IDE to work

namespace breep { namespace detail {
	// The code from the following method was adapted from the public domain
	template <typename ReturnType, typename FloatType, unsigned int ExponentBits, unsigned int MantissaBits>
	ReturnType toIEEE(FloatType value) {

		switch (std::fpclassify(value)) {
			case FP_INFINITE: {
				ReturnType exponentMax(0);
				for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
					exponentMax = (exponentMax << 1) + 1;
				}
				if (value > 0) {
					return exponentMax << MantissaBits;
				} else {
					return (ReturnType(1) << (MantissaBits + ExponentBits)) | (exponentMax << MantissaBits);
				}
			}
			case FP_NAN: {
				ReturnType exponentMax(0);
				for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
					exponentMax = (exponentMax << 1) + 1;
				}
				return (exponentMax << MantissaBits) | 1;
			}
			case FP_NORMAL:
			case FP_SUBNORMAL: {
				FloatType fnorm;
				ReturnType shift;
				ReturnType sign, exp, mantissa;

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
                    for (uint_fast8_t i = std::numeric_limits<ReturnType>::digits ; --i ;) {
                        while(fnorm >= FloatType(ReturnType(1) << i)) {
                            fnorm /= FloatType(ReturnType(1) << i);
                            shift += i;
                        }
                    }
				} else {
                    for (uint_fast8_t i = std::numeric_limits<ReturnType>::digits ; i-- ;) {
                        while(fnorm < FloatType(1) / FloatType(ReturnType(1) << i)) {
                            fnorm *= FloatType(ReturnType(2) << i);
                            shift -= (i + 1);
                        }
                    }
				}
				fnorm = fnorm - FloatType(1.);

				mantissa = static_cast<ReturnType>(fnorm * ((ReturnType(1) << MantissaBits) + FloatType(.5)));

				exp = static_cast<ReturnType>(shift + ((ReturnType(1) << (ExponentBits - 1)) - 1));

				return static_cast<ReturnType>((sign << (MantissaBits + ExponentBits)) | (exp << MantissaBits) | mantissa);
			}
			case FP_ZERO:
			default: {
				return std::signbit(value) ? ReturnType(1) << (ExponentBits + MantissaBits) : 0;
			}
		}
	}
}}

namespace breep {

	template <typename SizeType>
	void write_size(serializer& s, SizeType size) {
		static_assert(std::is_fundamental<SizeType>::value && std::is_integral<SizeType>::value, "SizeType must be an integer.");

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

	serializer& operator<<(serializer& s, uint8_t val) {
		s.m_os.put(val);
		return s;
	}

	serializer& operator<<(serializer& s, bool val) {
		s << (val ? static_cast<uint8_t>('1') : static_cast<uint8_t>('0'));
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
		s << detail::toIEEE<int32_t, float, 8, 23>(val);
		return s;
	}

	serializer& operator<<(serializer& s, double val) {
		s << detail::toIEEE<int64_t, double, 11, 52>(val);
		return s;
	}

	template <typename IterableContainer>
	serializer& operator<<(serializer& s, const IterableContainer& val) {
		write_size(s, val.size());
		for (const auto& current_value : val) {
			s << current_value;
		}
		return s;
	}

    serializer& operator<<(serializer& s, const std::vector<bool>& vect) {
        write_size(s, vect.size());
        for (std::vector<bool>::size_type i = 0 ; i < vect.size() ; i += std::numeric_limits<uint8_t>::digits) {
            uint8_t mask = 0;
            for (uint_fast8_t j = 0 ; j < std::numeric_limits<uint8_t>::digits && i+j < vect.size() ; ++j) {
                mask = static_cast<uint8_t>(mask | (vect[i + j] ? 1 << j : 0));
            }
            s << mask;
        }
        return s;
    }

	// Who doesn't love O(n*n) ?
	template <typename T>
	serializer& operator<<(serializer& s, const std::forward_list<T>& forward_list) {
		uint64_t size = 0;
		for (auto it = forward_list.begin(), end = forward_list.end() ; it != end ; ++it, ++size);
		write_size(s, size);
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

	template <typename... T>
	serializer& operator<<(serializer& s, const std::tuple<T...>& values) {
        for_each(values, [&s](auto&& val) -> void {
                    s << val;
                });
		return s;
	}
}
