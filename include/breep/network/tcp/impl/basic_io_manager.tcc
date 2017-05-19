///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


#include "breep/network/tcp/basic_io_manager.hpp" // allows my IDE to work

#include <vector>
#include <queue>
#include <limits>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "breep/network/detail/utils.hpp"
#include "breep/network/basic_peer_manager.hpp"
#include "breep/network/basic_peer.hpp"
#include "breep/util/exceptions.hpp"


template <unsigned int BUFFER_LENGTH, unsigned long keep_alive_millis, unsigned long U, unsigned long timeout_chk_interval>
breep::tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::basic_io_manager(unsigned short port)
		: m_owner(nullptr)
		, m_io_service{}
		, m_acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port))
		, m_acceptor_v4(nullptr)
		, m_socket{std::make_shared<boost::asio::ip::tcp::socket>(m_io_service)}
		, m_id_packet()
		, m_timeout_dlt(m_io_service, boost::posix_time::millisec(timeout_chk_interval))
	    , m_keepalive_dlt(m_io_service, boost::posix_time::millisec(keep_alive_millis))
		, m_data_queues()
{
	static_assert(BUFFER_LENGTH > std::numeric_limits<uint8_t>::max(), "The buffer size is too small");

	m_timeout_dlt.async_wait(boost::bind(&tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::timeout_impl, this));
	m_keepalive_dlt.async_wait(boost::bind(&tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::keep_alive_impl,this));

	boost::system::error_code ec;
	m_acceptor.set_option(boost::asio::ip::v6_only(false), ec);
	if (ec) {
		std::clog << "IP dual stack is unsupported on your system.\n";
		std::clog << "Adding ipv4 listener.\n\n";
	}

}


template <unsigned int BUFFER_LENGTH, unsigned long keep_alive_millis, unsigned long U, unsigned long timeout_chk_interval>
breep::tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::basic_io_manager(basic_io_manager<BUFFER_LENGTH>&& other)
		: m_owner(other.m_owner)
		, m_io_service()
		, m_acceptor(std::move(other.m_acceptor))
		, m_acceptor_v4(nullptr)
		, m_socket(std::make_shared<boost::asio::ip::tcp::socket>(m_io_service))
		, m_id_packet(std::move(other.m_id_packet))
		, m_timeout_dlt(m_io_service, boost::posix_time::millisec(timeout_chk_interval))
		, m_keepalive_dlt(m_io_service, boost::posix_time::millisec(keep_alive_millis))
		, m_data_queues(std::move(other.m_data_queues))
{
	other.m_socket->close();
	other.m_io_service.stop();
	unsigned short port = m_acceptor.local_endpoint().port();
	m_acceptor.close();

	m_timeout_dlt.async_wait(boost::bind(&tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::timeout_impl, this));
	m_keepalive_dlt.async_wait(boost::bind(&tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::keep_alive_impl,this));

	m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)};

	if (m_owner != nullptr) {
		m_acceptor.async_accept(*m_socket, boost::bind(&tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::accept, this, _1));
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
breep::tcp::basic_io_manager<T,U,V,W>::~basic_io_manager() {
	m_acceptor.close();
	m_socket->close();
	m_io_service.stop();
	if (m_acceptor_v4 != nullptr) {
		m_acceptor_v4->close();
		delete m_acceptor_v4;
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
template <typename data_container>
inline void breep::tcp::basic_io_manager<T,U,V,W>::send(commands command, const data_container& data, const peer& peer) const {
	send(command, data.cbegin(), data.size(), peer);
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
template <typename data_iterator, typename size_type>
void breep::tcp::basic_io_manager<T,U,V,W>::send(commands command, data_iterator it, size_type size, const peer& peer) const {

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

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
auto breep::tcp::basic_io_manager<T,U,V,W>::connect(const boost::asio::ip::address& address, unsigned short port) -> detail::optional<peer> {
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
		return detail::optional<peer>();
	}
	while (len--) {
		if (buffer[len] != io_protocol[len]) {
			return detail::optional<peer>();
		}
	}

	boost::asio::write(*socket, boost::asio::buffer(m_id_packet));
	len = 0;
	do {
		len += socket->read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), error);
		if (error) {
			return detail::optional<peer>();
		}
	} while (len <= buffer[0]);

	std::string input;
	detail::unmake_little_endian(detail::unowning_linear_container(buffer.data() + 3, len - 3), input);

	boost::uuids::uuid uuid;
	std::copy(input.data(), input.data() + input.size(), uuid.data);

	return detail::optional<peer>(peer(
			std::move(uuid),
			boost::asio::ip::address(address),
			static_cast<unsigned short>(buffer[1] << 8 | buffer[2]),
			data_type(std::move(socket))
	));
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::process_connected_peer(peer& peer) {
	m_data_queues.insert(std::make_pair(peer.id(), std::queue<std::vector<uint8_t>>()));
	peer.io_data.socket->async_read_some(
		boost::asio::buffer(*peer.io_data.fixed_buffer),
		boost::bind(&basic_io_manager<T>::process_read, this, peer, _1, _2)
	);
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::disconnect() {
	m_io_service.stop();
	m_io_service.reset();
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::run() {
	m_io_service.run();
}

/* PRIVATE */

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::owner(basic_peer_manager<basic_io_manager<T>>* owner) {
	if (m_owner == nullptr) {
		m_owner = owner;

		make_id_packet();

		m_acceptor.async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
		boost::asio::ip::v6_only v6_only;
		m_acceptor.get_option(v6_only);

		if (v6_only) {
			if (m_acceptor_v4 != nullptr) {
				delete m_acceptor_v4;
			}
			m_acceptor_v4 = new boost::asio::ip::tcp::acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_acceptor.local_endpoint().port()));
			m_acceptor_v4->async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
		}
	} else {
		throw invalid_state("Tried to set an already set owner. This object shouldn't be shared.");
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::process_read(peer& peer, boost::system::error_code error, std::size_t read) {

	if (!error) {
		peer.io_data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
		std::vector<uint8_t>& dyn_buff = *peer.io_data.dynamic_buffer;
		std::array<uint8_t, T>& fixed_buff = *peer.io_data.fixed_buffer;

		typename std::array<uint8_t, T>::size_type current_index{0};
		if (peer.io_data.last_command == commands::null_command) {
			peer.io_data.last_command = static_cast<commands>(fixed_buff[current_index++]);
			if (read == 1) {
				peer.io_data.socket->async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
				        boost::bind(&basic_io_manager<T>::process_read, this, peer, _1, _2)
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
					detail::peer_manager_attorney<tcp::basic_io_manager <T,U,V,W>>::data_received(*m_owner, peer, peer.io_data.last_command, dyn_buff);

					dyn_buff.clear();
					peer.io_data.last_command = commands::null_command;
					peer.io_data.socket->async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
						boost::bind(&basic_io_manager<T>::process_read, this, peer, _1, _2)
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

					peer.io_data.socket->async_read_some(
						boost::asio::buffer(fixed_buff.data() + count, fixed_buff.size() - count),
						boost::bind(&basic_io_manager::process_read, this, peer, _1, _2)
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

					peer.io_data.socket->async_read_some(
						boost::asio::buffer(fixed_buff.data() + count, fixed_buff.size() - count),
						boost::bind(&basic_io_manager<T>::process_read, this, peer, _1, _2)
					);
					has_work = false;
				}
			}
		} while (has_work);
	} else {
		// error
		peer.io_data.socket = std::shared_ptr<boost::asio::ip::tcp::socket>(nullptr);
		detail::peer_manager_attorney<tcp::basic_io_manager<T,U,V,W>>::peer_disconnected(*m_owner, peer);
	}
}


template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::write(const peer& peer) const {
	auto& buffers = m_data_queues.at(peer.id());
	boost::asio::async_write(
			*peer.io_data.socket,
	        boost::asio::buffer(buffers.front().data(), buffers.front().size()),
	        boost::bind(&basic_io_manager<T>::write_done, this, peer)
	);
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::write_done(const peer& peer) const {
	auto& buffers = m_data_queues.at(peer.id());
	buffers.pop();
	if (!buffers.empty()) {
		write(peer);
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
inline void breep::tcp::basic_io_manager<T,U,V,W>::accept(boost::system::error_code ec) {

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
				m_acceptor.async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
				return;
			}
			while(len--) {
				if (buffer[len] != protocol_id[len]) {
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
					return;
				}
			}

			// Reading the id
			len = 0;
			do {
				len += m_socket->read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), ec);
				if (ec) {
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
					return;
				}
			} while (len <= buffer[0]);

			std::string input;
			detail::unmake_little_endian(detail::unowning_linear_container(buffer.data() + 3, len - 3), input);
			boost::asio::write(*m_socket, boost::asio::buffer(m_id_packet));

			boost::uuids::uuid uuid;
			std::copy(input.data(), input.data() + input.size(), uuid.data);

			detail::peer_manager_attorney<tcp::basic_io_manager<T,U,V,W>>::peer_connected(
					*m_owner,
					peer(
						std::move(uuid),
						boost::asio::ip::address(m_socket->remote_endpoint().address()),
						static_cast<unsigned short>(buffer[1] << 8 | buffer[2]),
						data_type(m_socket)
					)
			);
		}
	}
	// reset the socket.
	m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	m_acceptor.async_accept(*m_socket, boost::bind(&basic_io_manager<T>::accept, this, _1));
}