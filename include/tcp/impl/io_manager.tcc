///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


#include "tcp/io_manager.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp> // boost::uuids::to_string
#include <boost/array.hpp>
#include <boost/uuid/string_generator.hpp>
#include <vector>
#include <limits>
#include <memory>
#include <string>

#include "detail/utils.hpp"
#include "network_manager.hpp"
#include "peer.hpp"
#include "exceptions.hpp"

template <unsigned int T>
template <typename data_container>
inline void breep::tcp::io_manager<T>::send(commands command, const data_container& data, const peernm& peer) const {
	send(command, data.cbegin(), data.size(), peer);
}

template <unsigned int T>
template <typename data_iterator, typename size_type>
void breep::tcp::io_manager<T>::send(commands command, data_iterator it, size_type size, const peernm& peer) const {

	std::vector<uint8_t> buff;
	buff.reserve(2 + size + size / std::numeric_limits<uint8_t>::max());

	buff.push_back(static_cast<uint8_t>(command));
	size_type current_index{0};
	while (current_index < size) {
		if (size - current_index > std::numeric_limits<uint8_t>::max()) {
			buff.push_back(0);
			for (uint8_t i = std::numeric_limits<uint8_t>::max() ; i-- ; ++current_index) {
				buff.push_back(*it++);
			}
		} else {
			buff.push_back(static_cast<uint8_t>(size - current_index));
			while (current_index++ < size) {
				buff.push_back(*it++);
			}
		}
	}

	m_io_service.post(
			[this, peer, local_buffer{std::move(buff)}] {
				std::queue<std::vector<uint8_t>>& buffers = m_data_queues.at(peer.id());
				bool being_lazy = buffers.empty();
				buffers.push(std::move(local_buffer));
				if (being_lazy) {
					write(peer);
				}
			}
	);
}

template <unsigned int T>
breep::peer<breep::tcp::io_manager<T>> breep::tcp::io_manager<T>::connect(const boost::asio::ip::address& address, unsigned short port) {
	boost::asio::ip::tcp::resolver resolver(m_io_service);
	auto endpoint_iterator = resolver.resolve({address.to_string(),std::to_string(port)});

	std::shared_ptr<boost::asio::ip::tcp::socket> socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	boost::asio::connect(*socket, endpoint_iterator);
	boost::asio::write(
			*socket,
			boost::asio::buffer(m_id_string_bigendian)
	);
	boost::system::error_code error;
	boost::array<uint8_t, 512> buffer;
	size_t len = socket->read_some(boost::asio::buffer(buffer), error);

	if (error) {
		return constant::bad_peer<io_manager<T>>;
	}

	size_t expected_length = buffer[0];
	std::vector<uint8_t> input{};
	input.reserve(expected_length);
	std::copy(buffer.cbegin() + 1, buffer.cbegin() + len, std::back_inserter(input));
	--len;

	while (len < expected_length) {
		expected_length -= len;
		len = socket->read_some(boost::asio::buffer(buffer), error);
		if (error) {
			return constant::bad_peer<io_manager<T>>;
		}
		std::copy(buffer.cbegin(), buffer.cbegin() + len, std::back_inserter(input));
	}

	return peernm(
			boost::uuids::string_generator{}(breep::detail::littleendian2<std::string>(input)),
			boost::asio::ip::address(address),
			std::move(socket)
	);
}

template <unsigned int T>
void breep::tcp::io_manager<T>::process_connected_peer(peernm& peer) {
	m_data_queues.insert(std::make_pair(peer.id(), std::queue<std::vector<uint8_t>>()));
	peer.m_socket->async_read_some(
		boost::asio::buffer(*peer.m_fixed_buffer),
		boost::bind(&io_manager<T>::process_read, this, peer, _1, _2)
	);
}

template <unsigned int T>
inline void breep::tcp::io_manager<T>::disconnect() {
	m_io_service.stop();
	for (auto it = m_owner->peers().begin(), end = m_owner->peers().end() ; it != end ; ++it) {
		it->second.m_socket.reset();
		it->second.m_last_received_command = commands::null_command;
		it->second.m_dynamic_buffer->clear();
	}
	m_io_service.reset();
}

template <unsigned int T>
inline void breep::tcp::io_manager<T>::run() {
	m_io_service.run();
}

/* PRIVATE */

template <unsigned int T>
inline void breep::tcp::io_manager<T>::owner(network_manager<io_manager<T>>* owner) {
	if (m_owner == nullptr) {
		m_owner = owner;
		m_id_string_bigendian = boost::uuids::to_string(m_owner->self().id());
		m_id_string_bigendian = breep::detail::littleendian2<std::string>(m_id_string_bigendian);
		m_id_string_bigendian.insert(m_id_string_bigendian.begin(), static_cast<uint8_t>(m_id_string_bigendian.size()));
		m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));

		boost::asio::ip::v6_only v6_only;
		m_acceptor.get_option(v6_only);
		if (v6_only) {
			if (m_acceptor_v4 != nullptr) {
				delete m_acceptor_v4;
			}
			m_acceptor_v4 = new boost::asio::ip::tcp::acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_acceptor.local_endpoint().port()));
			m_acceptor_v4->async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
		}
	} else {
		throw invalid_state("Tried to set an already set owner. This object shouldn't be shared.");
	}
}

template <unsigned int T>
void breep::tcp::io_manager<T>::process_read(peernm& peer, boost::system::error_code error, std::size_t read) {

	if (!error) {
		std::vector<uint8_t>& dyn_buff = *peer.m_dynamic_buffer;
		std::array<uint8_t, T>& fixed_buff = *peer.m_fixed_buffer;

		typename std::array<uint8_t, T>::size_type current_index{0};
		if (peer.m_last_received_command == commands::null_command) {
			peer.m_last_received_command = static_cast<commands>(fixed_buff[current_index++]);
			if (read == 1) {
				peer.m_socket->async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
				        boost::bind(&io_manager<T>::process_read, this, peer, _1, _2)
				);
				return;
			}
		}

		bool has_work = true;
		std::size_t max_idx = std::max(read, fixed_buff.size());
		do {
			uint8_t to_be_red = fixed_buff[current_index++];

			if (to_be_red) {

				if (to_be_red + current_index <= max_idx) {
				// We can reach the end of the packet.
					while (to_be_red--) {
						dyn_buff.push_back(fixed_buff[current_index++]);
					}
					detail::network_attorney_client<io_manager <T>>::data_received(*m_owner, peer, peer.m_last_received_command, dyn_buff);

					dyn_buff.clear();
					peer.m_last_received_command = commands::null_command;
					peer.m_socket->async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
						boost::bind(&io_manager<T>::process_read, this, peer, _1, _2)
					);
					has_work = false;

				} else {
				// We still have to wait for some more input
					uint8_t count{0};
					--current_index;
					// shifting the buffer so we can use the end of it to store the up coming bits
					while (to_be_red--) {
						fixed_buff[count++] = fixed_buff[current_index++];
					}

					peer.m_socket->async_read_some(
						boost::asio::buffer(fixed_buff.data() + count, fixed_buff.size() - count),
						boost::bind(&io_manager::process_read, this, peer, _1, _2)
					);
					has_work = false;
				}
			} else {
			// to_be_red == 0 <=> more than std::numeric_limits octets to process.
				to_be_red = std::numeric_limits<uint8_t>::max();
				if (to_be_red + current_index <= max_idx) {
				// We can reach the end of the sub packet
					while (to_be_red--) {
						dyn_buff.push_back(fixed_buff[current_index++]);
					}
				} else {
				// We still have to wait for some more input
					uint8_t count{0};
					--current_index;
					// shifting the buffer so we can use the end of it to store the up coming bits
					while (to_be_red--) {
						fixed_buff[count++] = fixed_buff[current_index++];
					}

					peer.m_socket->async_read_some(
						boost::asio::buffer(fixed_buff.data() + count, fixed_buff.size() - count),
						boost::bind(&io_manager<T>::process_read, this, peer, _1, _2)
					);
					has_work = false;
				}
			}
		} while (has_work);
	} else {
		// error
		peer.m_socket = std::shared_ptr<socket_type>(nullptr);
		detail::network_attorney_client<io_manager<T>>::peer_disconnected(*m_owner, peer);
	}
}


template <unsigned int T>
inline void breep::tcp::io_manager<T>::write(const peernm& peer) const {
	auto& buffers = m_data_queues.at(peer.id());
	boost::asio::async_write(
			*peer.m_socket,
	        boost::asio::buffer(buffers.front().data(), buffers.front().size()),
	        boost::bind(&io_manager<T>::write_done, this, peer)
	);
}

template <unsigned int T>
inline void breep::tcp::io_manager<T>::write_done(const peernm& peer) const {
	auto& buffers = m_data_queues.at(peer.id());
	buffers.pop();
	if (!buffers.empty()) {
		write(peer);
	}
}

template <unsigned int T>
inline void breep::tcp::io_manager<T>::accept(boost::system::error_code ec) {
	if (!ec) {
		boost::array<uint8_t, 512> buffer;
		size_t len = m_socket->read_some(boost::asio::buffer(buffer), ec);

		if (ec) {
			m_socket->close();
		} else {
			// Reading the id
			std::vector<uint8_t> data = detail::littleendian1(buffer);
			size_t expected_length = buffer[0];
			std::vector<uint8_t> peer_id{};
			peer_id.reserve(expected_length);
			std::copy(buffer.cbegin() + 1, buffer.cbegin() + len, std::back_inserter(peer_id));
			--len;

			while (len < expected_length) {
				expected_length -= len;
				len = m_socket->read_some(boost::asio::buffer(buffer), ec);
				if (ec) {
					m_socket->close();
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					return;
				}
				std::copy(buffer.cbegin(), buffer.cbegin() + len, std::back_inserter(peer_id));
			}

			// Sending our id
			boost::asio::write(
					*m_socket,
			        boost::asio::buffer(m_id_string_bigendian)
			);

			detail::network_attorney_client<io_manager<T>>::peer_connected(
					*m_owner,
					peernm(
						boost::uuids::string_generator{}(breep::detail::littleendian2<std::string>(peer_id)),
						boost::asio::ip::address(m_socket->remote_endpoint().address()),
						m_socket
					)
			);
		}
		// reset the socket.
		m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	}
	m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
}