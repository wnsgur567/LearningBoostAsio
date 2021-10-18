#pragma once

#include "NetCommon.h"
#include "NetMessage.h"
#include "net_tsqueue.h"
#include "net_connection.h"

namespace olc
{
	namespace net
	{
		template<typename T>
		class client_interface
		{
		public:
			client_interface() {}
			virtual ~client_interface()
			{
				// If the client is destroyed, always try and disconnect from server
				Disconnect();
			}
		public:
			// Connect to server with hostname/ip-address and port
			bool Connect(const std::string& host, const uint16_t port)
			{
				try
				{

					// Resolve hostname/ip-address into tangiable physical address
					boost::asio::ip::tcp::resolver resolver(m_context);
					auto endpoints = resolver.resolve(host, std::to_string(port));

					// Create Connection
					m_connection = std::make_unique<connection<T>>(
						connection<T>::owner::client,
						m_context,
						boost::asio::ip::tcp::socket(m_context),
						m_qMessagesIn);

					m_connection->ConnectToServer(endpoints);

					thrContext = std::thread(
						[this]() {m_context.run(); }
					);
				}
				catch (const std::exception& e)
				{
					std::cerr << "Client Exception: " << e.what() << "\n";
					return false;
				}
				return true;
			}
			// Disconnect from server
			void Disconnect()
			{
				// If connection exissts, and it's connected then...
				if (IsConnected())
				{
					// ...disconnect from server gracefully
					m_connection->Disconnect();
				}
				// Either way, we're also done with the asio context...
				m_context.stop();
				if (thrContext.joinable())
					thrContext.join();
			}
			// Check if client is actually connected to a server
			bool IsConnected()
			{
				if (m_connection)
					return m_connection->IsConnected();
				else
					return false;
			}
		public:
			void Send(const message<T>& msg)
			{
				if (IsConnected())
					m_connection->Send(msg);
			}
			// Retrieve queue of messges from server
			tsqueue<owned_message<T>>& Incoming()
			{
				return m_qMessagesIn;
			}
		protected:
			boost::asio::io_context m_context;
			std::thread thrContext;
			std::unique_ptr<connection<T>> m_connection;
		private:
			tsqueue<owned_message<T>> m_qMessagesIn;
		};
	}
}