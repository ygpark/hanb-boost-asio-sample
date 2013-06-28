#ifndef ___CHATCLIENT_H___
#define ___CHATCLIENT_H___

#include <asio.hpp>
#include <boost/array.hpp>
#include <pthread.h>
#include <deque>

#include "Protocol.h"

class ChatClient
{
public:
	ChatClient(asio::io_service& io_service);
	~ChatClient();

	bool IsConnecting() { return m_Socket.is_open(); }
	void LoginOK() { m_bIsLogin = true; }
	bool IsLogin() { return m_bIsLogin; }

	void Connect( asio::ip::tcp::endpoint endpoint );
	void Close();
	void PostSend( const int nSize, char* pData );
	void PostReceive();

private:
	void handle_connect(const asio::error_code& error);
	void handle_write(const asio::error_code& error, size_t bytes_transferred);
	void handle_receive( const asio::error_code& error, size_t bytes_transferred );
	void ProcessPacket( const char*pData );
private:
	asio::io_service& m_IOService;
	asio::ip::tcp::socket m_Socket;

	boost::array<char, MAX_RECEIVE_BUFFER_LEN> m_ReceiveBuffer;
	
	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN*2];	//512 * 2 = 1024

	pthread_mutex_t m_lock;
	std::deque< char* > m_SendDataQueue;

	bool m_bIsLogin;
};












#endif //___CHATCLIENT_H___
