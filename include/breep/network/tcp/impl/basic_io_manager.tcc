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


/**
 * @file basic_io_manager.tcc
 * @author Lucas Lazare
 * @since 0.1.0
 */


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

	m_timeout_dlt.async_wait(boost::bind(&io_manager::timeout_impl, this));
	m_keepalive_dlt.async_wait(boost::bind(&io_manager::keep_alive_impl,this));

	boost::system::error_code ec;
	m_acceptor.set_option(boost::asio::ip::v6_only(false), ec);
	if (ec) {
		breep::logger<io_manager>.debug("IP dual stack is unsupported on your system. Adding ipv4 listener.");
	}

}


template <unsigned int BUFFER_LENGTH, unsigned long keep_alive_millis, unsigned long U, unsigned long timeout_chk_interval>
breep::tcp::basic_io_manager<BUFFER_LENGTH,keep_alive_millis,U,timeout_chk_interval>::basic_io_manager(io_manager&& other) noexcept
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

	m_timeout_dlt.async_wait(boost::bind(&io_manager::timeout_impl, this));
	m_keepalive_dlt.async_wait(boost::bind(&io_manager::keep_alive_impl,this));

	m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)};

	if (m_owner != nullptr) {
		m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
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
void breep::tcp::basic_io_manager<T,U,V,W>::send(commands command, const data_container& data, const peer& target) const {
	send(command, data.cbegin(), data.size(), target);
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
template <typename data_iterator, typename size_type>
void breep::tcp::basic_io_manager<T,U,V,W>::send(commands command, data_iterator it, size_type size, const peer& target) const {

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
			[this, target, local_buffer{std::move(buff)}] () mutable {
				try {
					std::queue<std::vector<uint8_t>>& buffers = m_data_queues.at(target.id());
					bool being_lazy = buffers.empty();
					buffers.push(std::move(local_buffer));
					if (being_lazy) {
						write(target);
					}
				} catch (const std::out_of_range&) {
					breep::logger<io_manager>.warning("Peer " + target.id_as_string() + " disconnected unexpectedly while data was being sent");
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
	boost::asio::ip::tcp::socket socket(m_io_service);

	boost::system::error_code ec;
	boost::asio::connect(socket, endpoint_iterator, ec);
	if (ec) {
		return {};
	}

	boost::asio::write(socket, boost::asio::buffer(io_protocol));
	std::array<uint8_t, 128> buffer;
	boost::system::error_code error;
	size_t len = socket.read_some(boost::asio::buffer(buffer), error);
	if (error || len != 8) {
		breep::logger<io_manager>.warning("Target peer has not the same protocol ID format than us! (peer at "
		                                  + address.to_string() + "@" + std::to_string(port) + ").");
		return {};
	}
	while (len--) {
		if (buffer[len] != io_protocol[len]) {
			breep::logger<io_manager>.warning("Target peer has not the same io_manager protocol ID than us (["
			                                  + address.to_string() + "]:" + std::to_string(port) + ").");
			breep::logger<io_manager>.warning("Our protocol ID: " + std::to_string(IO_PROTOCOL_ID_1) + " " +
			                                  std::to_string(IO_PROTOCOL_ID_2) + ". Their protocol ID: "
			                                  + std::to_string(detail::read_uint32(buffer)) + " "
			                                  + std::to_string(detail::read_uint32(buffer, sizeof(uint32_t))) + ".");
			return {};
		}
	}

	boost::asio::write(socket, boost::asio::buffer(m_id_packet));
	len = 0;
	do {
		len += socket.read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), error);
		if (error) {
			return {};
		}
	} while (len <= buffer[0]);

	std::string input;
	detail::unmake_little_endian(detail::unowning_linear_container(buffer.data() + 3, len - 3), input);

	boost::uuids::uuid uuid;
	std::copy(input.data(), input.data() + input.size(), uuid.data);

	// TODO: can trap here
	std::underlying_type_t<commands> command[1];
	if (!socket.read_some(boost::asio::buffer(command, sizeof(command)), error) || error) {
		return detail::optional<peer>();
	}
	if (static_cast<commands>(command[0]) == commands::connection_refused) {
		breep::logger<io_manager>.info("Connection refused ([" + address.to_string() + "]:" + std::to_string(port) + ")");
		return {};
	}
	if (static_cast<commands>(command[0]) != commands::connection_accepted) {
		breep::logger<io_manager>.warning("Incompatible protocol, but protocol id match."
		                                  "(when connecting to [" + address.to_string() + "]:" + std::to_string(port) + ")");
		return {};
	}

	return detail::optional<peer>(peer(
			std::move(uuid),
			boost::asio::ip::address(address),
			static_cast<unsigned short>(buffer[1] << 8 | buffer[2]),
			std::make_shared<tcp::io_manager_data<T>>(std::move(socket))
	));
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::process_connected_peer(peer& connected) {
	m_data_queues.insert(std::make_pair(connected.id(), std::queue<std::vector<uint8_t>>()));

	if (connected.io_data->waiting_acceptance_answer) {
		std::underlying_type_t<commands> command[] = {
				static_cast<std::underlying_type_t<commands>>(commands::connection_accepted)
		};
		boost::asio::write(connected.io_data->socket, boost::asio::buffer(command, sizeof(command)));
	}

	connected.io_data->socket.async_read_some(
			boost::asio::buffer(connected.io_data->fixed_buffer),
			boost::bind(&io_manager::process_read, this, connected, _1, _2)
	);
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::process_connection_denial(basic_peer<io_manager>& peer) {

	if (peer.io_data->waiting_acceptance_answer) {
		std::underlying_type_t<commands> command[] = {
				static_cast<std::underlying_type_t<commands>>(commands::connection_refused)
		};
		boost::asio::write(peer.io_data->socket, boost::asio::buffer(command, sizeof(command)));
	}
}


template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::disconnect() {
	m_io_service.stop();
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::disconnect(peer& p) {
	boost::system::error_code error;
	p.io_data->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
	p.io_data->socket.close(error);
	p.io_data->dynamic_buffer.clear();
	p.io_data->dynamic_buffer.shrink_to_fit();
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::run() {
	m_io_service.reset();
	breep::logger<io_manager>.info("The network is now online.");
	m_io_service.run();
	breep::logger<io_manager>.info("The network is now offline.");
}

/* PRIVATE */

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::owner(basic_peer_manager<io_manager>* owner) {
	if (m_owner == nullptr) {
		m_owner = owner;

		make_id_packet();

		m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
		boost::asio::ip::v6_only v6_only;
		m_acceptor.get_option(v6_only);

		if (v6_only) {
			if (m_acceptor_v4 != nullptr) {
				delete m_acceptor_v4;
			}
			m_acceptor_v4 = new boost::asio::ip::tcp::acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_acceptor.local_endpoint().port()));
			m_acceptor_v4->async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
		}
	} else {
		throw invalid_state("Tried to set an already set owner. This object shouldn't be shared.");
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::process_read(peer& sender, boost::system::error_code error, std::size_t read) {

	if (!error) {
		sender.io_data->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
		std::vector<uint8_t>& dyn_buff = sender.io_data->dynamic_buffer;
		std::array<uint8_t, T>& fixed_buff = sender.io_data->fixed_buffer;

		read += sender.io_data->last_read;

		typename std::array<uint8_t, T>::size_type current_index{0};
		if (sender.io_data->last_command == commands::null_command) {
			sender.io_data->last_command = static_cast<commands>(fixed_buff[current_index++]);
			if (read == 1) {
				sender.io_data->socket.async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
						boost::bind(&io_manager::process_read, this, sender, _1, _2)
				);
				return;
			}
		}

		uint8_t to_be_read;
		bool has_work = true;

		// Reads part of the packet
		auto process_partial_read = [&] {
			--current_index;

			if (current_index == 1) {
				sender.io_data->last_command = commands::null_command;
				sender.io_data->last_read = read;
				sender.io_data->socket.async_read_some(
						boost::asio::buffer(fixed_buff.data() + read, fixed_buff.size() - read),
						boost::bind(&io_manager::process_read, this, sender, _1, _2)
				);
			} else {
				// shifting the buffer so we can use the end of it to store the up coming bits
				uint8_t count{0};
				while (read > current_index) {
					fixed_buff[count++] = fixed_buff[current_index++];
				}
				sender.io_data->last_read = count;

				sender.io_data->socket.async_read_some(
						boost::asio::buffer(fixed_buff.data() + count, fixed_buff.size() - count),
						boost::bind(&io_manager::process_read, this, sender, _1, _2)
				);
			}
			has_work = false;
		};

		// Reads all the packet
		auto process_read_all = [&] {
			while (to_be_read--) {
				dyn_buff.push_back(fixed_buff[current_index++]);
			}
			sender.io_data->last_read = 0;
		};

		// Ends the full packet read process
		auto packet_processed = [&] {
			detail::peer_manager_attorney<tcp::basic_io_manager <T,U,V,W>>::data_received(*m_owner, sender, sender.io_data->last_command, dyn_buff);

			dyn_buff.clear();
			sender.io_data->last_command = commands::null_command;
			sender.io_data->last_read = 0;

			if (current_index < read) {
				// we still have some extra data in the buffer
				for (auto j = 0u ; j < read - current_index ; ++j) {
					fixed_buff[j] = fixed_buff[current_index + j];
				}
				process_read(sender, error, read - current_index);

			} else {
				sender.io_data->socket.async_read_some(
						boost::asio::buffer(fixed_buff.data(), fixed_buff.size()),
						boost::bind(&io_manager::process_read, this, sender, _1, _2)
				);
			}
			has_work = false;
		};

		do {
			to_be_read = fixed_buff[current_index++];

			if (to_be_read) {

				if (to_be_read + current_index <= read) {
					// We can reach the end of the packet.
					process_read_all();
					packet_processed();

				} else {
					// We still have to wait for some more input
					process_partial_read();
				}
			} else {
				// to_be_red == 0 <=> more than std::numeric_limits octets to process.
				to_be_read = std::numeric_limits<uint8_t>::max();
				if (to_be_read + current_index <= read) {
					// We can reach the end of the sub packet
					process_read_all();

				} else {
					// We still have to wait for some more input
					process_partial_read();
				}
			}
		} while (has_work);

	} else {
		// error
		if (sender.io_data->socket.is_open()) {
			boost::system::error_code ec;
			sender.io_data->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			sender.io_data->socket.close(ec);
			detail::peer_manager_attorney<io_manager>::peer_disconnected(*m_owner, sender);
		}
	}
}


template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::write(const peer& target) const {

	try {
		auto& buffers = m_data_queues.at(target.id());
		boost::asio::async_write(
				target.io_data->socket,
				boost::asio::buffer(buffers.front().data(), buffers.front().size()),
				boost::bind(&io_manager::write_done, this, target)
		);
	} catch (const std::out_of_range&) {
		breep::logger<io_manager>.warning("Peer " + target.id_as_string() + " disconnected unexpectedly while data was being sent");
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::write_done(const peer& target) const {
	try {
		auto& buffers = m_data_queues.at(target.id());
		buffers.pop();
		if (!buffers.empty()) {
			write(target);
		}
	} catch (const std::out_of_range&) {
		breep::logger<io_manager>.warning("Peer " + target.id_as_string() + " disconnected unexpectedly while data was being sent");
	}
}

template <unsigned int T, unsigned long U, unsigned long V, unsigned long W>
void breep::tcp::basic_io_manager<T,U,V,W>::accept(boost::system::error_code ec) {

	if (!ec) {
		boost::array<uint8_t, 128> buffer;
		size_t len = m_socket->read_some(boost::asio::buffer(buffer), ec);

		if (ec) {
			breep::logger<io_manager>.warning("Failed to read data from incomming connection: ["
			                                  + m_socket->remote_endpoint().address().to_string() + "].");
			m_socket->close();
		} else {
			std::vector<uint8_t> protocol_id;
			protocol_id.reserve(8);
			detail::insert_uint32(protocol_id, IO_PROTOCOL_ID_1);
			detail::insert_uint32(protocol_id, IO_PROTOCOL_ID_2);
			m_socket->write_some(boost::asio::buffer(protocol_id), ec);

			// Reading the protocol ID
			if (len != 8) {
				breep::logger<io_manager>.warning("Incomming connection from [" + m_socket->remote_endpoint().address().to_string()
				                                  + "]: they don't have the same protocol ID format than us!");
				m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
				m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
				return;
			}
			while(len--) {
				if (buffer[len] != protocol_id[len]) {
					breep::logger<io_manager>.warning("Incomming peer has not the same io_manager protocol ID than us (["
					                                  + m_socket->remote_endpoint().address().to_string() + "]).");
					breep::logger<io_manager>.warning("Our protocol ID: " + std::to_string(IO_PROTOCOL_ID_1) + " " +
					                                  std::to_string(IO_PROTOCOL_ID_2) + ". Their protocol ID: "
					                                  + std::to_string(detail::read_uint32(buffer)) + " "
					                                  + std::to_string(detail::read_uint32(buffer, sizeof(uint32_t))) + ".");
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
					return;
				}
			}

			// Reading the id
			len = 0;
			do {
				len += m_socket->read_some(boost::asio::buffer(buffer.data() + len, buffer.size() - len), ec);
				if (ec) {
					m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
					m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
					return;
				}
			} while (len <= buffer[0]);

			std::string input;
			detail::unmake_little_endian(detail::unowning_linear_container(buffer.data() + 3, len - 3), input);
			boost::asio::write(*m_socket, boost::asio::buffer(m_id_packet));

			boost::uuids::uuid uuid;
			std::copy(input.data(), input.data() + input.size(), uuid.data);

			auto addr = m_socket->remote_endpoint().address();
			detail::peer_manager_attorney<io_manager>::peer_connected(
					*m_owner,
					peer(
							std::move(uuid),
							std::move(addr),
							static_cast<unsigned short>(buffer[1] << 8 | buffer[2]),
							std::make_shared<tcp::io_manager_data<T>>(m_socket, true)
					)
			);
		}
	}
	// reset the socket.
	m_socket = std::make_shared<boost::asio::ip::tcp::socket>(m_io_service);
	m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
}
