#ifndef BREEP_NETWORK_TCP_BASIC_IO_MANAGER_HPP
#define BREEP_NETWORK_TCP_BASIC_IO_MANAGER_HPP

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
 * @file tcp/network_manager.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */

#include <cstdint>
#include <unordered_map>
#include <queue>
#include <vector>
#include <memory>
#include <limits>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "breep/network/io_manager_base.hpp"
#include "breep/util/exceptions.hpp"
#include "breep/network/detail/commands.hpp"


namespace breep {
	template <typename T>
	class basic_peer_manager;
}

namespace breep { namespace tcp {

	/**
	 * io_manager_data, to be stored in peer<tcp::io_manager>.
	 */
	template <unsigned int BUFFER_LENGTH>
	struct io_manager_data final {

		io_manager_data() = delete;

		explicit io_manager_data(boost::asio::ip::tcp::socket&& socket_, bool waiting_acceptance_ans = false)
				: socket(std::move(socket_))
				, waiting_acceptance_answer(waiting_acceptance_ans)
		{}

		explicit io_manager_data(std::shared_ptr<boost::asio::ip::tcp::socket>& socket_ptr, bool waiting_acceptance_ans = false)
				: socket(std::move(*socket_ptr.get()))
				, waiting_acceptance_answer(waiting_acceptance_ans)
		{}

		~io_manager_data() = default;

		io_manager_data(const io_manager_data&) = delete;
		io_manager_data& operator=(const io_manager_data&) = delete;

		boost::asio::ip::tcp::socket socket;
		std::array<uint8_t, BUFFER_LENGTH> fixed_buffer{};
		std::vector<uint8_t> dynamic_buffer{};

		std::size_t last_read{};
		commands last_command{commands::null_command};

		bool waiting_acceptance_answer;

		std::chrono::milliseconds timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())};
	};

	/**
	 * @brief reference tcp network_manager implementation
	 *
	 * @tparam BUFFER_LENGTH          Length of the local buffer
	 * @tparam keep_alive_send_millis Time interval indicating the sending of keep_alive packets frequency (in milliseconds).
	 * @tparam timeout_millis         Time interval after which a peer should be considered dead if no packets have been received from him (in milliseconds).
	 *
	 * @since 0.1.0
	 */
	template <unsigned int BUFFER_LENGTH, unsigned long keep_alive_send_millis, unsigned long timeout_millis, unsigned long timeout_check_interval_millis>
	class basic_io_manager final: public io_manager_base<basic_io_manager<BUFFER_LENGTH,keep_alive_send_millis,timeout_millis,timeout_check_interval_millis>> {
	public:

		// The protocol ID should be changed at each compatibility break.
		static constexpr uint32_t IO_PROTOCOL_ID_1 =  755960664; // UPDATE THIS TOGETHER WITH THE HASHING FUNCTION +1 [util/type_traits.hpp]
		static constexpr uint32_t IO_PROTOCOL_ID_2 = 1683390697; // +3

		using io_manager = basic_io_manager<BUFFER_LENGTH,keep_alive_send_millis,timeout_millis,timeout_check_interval_millis>;
		using peer = basic_peer<io_manager>;
		using data_type = std::shared_ptr<io_manager_data<BUFFER_LENGTH>>;

		explicit basic_io_manager(unsigned short port);

		basic_io_manager(io_manager&& other) noexcept;

		~basic_io_manager() final;

		basic_io_manager(const io_manager&) = delete;
		io_manager& operator=(const io_manager&) = delete;

		template <typename Container>
		void send(commands command, const Container& data, const peer& target) const;

		template <typename InputIterator, typename size_type>
		void send(commands command, InputIterator it, size_type size, const peer& target) const;

		detail::optional<peer> connect(const boost::asio::ip::address& address, unsigned short port) override;

		void process_connected_peer(peer& connected) final;

		void process_connection_denial(peer& peer) final;

		void disconnect() final;

		void disconnect(peer& peer) final;

		void run() final;

		void set_log_level(log_level ll) const final {
			breep::logger<io_manager>.level(ll);
		}

	private:

		void port(unsigned short port) final {
			make_id_packet();

			m_acceptor.close();
			m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)};
			m_acceptor.async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));

			if (m_acceptor_v4 != nullptr) {
				m_acceptor_v4->close();
				*m_acceptor_v4 = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)};
				m_acceptor_v4->async_accept(*m_socket, boost::bind(&io_manager::accept, this, _1));
			}
		}

		void keep_alive_impl() {
			breep::logger<io_manager>.trace("Sending keep_alives");
			for (const auto& peers_pair : m_owner->peers()) {
				send(commands::keep_alive, constant::unused_param, peers_pair.second);
			}
			m_keepalive_dlt.expires_from_now(boost::posix_time::millisec(keep_alive_send_millis));
			m_keepalive_dlt.async_wait(boost::bind(&io_manager::keep_alive_impl, this));
		}

		void timeout_impl() {
			std::chrono::milliseconds time_now =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
			for (const auto& peers_pair : m_owner->peers()) {
				if (time_now - peers_pair.second.io_data->timestamp > std::chrono::milliseconds(timeout_millis)) {
					breep::logger<io_manager>.trace(peers_pair.second.id_as_string() + " timed out");
					peers_pair.second.io_data->socket.close();
				}
			}
			m_timeout_dlt.expires_from_now(boost::posix_time::millisec(timeout_check_interval_millis));
			m_timeout_dlt.async_wait(boost::bind(&io_manager::timeout_impl, this));
		}

		void owner(basic_peer_manager<io_manager>* owner) override;

		void process_read(peer& sender, boost::system::error_code error, std::size_t read);

		void write(const peer& target) const;

		void write_done(const peer& target) const;

		void accept(boost::system::error_code ec);

		void make_id_packet() {
			m_id_packet.clear();
			m_id_packet.resize(3, 0);
			detail::make_little_endian(detail::unowning_linear_container(m_owner->self().id().data), m_id_packet);

			m_id_packet[0] = static_cast<uint8_t>(m_id_packet.size() - 1);
			m_id_packet[1] = static_cast<uint8_t>(m_owner->port() >> 8) & std::numeric_limits<uint8_t>::max();
			m_id_packet[2] = static_cast<uint8_t>(m_owner->port() & std::numeric_limits<uint8_t>::max());
		}

		basic_peer_manager<io_manager>* m_owner;
		mutable boost::asio::io_service m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		boost::asio::ip::tcp::acceptor* m_acceptor_v4;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

		std::string m_id_packet;

		boost::asio::deadline_timer m_timeout_dlt;
		boost::asio::deadline_timer m_keepalive_dlt;

		mutable std::unordered_map<boost::uuids::uuid, std::queue<std::vector<uint8_t>>, boost::hash<boost::uuids::uuid>> m_data_queues;
	};
}} // namespace breep::tcp

#include "breep/network/tcp/impl/basic_io_manager.tcc"

#endif //BREEP_NETWORK_TCP_BASIC_IO_MANAGER_HPP
