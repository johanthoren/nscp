#pragma once

#include <nrpe/packet.hpp>
#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <iostream>

using boost::asio::ip::tcp;

namespace nsca {
	namespace client {


		template<class handler_type>
		class protocol : public boost::noncopyable {
		public:
			// traits
			typedef std::vector<char> read_buffer_type;
			typedef std::vector<char> write_buffer_type;
			typedef const nsca::packet request_type;
			typedef bool response_type;
			typedef handler_type client_handler;

		private:
			std::vector<char> iv_buffer_;
			std::vector<char> packet_buffer_;
			boost::shared_ptr<client_handler> handler_;
			nsca_encrypt crypto_;
			int time_;
			nsca::packet packet_;

			enum state {
				none,
				connected,
				got_iv,
				sent_request,
				has_request,
				done,
			};
			state current_state_;

			inline void set_state(state new_state) {
				current_state_ = new_state;
			}
		public:
			protocol(boost::shared_ptr<client_handler> handler) : handler_(handler), current_state_(none) {}
			virtual ~protocol() {}

			void on_connect() {
				set_state(connected);
			}
			void prepare_request(request_type &packet) {
				if (current_state_ == sent_request)
					set_state(has_request);
				else
					set_state(connected);
				packet_ = packet;
			}

			write_buffer_type& get_outbound() {
				std::string str = crypto_.get_rand_buffer(packet_.get_packet_length());
				packet_.get_buffer(str, time_);
				crypto_.encrypt_buffer(str);
				packet_buffer_ = std::vector<char>(str.begin(), str.end());
				return packet_buffer_;
			}
			read_buffer_type& get_inbound() {
				iv_buffer_ = std::vector<char>(nsca::length::iv::get_packet_length());
				return iv_buffer_;
			}

			response_type get_timeout_response() {
				return false;
			}
			response_type get_response() {
				return true;
			}
			bool has_data() {
				return current_state_ == has_request || current_state_ == got_iv;
			}
			bool wants_data() {
				return current_state_ == connected;
			}

			bool on_read(std::size_t bytes_transferred) {
				nsca::iv_packet iv_packet(std::string(iv_buffer_.begin(), iv_buffer_.end()));
				std::string iv = iv_packet.get_iv();
				time_ = iv_packet.get_time();
				crypto_.encrypt_init(handler_->get_password(), handler_->get_encryption(), iv);
				set_state(got_iv);
				return true;
			}
			bool on_write(std::size_t bytes_transferred) {
				set_state(sent_request);
				return true;
			}
		};
	}
}
