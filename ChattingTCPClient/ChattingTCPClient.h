#ifndef ___CHATCLIENT_H___
#define ___CHATCLIENT_H___

#include <boost/asio.hpp>
#include <boost/thread.hpp>	//boost::mutex
#include <deque>

#include "Protocol.h"

class ChatClient
{
public:
	ChatClient(boost::asio::io_service& io_service);
	~ChatClient();

	bool IsConnecting() { return m_Socket.is_open(); }
	void LoginOK() { m_bIsLogin = true; }
	bool IsLogin() { return m_bIsLogin; }

	void Connect( boost::asio::ip::tcp::endpoint endpoint );
	void Close();
	void PostSend( const int nSize, char* pData );
	void PostReceive();

private:
	void handle_connect(const boost::system::error_code& error);
	void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
	void handle_receive( const boost::system::error_code& error, size_t bytes_transferred );
	void ProcessPacket( const char*pData );
private:
	boost::asio::io_service& m_IOService;
	boost::asio::ip::tcp::socket m_Socket;

	std::array<char, MAX_RECEIVE_BUFFER_LEN> m_ReceiveBuffer;
	
	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN*2];	//512 * 2 = 1024

	boost::mutex m_lock;
	std::deque< char* > m_SendDataQueue;

	bool m_bIsLogin;
};












#endif //___CHATCLIENT_H___
