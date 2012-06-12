#pragma once

#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

namespace socket_helpers {

	class socket_exception : public std::exception {
		std::string error;
	public:
		//////////////////////////////////////////////////////////////////////////
		/// Constructor takes an error message.
		/// @param error the error message
		///
		/// @author mickem
		socket_exception(std::string error) : error(error) {}
		~socket_exception() throw() {}

		//////////////////////////////////////////////////////////////////////////
		/// Retrieve the error message from the exception.
		/// @return the error message
		///
		/// @author mickem
		const char* what() const throw() { return error.c_str(); }

	};

	struct allowed_hosts_manager {
		template<class addr_type_t>
		struct host_record {
			host_record(std::string host, addr_type_t addr, addr_type_t mask) 
				: addr(addr)
				, mask(mask)
				, host(host) {}
			host_record(const host_record &other) 
				: addr(other.addr)
				, mask(other.mask)
				, host(other.host) {}
			const host_record& operator=(const host_record &other) {
				addr = other.addr;
				mask = other.mask;
				host = other.host;
				return *this;
			}
			std::string host;
			addr_type_t addr;
			addr_type_t mask;
		};
		typedef boost::asio::ip::address_v4::bytes_type addr_v4;
		typedef boost::asio::ip::address_v6::bytes_type addr_v6;

		typedef host_record<addr_v4> host_record_v4;
		typedef host_record<addr_v6> host_record_v6;

		std::list<host_record_v4> entries_v4;
		std::list<host_record_v6> entries_v6;
		std::list<std::string> sources;
		bool cached;

		allowed_hosts_manager() : cached(true) {}
		allowed_hosts_manager(const allowed_hosts_manager &other) : entries_v4(other.entries_v4), entries_v6(other.entries_v6), sources(other.sources), cached(other.cached) {}
		const allowed_hosts_manager& operator=(const allowed_hosts_manager &other) {
			entries_v4 = other.entries_v4;
			entries_v6 = other.entries_v6;
			sources = other.sources;
			cached = other.cached;
			return *this;
		}

		void set_source(std::wstring source) {
			sources.clear();
			BOOST_FOREACH(const std::wstring &s, strEx::splitEx(source, _T(","))) {
				sources.push_back(utf8::cvt<std::string>(s));
			}
		}
		addr_v4 lookup_mask_v4(std::string mask);
		addr_v6 lookup_mask_v6(std::string mask);
		void refresh(std::list<std::string> &errors);

		template<class T>
		inline bool match_host(const T &allowed, const T &mask, const T &remote) const {
			for (int i=0;i<allowed.size(); i++) {
				if ( (allowed[i]&mask[i]) != (remote[i]&mask[i]) )
					return false;
			}
			return true;
		}
		bool is_allowed(const boost::asio::ip::address &address, std::list<std::string> &errors) {
			return (entries_v4.empty()&&entries_v6.empty())
				|| (address.is_v4() && is_allowed_v4(address.to_v4().to_bytes(), errors))
				|| (address.is_v6() && is_allowed_v6(address.to_v6().to_bytes(), errors))
				|| (address.is_v6() && address.to_v6().is_v4_compatible() && is_allowed_v4(address.to_v6().to_v4().to_bytes(), errors))
				|| (address.is_v6() && address.to_v6().is_v4_mapped() && is_allowed_v4(address.to_v6().to_v4().to_bytes(), errors))
				;
		}
		bool is_allowed_v4(const addr_v4 &remote, std::list<std::string> &errors) {
			if (!cached)
				refresh(errors);
			BOOST_FOREACH(const host_record_v4 &r, entries_v4) {
				if (match_host(r.addr, r.mask, remote))
					return true;
			}
			return false;
		}
		bool is_allowed_v6(const addr_v6 &remote, std::list<std::string> &errors) {
			if (!cached)
				refresh(errors);
			BOOST_FOREACH(const host_record_v6 &r, entries_v6) {
				if (match_host(r.addr, r.mask, remote))
					return true;
			}
			return false;
		}
		std::wstring to_wstring();
	};

	struct connection_info {
		static const int backlog_default;
		connection_info() : back_log(backlog_default), port(0), thread_pool_size(0), timeout(30) {}

		connection_info(const connection_info &other) 
			: address(other.address)
			, port(other.port)
			, thread_pool_size(other.thread_pool_size)
			, back_log(other.back_log)
			, ssl(other.ssl)
			, timeout(other.timeout)
			, allowed_hosts(other.allowed_hosts)
			{
			}
		connection_info& operator=(const connection_info &other) {
			address = other.address;
			port = other.port;
			thread_pool_size = other.thread_pool_size;
			back_log = other.back_log;
			ssl = other.ssl;
			timeout = other.timeout;
			allowed_hosts = other.allowed_hosts;
			return *this;
		}


		std::list<std::wstring> validate_ssl();
		std::list<std::wstring> validate();

		std::string address;
		unsigned int port;
		unsigned int thread_pool_size;
		int back_log;
		unsigned int timeout;

		struct ssl_opts {
			ssl_opts() : enabled(false) {}

			ssl_opts(const ssl_opts &other) 
				: enabled(other.enabled)
				, certificate(other.certificate)
				, certificate_format(other.certificate_format)
				, certificate_key(other.certificate_key)
				, ca_path(other.ca_path)
				, allowed_ciphers(other.allowed_ciphers)
				, dh_key(other.dh_key)
				, verify_mode(other.verify_mode)
			{}
			ssl_opts& operator=(const ssl_opts &other) {
				enabled = other.enabled;
				certificate = other.certificate;
				certificate_format = other.certificate_format;
				certificate_key = other.certificate_key;
				ca_path = other.ca_path;
				allowed_ciphers = other.allowed_ciphers;
				dh_key = other.dh_key;
				verify_mode = other.verify_mode;
			}


			bool enabled;
			std::string certificate;
			std::string certificate_format;
			std::string certificate_key;

			std::string ca_path;
			std::string allowed_ciphers;
			std::string dh_key;

			std::string verify_mode;

			std::string to_string() {
				std::stringstream ss;
				if (enabled) {
					ss << "ssl: " << verify_mode;
					ss << ", cert: " << certificate << " (" << certificate_format << "), " << certificate_key;
					ss << ", dh: " << dh_key << ", ciphers: " << allowed_ciphers << ", ca: " << ca_path;
				} else 
					ss << "ssl disabled";
				return ss.str();
			}
		};
		ssl_opts ssl;

		allowed_hosts_manager allowed_hosts;

		std::string get_port() { return utf8::cvt<std::string>(strEx::itos(port)); }
		std::string get_address() { return address; }
		std::string get_endpoint_string() {
			return address + ":" + get_port();
		}
		std::wstring get_endpoint_wstring() {
			return utf8::cvt<std::wstring>(get_endpoint_string());
		}
	};



	namespace io {
		void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b);

		struct timed_writer : public boost::enable_shared_from_this<timed_writer> {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> read_result;

			timed_writer(boost::asio::io_service& io_service) : io_service(io_service), timer(io_service) {}
			~timed_writer() {
				timer.cancel();
			}
			void start_timer(boost::posix_time::time_duration duration) {
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(&timed_writer::set_result, shared_from_this(), &timer_result, _1));
			}
			void stop_timer() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void write(AsyncWriteStream& stream, MutableBufferSequence &buffer) {
				async_write(stream, buffer, boost::bind(&timed_writer::set_result, shared_from_this(), &read_result, _1));
			}

			template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
			bool write_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffer) {
				write(stream, buffer);
				return wait(socket);
			}

			template<typename Socket>
			bool wait(Socket& socket) {
				io_service.reset();
				while (io_service.run_one()) {
					if (read_result) {
						read_result.reset();
						return true;
					}
					else if (timer_result) {
						socket.close();
						return false;
					}
				}
				return false;
			}

			void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
				if (!ec)
					a->reset(ec);
			}

		};


		template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
		bool write_with_timeout(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.get_io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_write(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.get_io_service().reset();
			while (sock.get_io_service().run_one()) {
				if (read_result) {
					timer.cancel();
					return true;
				}
				else if (timer_result) {
					rawSocket.close();
					return false;
				}
			}

			if (read_result && *read_result)
				throw boost::system::system_error(*read_result);
			return false;
		}


		struct timed_reader : public boost::enable_shared_from_this<timed_reader> {
			boost::asio::io_service &io_service;
			boost::posix_time::time_duration duration;
			boost::asio::deadline_timer timer;

			boost::optional<boost::system::error_code> timer_result;
			boost::optional<boost::system::error_code> write_result;

			timed_reader(boost::asio::io_service &io_service) : io_service(io_service), timer(io_service) {}
			~timed_reader() {
				timer.cancel();
			}

			void start_timer(boost::posix_time::time_duration duration) {
				timer.expires_from_now(duration);
				timer.async_wait(boost::bind(&timed_reader::set_result, shared_from_this(), &timer_result, _1));
			}
			void stop_timer() {
				timer.cancel();
			}

			template <typename AsyncWriteStream, typename MutableBufferSequence>
			void read(AsyncWriteStream& stream, const MutableBufferSequence &buffers) {
				async_read(stream, buffers, boost::bind(&timed_reader::set_result, shared_from_this(), &write_result, _1));
			}

			template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
			bool read_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffers) {
				read(stream, buffers);
				return wait(socket);
			}
			template <typename Socket>
			bool wait(Socket& socket) {
				io_service.reset();
				while (io_service.run_one()) {
					if (write_result) {
						write_result.reset();
						return true;
					}
					else if (timer_result) {
						socket.close();
						return false;
					}
				}
				return false;
			}
			void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
				if (!ec)
					a->reset(ec);
			}

		};


		template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
		bool read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
			boost::optional<boost::system::error_code> timer_result;
			boost::asio::deadline_timer timer(sock.get_io_service());
			timer.expires_from_now(duration);
			timer.async_wait(boost::bind(set_result, &timer_result, _1));

			boost::optional<boost::system::error_code> read_result;
			async_read(sock, buffers, boost::bind(set_result, &read_result, _1));

			sock.get_io_service().reset();
			while (sock.get_io_service().run_one()) {
				if (read_result) {
					timer.cancel();
					return true;
				} else if (timer_result) {
					rawSocket.close();
					return false;
				} else {
// 					if (!rawSocket.is_open()) {
// 						timer.cancel();
// 						rawSocket.close();
// 						return false;
// 					}
				}
			}

			if (*read_result)
				throw boost::system::system_error(*read_result);
			return false;
		}
	}
}