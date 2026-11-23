// asio_socket.cpp: Unified socket implementation using standalone ASIO
//
// Replaces the legacy XSocket (Winsock2/WSAEventSelect) with ASIO.
// Shared between Client and Server.
//
// Supports both polling mode (client) and async mode (server).
//
//////////////////////////////////////////////////////////////////////

#include "asio_socket.h"
#include <cstdio>
#include <algorithm>
#include "log.h"

namespace hb::shared::net {

namespace sock = socket;
using asio::ip::tcp;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ASIOSocket::ASIOSocket(asio::io_context& ctx, int block_limit)
	: m_ioContext(ctx)
	, m_socket(ctx)
	, m_acceptor(ctx)
	, m_strand(ctx)
	, m_block_limit(block_limit)
{
}

ASIOSocket::~ASIOSocket()
{
	cancel_async();
	close_connection();
}

bool ASIOSocket::init_buffer_size(size_t buffer_size)
{
	m_rcvBuffer.assign(buffer_size + 8, 0);
	m_sndBuffer.assign(buffer_size + 8, 0);
	m_buffer_size = buffer_size;
	// Async read buffer: header (3 bytes) + max body
	m_asyncRcvBuffer.assign(buffer_size + 8, 0);
	return true;
}

//////////////////////////////////////////////////////////////////////
// Poll - non-blocking event check (replaces WSAEventSelect + on_socket_event)
// Still used by client (manual poll mode) and for connect completion
//////////////////////////////////////////////////////////////////////

int ASIOSocket::Poll()
{
	if (m_type == 0) return sock::Event::NotInitialized;

	// Process any pending async operations (connect completion)
	m_ioContext.poll();
	m_ioContext.restart();

	// --- Handle async connect completion ---
	if (m_connectResultReady) {
		m_connectResultReady = false;
		m_connectPending = false;

		if (!m_connectError) {
			m_is_available = true;
			m_is_write_enabled = true;
			return sock::Event::ConnectionEstablish;
		}
		else {
			m_WSAErr = m_connectError.value();
			// Retry connection (same as legacy behavior)
			if (connect(m_addr, m_port_num) == false) {
				return sock::Event::SocketError;
			}
			return sock::Event::RetryingConnection;
		}
	}

	// Still waiting for connect
	if (m_connectPending) return 0;

	// --- Listen socket: probe for pending accept ---
	if (m_type == sock::Type::Listen) {
		if (m_pendingAcceptSocket.has_value()) {
			// Already have a pending accept from a previous poll
			return sock::Event::ConnectionEstablish;
		}

		asio::error_code ec;
		tcp::socket peer(m_ioContext);
		m_acceptor.accept(peer, ec);
		if (!ec) {
			m_pendingAcceptSocket.emplace(std::move(peer));
			return sock::Event::ConnectionEstablish;
		}
		// would_block = no pending connections
		return 0;
	}

	// --- Normal socket: check for data and handle writes ---
	if (m_type != sock::Type::Normal) return sock::Event::SocketMismatch;

	int result = 0;

	// Check if socket has been closed by peer
	asio::error_code ec;
	size_t avail = m_socket.available(ec);
	if (ec) {
		if (ec == asio::error::eof || ec == asio::error::connection_reset ||
			ec == asio::error::connection_aborted) {
			m_type = sock::Type::shutdown;
			return sock::Event::SocketClosed;
		}
		m_WSAErr = ec.value();
		return sock::Event::SocketError;
	}

	// Read available data.
	// NOTE: If on_read() completes a packet (READCOMPLETE), the data sits
	// in m_rcvBuffer. Callers that also use drain_to_queue() MUST check for
	// READCOMPLETE and queue the packet themselves before calling drain_to_queue(),
	// otherwise drain_to_queue() will overwrite m_rcvBuffer and lose the packet.
	if (avail > 0) {
		int readResult = on_read();
		if (readResult != sock::Event::OnRead) {
			result = readResult;
		}
	}

	// Try to drain unsent data queue
	if (!m_unsentQueue.empty() && m_is_write_enabled) {
		int writeResult = send_unsent_data();
		if (writeResult != sock::Event::UnsentDataSendComplete && result == 0) {
			result = writeResult;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////
// Connect - async non-blocking connect
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::connect(const char* addr, int port)
{
	if (m_type == sock::Type::Listen) return false;

	// Close existing socket if open
	asio::error_code ec;
	if (m_socket.is_open()) {
		m_socket.close(ec);
	}

	m_socket = tcp::socket(m_ioContext);
	m_socket.open(tcp::v4(), ec);
	if (ec) {
		hb::logger::debug("[ERROR] ASIOSocket::connect - open() failed: {}\n", ec.message().c_str());
		return false;
	}

	// Set non-blocking
	m_socket.non_blocking(true, ec);

	// Set socket options
	uint32_t bufSize = hb::shared::limits::MsgBufferSize * 2;
	m_socket.set_option(asio::socket_base::receive_buffer_size(bufSize), ec);
	m_socket.set_option(asio::socket_base::send_buffer_size(bufSize), ec);
	m_socket.set_option(tcp::no_delay(true), ec);

	// Store connection info for reconnect
	std::snprintf(m_addr, sizeof(m_addr), "%s", (addr != nullptr) ? addr : "");
	m_port_num = port;

	// start async connect
	tcp::endpoint endpoint(asio::ip::make_address(addr, ec), static_cast<unsigned short>(port));
	if (ec) {
		hb::logger::debug("[ERROR] ASIOSocket::connect - invalid address '{}': {}\n", addr, ec.message().c_str());
		return false;
	}

	m_connectPending = true;
	m_connectResultReady = false;
	m_connectError.clear();

	m_socket.async_connect(endpoint, [this](const asio::error_code& err) {
		m_connectError = err;
		m_connectResultReady = true;
	});

	m_type = sock::Type::Normal;
	return true;
}

//////////////////////////////////////////////////////////////////////
// BlockConnect - blocking connect with hostname resolution
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::block_connect(char* addr, int port)
{
	if (m_type == sock::Type::Listen) return false;

	asio::error_code ec;
	if (m_socket.is_open()) {
		m_socket.close(ec);
	}

	// Resolve hostname
	tcp::resolver resolver(m_ioContext);
	auto results = resolver.resolve(addr, std::to_string(port), ec);
	if (ec || results.empty()) {
		return false;
	}

	m_socket = tcp::socket(m_ioContext);

	// Blocking connect to first resolved endpoint
	asio::connect(m_socket, results, ec);
	if (ec) {
		m_WSAErr = ec.value();
		return false;
	}

	// Set socket options
	uint32_t bufSize = hb::shared::limits::MsgBufferSize * 2;
	m_socket.set_option(asio::socket_base::receive_buffer_size(bufSize), ec);
	m_socket.set_option(asio::socket_base::send_buffer_size(bufSize), ec);
	m_socket.set_option(tcp::no_delay(true), ec);

	// Set non-blocking after connect completes
	m_socket.non_blocking(true, ec);

	std::snprintf(m_addr, sizeof(m_addr), "%s", (addr != nullptr) ? addr : "");
	m_port_num = port;

	m_type = sock::Type::Normal;
	m_is_available = true;
	m_is_write_enabled = true;
	return true;
}

//////////////////////////////////////////////////////////////////////
// Listen - set up listening acceptor
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::listen(char* addr, int port)
{
	if (m_type != 0) return false;

	asio::error_code ec;

	tcp::endpoint endpoint(asio::ip::make_address(addr, ec), static_cast<unsigned short>(port));
	if (ec) return false;

	m_acceptor.open(tcp::v4(), ec);
	if (ec) return false;

	m_acceptor.set_option(asio::socket_base::reuse_address(true), ec);

	m_acceptor.bind(endpoint, ec);
	if (ec) {
		m_acceptor.close();
		return false;
	}

	m_acceptor.listen(5, ec);
	if (ec) {
		m_acceptor.close();
		return false;
	}

	// Set non-blocking so Poll() can probe for accepts
	m_acceptor.non_blocking(true, ec);

	m_type = sock::Type::Listen;
	return true;
}

//////////////////////////////////////////////////////////////////////
// Accept - accept pending connection into target socket (polling mode)
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::accept(ASIOSocket* sock)
{
	if (m_type != sock::Type::Listen) return false;
	if (sock == nullptr) return false;

	asio::error_code ec;

	// Use pending socket from Poll() if available
	if (m_pendingAcceptSocket.has_value()) {
		if (sock->m_socket.is_open()) {
			sock->m_socket.close(ec);
		}
		sock->m_socket = std::move(m_pendingAcceptSocket.value());
		m_pendingAcceptSocket.reset();
	}
	else {
		// Try direct non-blocking accept
		if (sock->m_socket.is_open()) {
			sock->m_socket.close(ec);
		}
		sock->m_socket = tcp::socket(m_ioContext);
		m_acceptor.accept(sock->m_socket, ec);
		if (ec) return false;
	}

	// Configure accepted socket
	sock->m_socket.non_blocking(true, ec);

	uint32_t bufSize = hb::shared::limits::MsgBufferSize * 2;
	sock->m_socket.set_option(asio::socket_base::receive_buffer_size(bufSize), ec);
	sock->m_socket.set_option(asio::socket_base::send_buffer_size(bufSize), ec);
	sock->m_socket.set_option(tcp::no_delay(true), ec);

	sock->m_type = sock::Type::Normal;
	sock->m_is_write_enabled = true;
	return true;
}

//////////////////////////////////////////////////////////////////////
// accept_from_socket - accept a pre-connected socket (async accept path)
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::accept_from_socket(asio::ip::tcp::socket&& peer)
{
	asio::error_code ec;

	if (m_socket.is_open()) {
		m_socket.close(ec);
	}

	m_socket = std::move(peer);

	// Configure accepted socket
	m_socket.non_blocking(true, ec);

	uint32_t bufSize = hb::shared::limits::MsgBufferSize * 2;
	m_socket.set_option(asio::socket_base::receive_buffer_size(bufSize), ec);
	m_socket.set_option(asio::socket_base::send_buffer_size(bufSize), ec);
	m_socket.set_option(tcp::no_delay(true), ec);

	m_type = sock::Type::Normal;
	m_is_write_enabled = true;
	m_is_available = true;
	return true;
}

//////////////////////////////////////////////////////////////////////
// on_read - read state machine (header then body) - polling mode
//////////////////////////////////////////////////////////////////////

int ASIOSocket::on_read()
{
	asio::error_code ec;

	if (m_status == sock::Status::ReadingHeader) {
		size_t n = m_socket.read_some(
			asio::buffer(m_rcvBuffer.data() + m_total_read_size, m_read_size), ec);

		if (ec == asio::error::would_block) return sock::Event::Block;
		if (ec || n == 0) {
			if (ec == asio::error::eof || ec == asio::error::connection_reset || n == 0) {
				m_type = sock::Type::shutdown;
				return sock::Event::SocketClosed;
			}
			m_WSAErr = ec.value();
			return sock::Event::SocketError;
		}

		m_read_size -= static_cast<uint32_t>(n);
		m_total_read_size += static_cast<uint32_t>(n);

		if (m_read_size == 0) {
			m_status = sock::Status::ReadingBody;
			uint16_t* wp = reinterpret_cast<uint16_t*>(m_rcvBuffer.data() + 1);
			m_read_size = static_cast<uint32_t>(*wp - 3);

			if (m_read_size == 0) {
				m_status = sock::Status::ReadingHeader;
				m_read_size = 3;
				m_total_read_size = 0;
				return sock::Event::ReadComplete;
			}
			else if (m_read_size > m_buffer_size) {
				m_status = sock::Status::ReadingHeader;
				m_read_size = 3;
				m_total_read_size = 0;
				return sock::Event::MsgSizeTooLarge;
			}
		}
		return sock::Event::OnRead;
	}
	else if (m_status == sock::Status::ReadingBody) {
		size_t n = m_socket.read_some(
			asio::buffer(m_rcvBuffer.data() + m_total_read_size, m_read_size), ec);

		if (ec == asio::error::would_block) return sock::Event::Block;
		if (ec || n == 0) {
			if (ec == asio::error::eof || ec == asio::error::connection_reset || n == 0) {
				m_type = sock::Type::shutdown;
				return sock::Event::SocketClosed;
			}
			m_WSAErr = ec.value();
			return sock::Event::SocketError;
		}

		m_read_size -= static_cast<uint32_t>(n);
		m_total_read_size += static_cast<uint32_t>(n);

		if (m_read_size == 0) {
			m_status = sock::Status::ReadingHeader;
			m_read_size = 3;
			m_total_read_size = 0;
		}
		else {
			return sock::Event::OnRead;
		}
	}

	return sock::Event::ReadComplete;
}

//////////////////////////////////////////////////////////////////////
// send - send with unsent queue fallback (polling mode)
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send(const char* data, size_t size, bool save_flag)
{
	// If there's queued data, queue this too to preserve ordering
	if (!m_unsentQueue.empty()) {
		if (save_flag) {
			if (!register_unsent_data(data, size)) {
				return sock::Event::QueueFull;
			}
			return sock::Event::Block;
		}
		else {
			return 0;
		}
	}

	int out_len = 0;
	while (out_len < size) {
		asio::error_code ec;
		size_t n = m_socket.write_some(
			asio::buffer(data + out_len, size - out_len), ec);

		if (ec) {
			if (ec == asio::error::would_block) {
				m_is_write_enabled = false;
				if (save_flag) {
					if (!register_unsent_data(data + out_len, size - out_len)) {
						return sock::Event::QueueFull;
					}
				}
				return sock::Event::Block;
			}
			m_WSAErr = ec.value();
			return sock::Event::SocketError;
		}
		out_len += static_cast<int>(n);
	}

	return out_len;
}

//////////////////////////////////////////////////////////////////////
// send_for_internal_use - send without queueing (for draining unsent queue)
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send_for_internal_use(const char* data, size_t size)
{
	int out_len = 0;
	while (out_len < size) {
		asio::error_code ec;
		size_t n = m_socket.write_some(
			asio::buffer(data + out_len, size - out_len), ec);

		if (ec) {
			if (ec == asio::error::would_block) {
				return out_len;
			}
			m_WSAErr = ec.value();
			return sock::Event::SocketError;
		}
		out_len += static_cast<int>(n);
	}
	return out_len;
}

//////////////////////////////////////////////////////////////////////
// register_unsent_data - queue data for later sending
//////////////////////////////////////////////////////////////////////

bool ASIOSocket::register_unsent_data(const char* data, size_t size)
{
	if (static_cast<int>(m_unsentQueue.size()) >= m_block_limit) {
		hb::logger::debug("[ASIO] Queue full! Dropped packet. Queue size: {}, limit: {}\n",
			static_cast<int>(m_unsentQueue.size()), m_block_limit);
		return false;
	}

	m_unsentQueue.emplace_back(data, size);
	return true;
}

//////////////////////////////////////////////////////////////////////
// send_unsent_data - drain the unsent queue
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send_unsent_data()
{
	while (!m_unsentQueue.empty()) {
		auto& block = m_unsentQueue.front();
		int ret = send_for_internal_use(block.remaining(), block.remainingSize());

		if (ret == block.remainingSize()) {
			// Entire block sent
			m_unsentQueue.pop_front();
		}
		else if (ret >= 0) {
			// Partial send - advance offset, wait for next writable
			block.offset += ret;
			m_is_write_enabled = false;
			return sock::Event::UnsentDataSendBlock;
		}
		else {
			// Error
			return ret;
		}
	}

	return sock::Event::UnsentDataSendComplete;
}

//////////////////////////////////////////////////////////////////////
// send_msg - send message with protocol header and optional encryption
// (synchronous polling mode - used by client and polling server path)
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send_msg(char* data, size_t size, char key)
{
	if (size > m_buffer_size) return sock::Event::MsgSizeTooLarge;
	if (m_type != sock::Type::Normal) return sock::Event::SocketMismatch;
	if (m_type == 0) return sock::Event::NotInitialized;

	// If async mode is active, delegate to async path
	if (m_async_mode.load(std::memory_order_relaxed)) {
		return send_msg_async(data, size, key);
	}

	// Build protocol message: [key:1][size:2][payload:N]
	m_sndBuffer[0] = key;

	uint16_t* wp = reinterpret_cast<uint16_t*>(m_sndBuffer.data() + 1);
	*wp = static_cast<uint16_t>(size + 3);

	std::memcpy(m_sndBuffer.data() + 3, data, size);

	// Encrypt payload if key is set
	if (key != 0) {
		for (uint32_t i = 0; i < size; i++) {
			m_sndBuffer[3 + i] += static_cast<char>(i ^ key);
			m_sndBuffer[3 + i] = static_cast<char>(m_sndBuffer[3 + i] ^ (key ^ static_cast<char>(size - i)));
		}
	}

	int ret;
	if (m_is_write_enabled == false) {
		if (!register_unsent_data(m_sndBuffer.data(), size + 3)) {
			return sock::Event::QueueFull;
		}
		ret = static_cast<int>(size + 3);
	}
	else {
		ret = send(m_sndBuffer.data(), size + 3, true);
	}

	if (ret < 0) return ret;
	else return (ret - 3);
}

//////////////////////////////////////////////////////////////////////
// send_msg_async - async send (posts to strand, builds frame per-message)
// Called from game thread. Errors come via error callback.
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send_msg_async(char* data, size_t size, char key)
{
	if (size > m_buffer_size || size > UINT16_MAX - 3) return sock::Event::MsgSizeTooLarge;
	if (m_type != sock::Type::Normal) return sock::Event::SocketMismatch;

	// Build frame into a per-message vector
	std::vector<char> frame(size + 3);
	frame[0] = key;

	uint16_t* wp = reinterpret_cast<uint16_t*>(frame.data() + 1);
	*wp = static_cast<uint16_t>(size + 3);

	std::memcpy(frame.data() + 3, data, size);

	// Encrypt payload if key is set
	if (key != 0) {
		for (uint32_t i = 0; i < size; i++) {
			frame[3 + i] += static_cast<char>(i ^ key);
			frame[3 + i] = static_cast<char>(frame[3 + i] ^ (key ^ static_cast<char>(size - i)));
		}
	}

	// Post to strand to serialize with other writes
	asio::post(m_strand, [this, buf = std::move(frame)]() mutable {
		bool wasEmpty = m_asyncWriteQueue.empty();
		if (m_asyncWriteQueue.size() >= MAX_ASYNC_WRITE_QUEUE) {
			// Queue full - invoke error callback
			if (m_onError) {
				m_onError(m_socket_index, sock::Event::QueueFull);
			}
			return;
		}
		m_asyncWriteQueue.push_back(std::move(buf));
		if (wasEmpty && !m_async_write_in_progress) {
			do_async_write();
		}
	});

	// Return optimistic success
	return static_cast<int>(size);
}

//////////////////////////////////////////////////////////////////////
// do_async_write - drain async write queue (runs on strand)
//////////////////////////////////////////////////////////////////////

void ASIOSocket::do_async_write()
{
	if (m_asyncWriteQueue.empty()) {
		m_async_write_in_progress = false;
		return;
	}

	m_async_write_in_progress = true;
	auto& front = m_asyncWriteQueue.front();

	asio::async_write(m_socket, asio::buffer(front.data(), front.size()),
		asio::bind_executor(m_strand,
			[this](const asio::error_code& ec, std::size_t /*bytesWritten*/) {
				if (ec) {
					m_async_write_in_progress = false;
					if (m_onError) {
						m_onError(m_socket_index, sock::Event::SocketError);
					}
					return;
				}
				m_asyncWriteQueue.pop_front();
				do_async_write();
			}));
}

//////////////////////////////////////////////////////////////////////
// send_msg_blocking_mode - blocking send for shutdown phase
//////////////////////////////////////////////////////////////////////

int ASIOSocket::send_msg_blocking_mode(char* buf, int nbytes)
{
	int nleft = nbytes;
	while (nleft > 0) {
		asio::error_code ec;
		// Temporarily set blocking for this operation
		m_socket.non_blocking(false, ec);
		size_t nwritten = m_socket.write_some(asio::buffer(buf, nleft), ec);
		m_socket.non_blocking(true, ec);

		if (ec) return -1;
		if (nwritten == 0) break;
		nleft -= static_cast<int>(nwritten);
		buf += nwritten;
	}
	return (nbytes - nleft);
}

//////////////////////////////////////////////////////////////////////
// get_rcv_data_pointer - get received message with decryption (polling mode)
//////////////////////////////////////////////////////////////////////

char* ASIOSocket::get_rcv_data_pointer(size_t* msg_size, char* key)
{
	char key_val = m_rcvBuffer[0];
	if (key != nullptr) *key = key_val;

	uint16_t* wp = reinterpret_cast<uint16_t*>(m_rcvBuffer.data() + 1);
	*msg_size = (*wp) - 3;
	size_t size = (*wp) - 3;

	if (size > hb::shared::limits::MsgBufferSize) size = hb::shared::limits::MsgBufferSize;

	// Decrypt payload if key is set
	if (key_val != 0) {
		for (size_t i = 0; i < size; i++) {
			m_rcvBuffer[3 + i] = static_cast<char>(m_rcvBuffer[3 + i] ^ (key_val ^ static_cast<char>(size - i)));
			m_rcvBuffer[3 + i] -= static_cast<char>(i ^ key_val);
		}
	}
	return (m_rcvBuffer.data() + 3);
}

//////////////////////////////////////////////////////////////////////
// drain_to_queue - read all available packets into the receive queue
//////////////////////////////////////////////////////////////////////

int ASIOSocket::drain_to_queue()
{
	int packets_queued = 0;
	constexpr int MAX_DRAIN_PER_CALL = 300;

	while (packets_queued < MAX_DRAIN_PER_CALL &&
		m_recv_queue.size() < MAX_QUEUE_SIZE)
	{
		int ret = on_read();

		switch (ret) {
		case sock::Event::ReadComplete:
		{
			size_t size = 0;
			char* data = get_rcv_data_pointer(&size);

			if (data != nullptr && size > 0) {
				m_recv_queue.emplace_back(data, size);
				packets_queued++;
			}
		}
		break;

		case sock::Event::Block:
			return packets_queued;

		case sock::Event::SocketError:
		case sock::Event::SocketClosed:
		case sock::Event::MsgSizeTooLarge:
			return -1;

		case sock::Event::OnRead:
			break;

		default:
			break;
		}
	}

	return packets_queued;
}

bool ASIOSocket::peek_packet(NetworkPacket& outPacket) const
{
	if (m_recv_queue.empty()) return false;
	outPacket = m_recv_queue.front();
	return true;
}

bool ASIOSocket::pop_packet()
{
	if (m_recv_queue.empty()) return false;
	m_recv_queue.pop_front();
	return true;
}

//////////////////////////////////////////////////////////////////////
// Connection info
//////////////////////////////////////////////////////////////////////

int ASIOSocket::get_peer_address(char* addr_string)
{
	asio::error_code ec;
	auto ep = m_socket.remote_endpoint(ec);
	if (!ec) {
		std::string addr = ep.address().to_string();
		std::snprintf(addr_string, 46, "%s", addr.c_str()); // 46 = INET6_ADDRSTRLEN
		return 0;
	}
	return -1;
}

NativeSocketHandle ASIOSocket::get_socket()
{
	return m_socket.native_handle();
}

//////////////////////////////////////////////////////////////////////
// close_connection - graceful shutdown and close
//////////////////////////////////////////////////////////////////////

void ASIOSocket::close_connection()
{
	asio::error_code ec;

	if (m_acceptor.is_open()) {
		m_acceptor.close(ec);
	}

	if (!m_socket.is_open()) return;

	// shutdown send side
	m_socket.shutdown(tcp::socket::shutdown_send, ec);

	// Drain receive buffer
	char tmp[100];
	for (;;) {
		size_t n = m_socket.read_some(asio::buffer(tmp, sizeof(tmp)), ec);
		if (ec || n == 0) break;
	}

	m_socket.close(ec);
	m_type = sock::Type::shutdown;
}

//////////////////////////////////////////////////////////////////////
// cancel_async - cancel all pending async operations
//////////////////////////////////////////////////////////////////////

void ASIOSocket::cancel_async()
{
	m_async_mode.store(false, std::memory_order_relaxed);

	asio::error_code ec;
	if (m_socket.is_open()) {
		m_socket.cancel(ec);
	}
	if (m_acceptor.is_open()) {
		m_acceptor.cancel(ec);
	}
}

//////////////////////////////////////////////////////////////////////
// set_callbacks - set async message and error callbacks
//////////////////////////////////////////////////////////////////////

void ASIOSocket::set_callbacks(MessageCallback onMessage, ErrorCallback onError)
{
	m_onMessage = std::move(onMessage);
	m_onError = std::move(onError);
}

//////////////////////////////////////////////////////////////////////
// start_async_read - begin async read chain (header -> body -> callback -> repeat)
//////////////////////////////////////////////////////////////////////

void ASIOSocket::start_async_read()
{
	m_async_mode.store(true, std::memory_order_relaxed);
	do_async_read_header();
}

void ASIOSocket::do_async_read_header()
{
	// Read exactly 3 bytes: [key:1][size:2]
	asio::async_read(m_socket, asio::buffer(m_asyncRcvBuffer.data(), 3),
		asio::bind_executor(m_strand,
			[this](const asio::error_code& ec, std::size_t /*bytesRead*/) {
				if (ec) {
					if (m_onError) {
						int errCode = sock::Event::SocketError;
						if (ec == asio::error::eof || ec == asio::error::connection_reset ||
							ec == asio::error::connection_aborted || ec == asio::error::operation_aborted) {
							errCode = sock::Event::SocketClosed;
						}
						m_onError(m_socket_index, errCode);
					}
					return;
				}

				// Parse header: byte 0 = key, bytes 1-2 = total size (including header)
				uint16_t* wp = reinterpret_cast<uint16_t*>(m_asyncRcvBuffer.data() + 1);
				size_t totalSize = static_cast<size_t>(*wp);

				if (totalSize <= 3) {
					// Header-only message (no body)
					char key = m_asyncRcvBuffer[0];
					if (m_onMessage) {
						m_onMessage(m_socket_index, nullptr, 0, key);
					}
					do_async_read_header();
					return;
				}

				size_t bodySize = totalSize - 3;
				if (bodySize > m_buffer_size) {
					// Message too large
					if (m_onError) {
						m_onError(m_socket_index, sock::Event::MsgSizeTooLarge);
					}
					return;
				}

				do_async_read_body(bodySize);
			}));
}

void ASIOSocket::do_async_read_body(size_t bodySize)
{
	// Read body into buffer starting at offset 3 (after header)
	asio::async_read(m_socket, asio::buffer(m_asyncRcvBuffer.data() + 3, bodySize),
		asio::bind_executor(m_strand,
			[this, bodySize](const asio::error_code& ec, std::size_t /*bytesRead*/) {
				if (ec) {
					if (m_onError) {
						int errCode = sock::Event::SocketError;
						if (ec == asio::error::eof || ec == asio::error::connection_reset ||
							ec == asio::error::connection_aborted || ec == asio::error::operation_aborted) {
							errCode = sock::Event::SocketClosed;
						}
						m_onError(m_socket_index, errCode);
					}
					return;
				}

				// Decrypt payload
				char key = m_asyncRcvBuffer[0];
				if (key != 0) {
					for (uint32_t i = 0; i < bodySize; i++) {
						m_asyncRcvBuffer[3 + i] = static_cast<char>(
							m_asyncRcvBuffer[3 + i] ^ (key ^ static_cast<char>(bodySize - i)));
						m_asyncRcvBuffer[3 + i] -= static_cast<char>(i ^ key);
					}
				}

				// Deliver message via callback
				if (m_onMessage) {
					m_onMessage(m_socket_index, m_asyncRcvBuffer.data() + 3, bodySize, key);
				}

				// Chain to next header read
				do_async_read_header();
			}));
}

//////////////////////////////////////////////////////////////////////
// start_async_accept - begin async accept loop on listen socket
//////////////////////////////////////////////////////////////////////

void ASIOSocket::start_async_accept(AcceptCallback callback)
{
	if (m_type != sock::Type::Listen) return;

	m_onAccept = std::move(callback);
	m_async_mode.store(true, std::memory_order_relaxed);

	// start the accept loop
	auto doAccept = [this]() {
		m_acceptor.async_accept(
			[this](const asio::error_code& ec, tcp::socket peer) {
				if (!ec) {
					if (m_onAccept) {
						m_onAccept(std::move(peer));
					}
				}
				else if (ec == asio::error::operation_aborted) {
					return; // Shutting down
				}
				// Continue accepting (even after errors like too many open files)
				if (m_async_mode.load(std::memory_order_relaxed)) {
					start_async_accept(m_onAccept);
				}
			});
	};
	doAccept();
}

} // namespace hb::shared::net
