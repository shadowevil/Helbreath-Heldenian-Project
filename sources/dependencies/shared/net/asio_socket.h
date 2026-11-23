// asio_socket.h: Unified socket class using standalone ASIO
//
// Replaces the legacy XSocket (Winsock2/WSAEventSelect) with ASIO.
// Shared between Client and Server.
//
// Supports both polling mode (client) and async mode (server).
// When using async mode, a shared io_context with background threads
// drives async_read/async_write with per-socket strands.
//
//////////////////////////////////////////////////////////////////////

#pragma once

// ASIO standalone configuration (must be before asio.hpp)
#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7+
#endif

#include "ASIO/asio.hpp"

#include "net_constants.h"
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <mutex>
#include <atomic>

namespace hb::shared::net {

// Socket type, status, and event enums
namespace socket
{

namespace Type
{
	enum : int
	{
		Listen   = 1,
		Normal   = 2,
		shutdown = 3,
	};
}

namespace Status
{
	enum : char
	{
		ReadingHeader = 11,
		ReadingBody   = 12,
	};
}

namespace Event
{
	enum : int
	{
		SocketMismatch         = -121,
		ConnectionEstablish    = -122,
		RetryingConnection     = -123,
		OnRead                 = -124,
		ReadComplete           = -125,
		Unknown                = -126,
		SocketClosed           = -127,
		Block                  = -128,
		SocketError            = -129,
		CriticalError          = -130,
		NotInitialized         = -131,
		MsgSizeTooLarge        = -132,
		ConfirmCodeNotMatch    = -133,
		QueueFull              = -134,
		UnsentDataSendBlock    = -135,
		UnsentDataSendComplete = -136,
	};
}

} // namespace socket

// Native socket handle type (for get_socket compatibility)
using NativeSocketHandle = asio::ip::tcp::socket::native_handle_type;

struct NetworkPacket {
	std::vector<uint8_t> data;
	size_t reportedSize;

	NetworkPacket() : reportedSize(0) {}
	NetworkPacket(const char* src, size_t size)
		: reportedSize(size)
	{
		data.reserve(size + 1024);
		data.assign(reinterpret_cast<const uint8_t*>(src),
			reinterpret_cast<const uint8_t*>(src) + size);
		data.insert(data.end(), 1024, 0);
	}

	size_t size() const { return reportedSize; }
	bool empty() const { return reportedSize == 0; }
	const char* ptr() const { return reinterpret_cast<const char*>(data.data()); }
};

// Unsent data block (RAII replacement for raw char* + size pairs)
struct UnsentBlock {
	std::vector<char> data;
	size_t offset = 0;  // bytes already sent from this block

	UnsentBlock() = default;
	UnsentBlock(const char* data, size_t size)
		: data(data, data + size), offset(0) {}

	const char* remaining() const { return data.data() + offset; }
	size_t remainingSize() const { return data.size() - offset; }
};

// Async callback types
using MessageCallback = std::function<void(int socketIndex, const char* data, size_t size, char key)>;
using ErrorCallback = std::function<void(int socketIndex, int errorCode)>;
using AcceptCallback = std::function<void(asio::ip::tcp::socket peer)>;

class ASIOSocket
{
public:
	// Constructor takes a reference to an external io_context (from IOServicePool)
	ASIOSocket(asio::io_context& ctx, int block_limit);
	virtual ~ASIOSocket();

	// Non-copyable
	ASIOSocket(const ASIOSocket&) = delete;
	ASIOSocket& operator=(const ASIOSocket&) = delete;

	// Buffer initialization
	bool init_buffer_size(size_t buffer_size);

	// Connection management
	bool connect(const char* addr, int port);
	bool block_connect(char* addr, int port);
	bool listen(char* addr, int port);
	bool accept(ASIOSocket* sock);

	// Accept a pre-connected socket (for async accept path)
	bool accept_from_socket(asio::ip::tcp::socket&& peer);

	// Polling (replaces WSAEventSelect + on_socket_event) - still works for client
	int Poll();

	// Sending (synchronous path for polling mode)
	int send_msg(char* data, size_t size, char key = 0);
	int send_msg_blocking_mode(char* buf, int nbytes);

	// Async sending (posts to strand, builds frame, chains async_write)
	int send_msg_async(char* data, size_t size, char key = 0);

	// Receiving (synchronous path for polling mode)
	char* get_rcv_data_pointer(size_t* msg_size, char* key = 0);

	// v4 Networking API (packet queue) - polling mode only
	int drain_to_queue();
	bool peek_packet(NetworkPacket& outPacket) const;
	bool pop_packet();
	bool has_pending_packets() const { return !m_recv_queue.empty(); }
	size_t get_queue_size() const { return m_recv_queue.size(); }
	void clear_queue() { m_recv_queue.clear(); }
	void queue_completed_packet(const char* data, size_t size) { m_recv_queue.emplace_back(data, size); }

	// Connection info
	int get_peer_address(char* addr_string);
	NativeSocketHandle get_socket();

	// Close
	void close_connection();

	// --- Async mode (server) ---
	void set_socket_index(int idx) { m_socket_index = idx; }
	void set_callbacks(MessageCallback onMessage, ErrorCallback onError);
	void start_async_read();
	void start_async_accept(AcceptCallback callback);
	void cancel_async();

	// Public state
	int  m_WSAErr = 0;
	bool m_is_available = false;
	bool m_is_write_enabled = false;
	char m_type = 0;

private:
	// Internal read/send helpers (synchronous polling path)
	int on_read();
	int send(const char* data, size_t size, bool save_flag);
	int send_for_internal_use(const char* data, size_t size);
	int send_unsent_data();
	bool register_unsent_data(const char* data, size_t size);

	// Async read chain
	void do_async_read_header();
	void do_async_read_body(size_t bodySize);

	// Async write chain
	void do_async_write();

	// ASIO internals - reference to external io_context
	asio::io_context& m_ioContext;
	asio::ip::tcp::socket m_socket;
	asio::ip::tcp::acceptor m_acceptor;
	asio::io_context::strand m_strand;

	// Async connect state
	bool m_connectPending = false;
	bool m_connectResultReady = false;
	asio::error_code m_connectError;

	// Pending accept socket (stored between Poll detecting accept and accept call)
	std::optional<asio::ip::tcp::socket> m_pendingAcceptSocket;

	// Receive/send buffers (RAII vectors replace raw char*)
	std::vector<char> m_rcvBuffer;
	std::vector<char> m_sndBuffer;
	size_t m_buffer_size = 0;

	// Read state machine (polling mode)
	char     m_status = hb::shared::net::socket::Status::ReadingHeader;
	size_t m_read_size = 3;
	size_t m_total_read_size = 0;

	// Stored connection info (for reconnect)
	char m_addr[30] = {};
	int  m_port_num = 0;

	// Unsent data queue (replaces raw pointer circular buffer) - polling mode
	std::deque<UnsentBlock> m_unsentQueue;
	int m_block_limit;

	// Packet receive queue - polling mode
	std::deque<NetworkPacket> m_recv_queue;
	static constexpr size_t MAX_QUEUE_SIZE = 2000;

	// --- Async mode members ---
	int m_socket_index = -1;
	MessageCallback m_onMessage;
	ErrorCallback m_onError;
	AcceptCallback m_onAccept;

	// Async read buffer (separate from polling m_rcvBuffer)
	std::vector<char> m_asyncRcvBuffer;

	// Async write queue (strand-serialized)
	std::deque<std::vector<char>> m_asyncWriteQueue;
	bool m_async_write_in_progress = false;

	// Async write queue size limit
	static constexpr size_t MAX_ASYNC_WRITE_QUEUE = 5000;

	// Flag to indicate async mode is active
	std::atomic<bool> m_async_mode{false};
};

} // namespace hb::shared::net
