#ifndef BREEP_INVALID_STATE_HPP
#define BREEP_INVALID_STATE_HPP


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
 * @file invalid_state.hpp
 * @author Lucas Lazare
 */

#include <stdexcept>

namespace breep {

	/**
	 * @class invalid_state invalid_state.hpp
	 * @brief exception thrown whenever a method is called when it shouldn't be
	 *
	 * @details thrown by \em breep::network::connect, \em breep::network::connect_sync
	 *
	 * @since 0.1.0
	 */
	class invalid_state: public std::runtime_error {

	public:
		explicit invalid_state(const std::string& what_arg): runtime_error(what_arg) {}
		explicit invalid_state(const char* what_arg): runtime_error(what_arg) {}
	};
}

#endif //BREEP_INVALID_STATE_HPP
