#pragma once

#include "NetCommon.h"
#include "net_tsqueue.h"
#include "NetMessage.h"
#include "net_connection.h"

namespace olc
{
	namespace net
	{
		template<typename T>
		class server_interface
		{
			server_interface(uint16_t port)
				: m_asioAccepter(m_asioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
			{

			}

			virtual ~server_interface()
			{

			}

			bool Start()
			{
				try
				{
					WaitForClientConnection();

					m_threadContext = std::thread([this]() {m_asioContext.run(); })
				}
				catch (const std::exception&)
				{	// Somthing prohibited the server from listening
					std::cerr << "[SERVER] Exception: " << e.what() << "\n";
					return false;
				}

				std::cout << "[SERVER] Started!\n";
				return true;
			}
			void Stop()
			{
				// Request the context to close
				m_asioContext.stop();

				// Tidy up the context thread
				if (m_threadContext.joinable())
					m_threadContext.join();

				// Inform someone, anybody, if they care..
				std::cout << "[SERVER] Stopped\n";
			}

			// Async - Instrcut asio to wait for connection
			void WaitForClientConnection()
			{
				m_asioAccepter.async_accept(
					[this](std::error_code ec, boost::asio::ip::tcp::socket socket)
					{	// 积己等 家南捞 param栏肺
						if (!ec)
						{
							std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

							std::shared_future<connection<T>> newconn =
								std::make_shared<connection<T>>(connection<T>::owner::server,
									m_asioContext, std::move(socket), m_qMessageIn);

							// Give the user server a chance to deny connection
							if (OnClientConnect(newconn))
							{
								// Connection allowed, so add to container of new connections
								m_deqConnections.push_back(std::move(newconn));

								// Issue a task to the connection's asio context to sit and wait for bytes to arrive
								m_deqConnections.back()->ConnectToClient(nIDCounter++);
								std::cout << "[" << m_deqConnections.back()->GetID() << "] Connect Approved\n";

							}
							else
							{
								std::cout << "[-----] Connection Denied\n";
							}
						}
						else
						{
							// Error has occured during acceptance
							std::cout << "[SERVER] New Connction Error: " << ec.message() << "\n";
						}

						// Prime the asio context with more work
						// wait for another connection...
						WaitForClientConnection();
					}
				);
			}

			// Send a message to a specific client
			void MessageClient(std::shared_ptr<connection<T>> client, const  message<T>& msg)
			{
				// Check client is connected...
				if (client && client->IsConnected())
				{
					// ... and post the message via the connection
					client->Send(msg);
				}
				else
				{
					OnClientDisconnect(client);
					client.reset();
					m_deqConnections.erase(
						std::remove(m_deqConnections.begin(), m_deqConnections.end(), client), m_deqConnections.end()
					);
				}
			}

			// Send message to all clients
			void MessageAllClients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
			{
				bool bInvalidClientExists = false;

				for (auto& item : m_deqConnections)
				{
					// Check client is connected...
					if (client && client->IsConnected())
					{
						if (client != pIgnoreClient)
							client->Send(msg);
					}
					else
					{
						OnClientDisconnect(client);
						client.reset();
						bInvalidClientExists = true;
					}
				}

				// call once for optimization
				if (bInvalidClientExists)
					m_deqConnections.erase(
						std::remove(m_deqConnections.begin(), m_deqConnections.end(), nullptr), m_deqConnections.end()
					);
			}

			// Force server to respond to incoming messages
			void Update(size_t nMaxMessages = -1)
			{
				// Process as many messages as you coan up to the value
				size_t nMessageCount = 0;
				while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty())
				{
					auto msg = m_qMessageIn.pop_front();

					// Pss to message handler
					OnMessage(msg.remote, msg.msg);

					nMessageCount++;
				}
			}

		protected:
			// Called when a client connects , you can veto the connection by returning false
			virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
			{
				return true;
			}

			// Called when a client appears to have disconnectd
			virtual bool OnClientDisconnect(std::shared_ptr<connection<T>> client)
			{
				return true;
			}

			// Called whnen a message arrives
			virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T>& msg)
			{

			}
		protected:
			// Thread Safe Queue for incoming message packets
			tsqueue<owned_message<T>> m_qMessageIn;

			// Container of active validated connection
			std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

			// Order of declaration si important - it is also the order of initialisation
			boost::asio::io_context m_asioContext;
			std::thread m_threadContext;

			// These things need an asio context
			boost::asio::ip::tcp::acceptor m_asioAccepter;

			// Clients will be identified in the "wider system" via an ID
			uint32_t nIDCounter = 10000;
		};
	}
}