#pragma once

#include "NetCommon.h"
#include "net_tsqueue.h"
#include "NetMessage.h"

namespace olc
{
	namespace net
	{
		template<typename T>
		class connection : public std::enable_shared_from_this<T>
		{
		public:
			connection() {}
			virtual ~connection() {}

		public:
			bool ConnectToServer();
			bool Disconnect();
			bool IsConnected() const;
		public:
			bool Send(const message<T>& msg);
		protected:
			// Each connection has a unique socket to a remote
			boost::asio::ip::tcp::socket m_socket;

			// This context is shared with the whole asio instance
			// 서버를 기준으로도 하나의 io_context를 사용
			// io_context의 동작은 thread safe 하게 돌아감
			boost::asio::io_context& m_asioContext;

			// This queue holds all messages to be send to the remote side of this connection
			tsqueue<message<T>> m_qMessagesOut;

			// This queue holds all messages that have been recieved from the remote side of this connection
			// Note it is a reference as the "owner" of this connection is expected to provide a queue
			tsqueue<owned_message>& m_qMessagesIn;
		};
	}
}