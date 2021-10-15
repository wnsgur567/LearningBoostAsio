
// https://www.youtube.com/watch?v=2hNdkYInj4g&t=37s&ab_channel=javidx9


#ifdef __WIN32
#define __WIN32_WINNT 0x0A00
#endif


#include <iostream>
#define ASIO_STANDALONE
#include <boost/asio.hpp>

std::vector<char> vBuffer(20 * 1024);

void GrabSomeData(boost::asio::ip::tcp::socket& socket)
{
	socket.async_read_some(boost::asio::buffer(vBuffer.data(), vBuffer.size()),
		[&](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				std::cout << "\n\nRead " << length << " bytes\n\n";

				for (size_t i = 0; i < length; i++)
				{
					std::cout << vBuffer[i];
					GrabSomeData(socket);
				}
			}
		}
	);
}


int main()
{
	boost::system::error_code error_code;
	// Create a "context" - essentially the platform specific interface
	boost::asio::io_context context;

	// start context
	std::thread thrContext = std::thread(
		[&]() {context.run(); }
	);

	// Get the address of somewhere we wish to connect to
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1", error_code), 9000);

	// Tell socket to try and connect
	boost::asio::ip::tcp::socket socket(context);

	// tell socket to try and connect
	socket.connect(endpoint, error_code);

	if (!error_code)
	{
		std::cout << "Connected!" << std::endl;
	}
	else
	{
		std::cout << "Failed to connect to address:\n" << error_code.message() << std::endl;
	}

	// successful
	if (socket.is_open())
	{
		GrabSomeData(socket);

		std::string sRequest =
			"Get/index.html HTTP/1.1\r\n"
			"Host: example.com\r\n"
			"Connection: close\r\n\r\n";

		// 
		socket.wait(socket.wait_read);

		// sync io
		socket.write_some(boost::asio::buffer(sRequest.data(), sRequest.size()), error_code);

		size_t bytes = socket.available();
		std::cout << "Bytes Available: " << bytes << std::endl;

		/*if (bytes > 0)
		{
			std::vector<char> vBuffer(bytes);
			socket.read_some(boost::asio::buffer(vBuffer.data(), vBuffer.size()), error_code);

			for (auto c : vBuffer)
				std::cout << c;
		}*/
	}

	

	return 0;
}