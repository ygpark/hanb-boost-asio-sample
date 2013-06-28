#ifndef _SERVER_SESSION_H_
#define _SERVER_SESSION_H_

#include <deque>

#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <asio.hpp>
#include <pthread.h>

#include "Protocol.h"


class ChatServer;

class Session 
{
public:
	Session(int nSessionID, asio::io_service& io_service, ChatServer* pServer);
	~Session();

	int SessionID() { return m_nSessionID; }

	asio::ip::tcp::socket& Socket() { return m_Socket; }

	void Init();

	void PostReceive();
	
	void PostSend( const int nSize, char* pData );

	void SetName( const char* pszName ) { m_Name = pszName; }
	const char* GetName()				{ return m_Name.c_str(); }


private:
	void handle_write(const asio::error_code& error, size_t bytes_transferred);
	
	void handle_receive( const asio::error_code& error, size_t bytes_transferred );
	
	


	int m_nSessionID;
	asio::ip::tcp::socket m_Socket;
	
	boost::array<char, MAX_RECEIVE_BUFFER_LEN> m_ReceiveBuffer;

	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN*2];

	std::deque< char* > m_SendDataQueue;

	std::string m_Name;

	ChatServer* m_pServer;
	pthread_mutex_t m_lock;
};

#endif //_SERVER_SESSION_H_
