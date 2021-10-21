#include<olc_net.h>
#include<iostream>

enum class CustomMsgTypes : uint32_t
{
	ServerAccept,
	ServerDeny,
	ServerPing,
	MessageAll,
	ServerMessage,
	TestInt
};

class CustomClient : public olc::net::client_interface<CustomMsgTypes>
{
public:
	void PingServer()
	{
		std::cout << "Ping Request\n";

		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::ServerPing;

		std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();

		msg << timeNow;
		Send(msg);
	}

	void MessageAll()
	{
		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::MessageAll;
		Send(msg);
	}

	void TestInt()
	{
		std::cout << "Test Int\n";

		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::TestInt;

		int n = rand() % 15;

		msg << n;
		Send(msg);
	}
};

int main()
{
	CustomClient c;
	c.Connect("127.0.0.1", 60000);

	bool key[3] = { false,false,false };
	bool old_key[3] = { false,false,false };

	bool bQuit = false;
	while (!bQuit)
	{
		if (GetForegroundWindow() == GetConsoleWindow())
		{
			key[0] = GetAsyncKeyState('1') & 0x8000;
			key[1] = GetAsyncKeyState('2') & 0x8000;
			key[2] = GetAsyncKeyState('3') & 0x8000;
		}

		if (key[0] && !old_key[0])
			c.TestInt();
		if (key[1] && !old_key[1])
			c.MessageAll();
		if (key[2] && !old_key[2]) bQuit = true;

		for (int i = 0; i < 3; i++) old_key[i] = key[i];


		if (c.IsConnected())
		{
			if (!c.Incoming().empty())
			{
				auto msg = c.Incoming().pop_front().msg;

				switch (msg.header.id)
				{
				case CustomMsgTypes::ServerAccept:
				{
					std::cout << "Server Accepted Connection\n";
				}
				break;
				case CustomMsgTypes::ServerPing:
				{
					// Server has responded to a ping request
					std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
					std::chrono::system_clock::time_point timeThen;
					msg >> timeThen;
					std::cout << "Ping: " << std::chrono::duration<double>(timeNow - timeThen).count() << "\n";
				}
				break;
				case CustomMsgTypes::ServerMessage:
				{
					uint32_t clientID;
					msg >> clientID;
					std::cout << "Hello from [" << clientID << "]\n";
				}
				break;

				case CustomMsgTypes::TestInt:
				{
					int n;
					msg >> n;
					std::cout << "num : " << n << "\n";
				}
				break;
				}
			}
		}
		else
		{
			std::cout << "Server Down\n";
			bQuit = true;
		}

	}

	return 0;
}