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


template <unsigned int BUFFER_LENGTH>
breep::tcp::io_manager<BUFFER_LENGTH>::io_manager(unsigned short port)
		: m_owner(nullptr)
		, m_io_service{}
		, m_acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port))
		, m_acceptor_v4(nullptr)
		, m_socket{std::make_shared<boost::asio::ip::tcp::socket>(m_io_service)}
		, m_id_packet()
		, m_data_queues()
{
	static_assert(BUFFER_LENGTH > std::numeric_limits<uint8_t>::max(), "The buffer size is too small");

	boost::system::error_code ec;
	m_acceptor.set_option(boost::asio::ip::v6_only(false), ec);
	if (ec) {
		std::clog << "IP dual stack is unsupported on your system.\n"; // todo: set an easy way to programmatically get this error
		std::clog << "Adding ipv4 listener.\n\n";
	}
}


template <unsigned int BUFFER_LENGTH>
breep::tcp::io_manager<BUFFER_LENGTH>::io_manager(io_manager<BUFFER_LENGTH>&& other)
		: m_owner(other.m_owner)
		, m_io_service()
		, m_acceptor(std::move(other.m_acceptor))
		, m_acceptor_v4(nullptr)
		, m_socket(std::make_shared<boost::asio::ip::tcp::socket>(m_io_service))
		, m_id_packet(std::move(other.m_id_packet))
		, m_data_queues(std::move(other.m_data_queues))
{
	other.m_socket->close();
	other.m_io_service.stop();
	unsigned short port = m_acceptor.local_endpoint().port();
	m_acceptor.close();

	m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)};

	if (m_owner != nullptr) {
		m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<BUFFER_LENGTH>::accept, this, _1));
	}
}

template <unsigned int T>
breep::tcp::io_manager<T>::~io_manager() {
	m_acceptor.close();
	m_socket->close();
	m_io_service.stop();
	if (m_acceptor_v4 != nullptr) {
		m_acceptor_v4->close();
		delete m_acceptor_v4;
	}
}

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
	std::vector<uint8_t> io_protocol;
	io_protocol.reserve(8);
	detail::insert_uint32(io_protocol, IO_PROTOCOL_ID_1);
	detail::insert_uint32(io_protocol, IO_PROTOCOL_ID_2);

	boost::asio::ip::tcp::resolver resolver(m_io_service);
	auto endpoint_iterator = resolver.resolve({address.to_string(),std::to_string(port)});
	std::shared_ptr<boost::asio::ip::tcp::socket> socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	boost::asio::connect(*socket, endpoint_iterator);

	boost::asio::write(*socket, boost::asio::buffer(io_protocol));
	std::array<uint8_t, 128> buffer;
	boost::system::error_code error;
	size_t len = socket->read_some(boost::asio::buffer(buffer), error);
	if (error || len != 8) {
		return constant::bad_peer<io_manager<T>>;
	}
	while (len--) {
		if (buffer[len] != io_protocol[len]) {
			return constant::bad_peer<io_manager<T>>;
		}
	}

	boost::asio::write(*socket, boost::asio::buffer(m_id_packet));
	len = 0;
	do {
		len += socket->read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), error);
		if (error) {
			return constant::bad_peer<io_manager<T>>;
		}
	} while (len <= buffer[0]);

	std::string input;
	input.reserve(len - 1 - buffer[1]);
	little_endian(detail::unowning_linear_container(buffer.data() + 2, len - 2 - buffer[1]),
	              std::back_inserter(input));

	return peernm(
			boost::uuids::string_generator{}(input),
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

		std::string id_str(boost::uuids::to_string(m_owner->self().id()));
		m_id_packet.clear();
		m_id_packet.push_back(0);
		detail::make_little_endian(id_str, m_id_packet);
		m_id_packet[0] = static_cast<char>(m_id_packet.size() - 1);

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
					while (fixed_buff.size() > current_index) {
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
					while (fixed_buff.size() > current_index) {
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
		boost::array<uint8_t, 128> buffer;
		size_t len = m_socket->read_some(boost::asio::buffer(buffer), ec);

		if (ec) {
			m_socket->close();
		} else {
			std::vector<uint8_t> protocol_id;
			protocol_id.reserve(8);
			detail::insert_uint32(protocol_id, IO_PROTOCOL_ID_1);
			detail::insert_uint32(protocol_id, IO_PROTOCOL_ID_2);
			m_socket->write_some(boost::asio::buffer(protocol_id), ec);

			// Reading the protocol ID
			if (len != 8) {
				m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
				m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
				return;
			}
			while(len--) {
				if (buffer[len] != protocol_id[len]) {
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
					return;
				}
			}

			// Reading the id
			len = 0;
			do {
				len += m_socket->read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), ec);
				if (ec) {
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
					return;
				}
			} while (len <= buffer[0]);

			std::string input;
			input.reserve(len - 1 - buffer[1]);
			little_endian(detail::unowning_linear_container(buffer.data() + 2, len - 2 - buffer[1]),
			              std::back_inserter(input));

			boost::asio::write(*m_socket, boost::asio::buffer(m_id_packet));

			detail::network_attorney_client<io_manager<T>>::peer_connected(
					*m_owner,
					peernm(
						boost::uuids::string_generator{}(input),
						boost::asio::ip::address(m_socket->remote_endpoint().address()),
						m_socket
					)
			);
		}
	}
	// reset the socket.
	m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<T>::accept, this, _1));
}