#pragma once 
#ifndef _CHATTING_SERVER_H_
#define _CHATTING_SERVER_H_


#include <deque>
#include <vector>
#include <asio.hpp>
#include "Protocol.h"

class Session;

class ChatServer
{
public:
	ChatServer( asio::io_service& io_service );
	~ChatServer();

	void Init( const int nMaxSessionCount );
	void Start();
	void CloseSession( const int nSessionID );
	void ProcessPacket( const int nSessionID, const char*pData );


private:
	bool PostAccept();
	void handle_accept(Session* pSession, const asio::error_code& error);

private:
	int m_nSeqNumber;
	bool m_bIsAccepting;
	
	std::vector< Session* > m_SessionPool;
	std::deque< int > m_SessionTickets;
	
	asio::ip::tcp::acceptor m_acceptor;
};



#endif //_CHATTING_SERVER_H_
