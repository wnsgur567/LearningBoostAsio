#pragma once

#include "NetCommon.h"
#include "net_tsqueue.h"
#include "NetMessage.h"

namespace olc
{
	namespace net
	{
		// Forward declare
		template<typename T>
		class server_interface;

		template<typename T>
		class connection : public std::enable_shared_from_this<connection<T>>
		{
		public:
			enum class owner
			{
				server,
				client
			};

			connection(owner parent, boost::asio::io_context& asioContext, boost::asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn)
				:m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn)
			{
				m_nOwnerType = parent;

				// Construct validation check data
				if (m_nOwnerType == owner::server)
				{
					// Connection is Server -> Client,
					// Construct random data for the client to transform ans send back for validation
					m_nHandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

					// Pre-calculate the result for checking when the client responds
					m_nHandshakeCheck = scramble(m_nHandshakeOut);
				}
				else
				{
					m_nHandshakeIn = 0;
					m_nHandshakeOut = 0;
				}
			}


			virtual ~connection() {}

			// This ID is used system wide - its how clients will understand other clients exist across the whole system
			uint32_t GetID() const
			{
				return id;
			}

		public:
			void ConnectToClient(olc::net::server_interface<T>* server, uint32_t uid = 0)
			{
				if (m_nOwnerType == owner::server)
				{
					if (m_socket.is_open())
					{
						id = uid;

						// A client has attempted to connect to the server
						// We wish the client to first validate itself, so first write out the handshake data to be validated
						WriteValidation();

						// Next , issue a task to sit and wait asynchronously for precisely
						// the validation data sent back from the client
						ReadValidation(server);
					}
				}
			}
			void ConnectToServer(const boost::asio::ip::tcp::resolver::results_type& endpoints)
			{
				// Only clients can connect to servers
				if (m_nOwnerType == owner::client)
				{
					// Request asio attempts to connect to an endpoint
					boost::asio::async_connect(m_socket, endpoints,
						[this](std::error_code ec, boost::asio::ip::tcp::endpoint endpoint)
						{
							if (!ec)
							{
								// First thing server will do is send packet to be validated 
								// so wait for that and respond
								ReadValidation();
							}
						});
				}
			}
			void Disconnect()
			{
				if (IsConnected())
					boost::asio::post(m_asioContext,
						[this]()
						{
							m_socket.close();
						});
			}
			bool IsConnected() const
			{
				return m_socket.is_open();
			}
		public:
			// Async - Send a message, connections are one-to-one
			// so no need to specify the target, for a client, the target is the server and vice versa
			void Send(const message<T>& msg)
			{
				boost::asio::post(m_asioContext,
					[this, msg]()
					{
						// If the queue has a message in it, 
						// then we must assume that it is in the process of asynchronously being written.
						// If no messages were available to be written, then start the process of writing the message
						bool bWriteingMessage = !m_qMessagesOut.empty();
						m_qMessagesOut.push_back(msg);
						if (!bWriteingMessage)
						{
							WriteHeader();
						}
					});
			}

		private:
			// Async - Prime context ready to read a message header
			void ReadHeader()
			{
				boost::asio::async_read(m_socket, boost::asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// A complete message header has been read, check if this message has a body to follow...
							if (m_msgTemporaryIn.header.size > 0)
							{
								m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
								ReadBody();
							}
							else
							{
								// it doesn't so add this bodyless message to the connections incoming message queue
								AddToIncomingMessageQueue();
							}
						}
						else
						{
							// Reading from the client went wrong, most likely a disconnect has occured.
							// Close the socket and let the system tidy it up later
							std::cout << "[" << id << "] Read Header Fail.\n";
							m_socket.close();
						}
					});
			}

			// Async - Prime context ready to read a message body
			void ReadBody()
			{

				boost::asio::async_read(m_socket, boost::asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							AddToIncomingMessageQueue();
						}
						else
						{
							std::cout << "[" << id << "] Read Body Fail.\n";
							m_socket.close();
						}
					});
			}

			// Async - Prime context to write a message haeader
			void WriteHeader()
			{
				// If this function is called, we know the outgoing message queue must have at least one message to send
				// So allocate a transmission buffer to hold the message, 
				// and issue the work - asio sned thes bytes
				boost::asio::async_write(m_socket, boost::asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
					{
						// asio has now sent the bytes - if there was a problem an error would be available
						if (!ec)
						{
							// check if the message header jst sent also has a message body
							if (m_qMessagesOut.front().body.size() > 0)
							{
								// ...it does, so issue the task to write the body bytes
								WriteBody();
							}
							else
							{
								// ...it didn't 
								// So we are done with this message.
								// Remove it from the outgoing message queue
								m_qMessagesOut.pop_front();

								// If the queue is not empty, there are more messages to send,
								// so make this happen by issuing the task to send the next header.
								if (!m_qMessagesOut.empty())
								{
									WriteHeader();
								}
							}
						}
						else
						{
							std::cout << "[" << id << "] Write Header Fail.\n";
							m_socket.close();
						}
					});
			}

			// Async - Priem context tot write a message body
			void WriteBody()
			{
				boost::asio::async_write(m_socket, boost::asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// Sending was successful
							// so we are done with the message and remove it from the queue
							m_qMessagesOut.pop_front();

							// If the queue still has messages in it,
							// then issue the task to send the next messages' header
							if (!m_qMessagesOut.empty())
							{
								WriteHeader();
							}
						}
						else
						{
							std::cout << "[" << id << "] Write Body Fail.\n";
							m_socket.close();
						}
					});
			}
			// Async - Prime context to write a message header
			void AddToIncomingMessageQueue()
			{
				if (m_nOwnerType == owner::server)
					m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });
				else
					m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });


				ReadHeader();
			}


			// encrypt
			// 클라이언트 검증
			// 일부 숫자의 패턴에 버전을 넣는다면 버전 검증용으로도 사용할 수 있음
			uint64_t scramble(uint64_t nInput)
			{
				uint64_t out = nInput ^ 0xDEADBEEFCDECAFE;
				out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
				return out ^ 0xC0DEFACE12345678;
			}

			// Async - Used by both client and server to write validation packet
			void WriteValidation()
			{
				boost::asio::async_write(m_socket, boost::asio::buffer(&m_nHandshakeOut, sizeof(uint64_t)),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// Validation data sent, clients should sit and wait for a response (or a closure)
							if (m_nOwnerType == owner::client)
								ReadHeader();

						}
						else
						{
							m_socket.close();
						}
					});
			}

			void ReadValidation(olc::net::server_interface<T>* server = nullptr)
			{
				boost::asio::async_read(m_socket, boost::asio::buffer(&m_nHandshakeIn, sizeof(uint64_t)),
					[this, server](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							if (m_nOwnerType == owner::server)
							{
								if (m_nHandshakeIn == m_nHandshakeCheck)
								{
									// CLient has proveided valid solution, so allow it to connect
									std::cout << "Client Validated" << std::endl;
									server->OnClientValidated(this->shared_from_this());


									// Sit waiting to receive data now
									ReadHeader();
								}
								else
								{
									// Client gave incorrect data, so disconnect
									std::cout << "Client Disconnected (Fail Validation)" << std::endl;
									m_socket.close();
								}
							}
							else
							{
								// Connection is a client , so solve puzzle
								m_nHandshakeOut = scramble(m_nHandshakeIn);
								// write the result
								WriteValidation();
							}
						}
						else
						{
							// Some bigger failure occured
							std::cout << "Client Disconnected (ReadValidation)" << std::endl;
							m_socket.close();
						}
					});
			}


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
			tsqueue<owned_message<T>>& m_qMessagesIn;

			// Incoming messages are constructed asynchronusly,
			// so we will store the part assembled message here, until it is ready
			message<T> m_msgTemporaryIn;

			// The "owner" decides how some of the connection behaves
			owner m_nOwnerType = owner::server;
			uint32_t id = 0;

			// Handshake Validation
			uint64_t m_nHandshakeOut = 0;
			uint64_t m_nHandshakeIn = 0;
			uint64_t m_nHandshakeCheck = 0;
		};
	}
}