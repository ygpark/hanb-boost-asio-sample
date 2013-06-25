#pragma once 

#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <string>
#include <vector>

#include "ServerSession.h"
#include "Protocol.h"

/*
 * ChatServer가 하는 일.
 * 1. Init(MAX): Session Pool을 MAX_SESSION_COUNT만큼 생성한다.
 * 1. Start(): 비동기 accept를 시작한다.
 * 2. 
 *
 *
 *
 *
 *
 * */

class ChatServer
{
public:
	ChatServer( boost::asio::io_service& io_service )
		: m_acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT_NUMBER))
	{
		m_bIsAccepting = false;
	}

	~ChatServer()
	{
		for( size_t i = 0; i < m_SessionPool.size(); ++i )
		{
			if( m_SessionPool[i]->Socket().is_open() )
			{
				m_SessionPool[i]->Socket().close();
			}

			delete m_SessionPool[i];
		}
	}

	void Init( const int nMaxSessionCount )
	{
		for( int i = 0; i < nMaxSessionCount; ++i )
		{
			Session* pSession = new Session( i, m_acceptor.get_io_service(), this );
			m_SessionPool.push_back( pSession );
			m_SessionTickets.push_back( i );
		}
	}

	void Start()
	{
		std::cout << "Start Server....." << std::endl;

		PostAccept();
	}

	/**
	 * Session을 Poll에 반납한다.
	 * 만일 Session Pool이 가득차서 Accept가 중지된 상태라면 다시 Accept한다.
	 **/
	void CloseSession( const int nSessionID )
	{
		std::cout << "Client is disconnected. Session ID: " << nSessionID << std::endl;

		m_SessionTickets.push_back( nSessionID );

		if( m_bIsAccepting == false )
		{
			PostAccept();
		}
	}

	/**
	 * 각 Session에서 받은 Packet을 처리한다.
	 **/
	void ProcessPacket( const int nSessionID, const char*pData )
	{
		PACKET_HEADER* pheader = (PACKET_HEADER*)pData;

		switch( pheader->nID )
		{
		case REQ_IN:
			{
				PKT_REQ_IN* pPacket = (PKT_REQ_IN*)pData;
				m_SessionPool[ nSessionID ]->SetName( pPacket->szName );

				std::cout << "클라이언트 로그인 성공 Name: " << m_SessionPool[ nSessionID ]->GetName() << std::endl; 

				PKT_RES_IN SendPkt;
				SendPkt.Init();
				SendPkt.bIsSuccess = true;
				
				m_SessionPool[ nSessionID ]->PostSend( SendPkt.nSize, (char*)&SendPkt ); 
			}
			break;
		case REQ_CHAT:
			{
				PKT_REQ_CHAT* pPacket = (PKT_REQ_CHAT*)pData;

				PKT_NOTICE_CHAT SendPkt;
				SendPkt.Init();
				//strncpy_s( SendPkt.szName, MAX_NAME_LEN, m_SessionPool[ nSessionID ]->GetName(), MAX_NAME_LEN-1 );
				//strncpy_s( SendPkt.szMessage, MAX_MESSAGE_LEN, pPacket->szMessage, MAX_MESSAGE_LEN-1 );
				strncpy( SendPkt.szName, m_SessionPool[ nSessionID ]->GetName(), MAX_NAME_LEN-1 );
				strncpy( SendPkt.szMessage, pPacket->szMessage, MAX_MESSAGE_LEN-1 );

				size_t nTotalSessionCount = m_SessionPool.size();
				
				for( size_t i = 0; i < nTotalSessionCount; ++i )
				{
					if( m_SessionPool[ i ]->Socket().is_open() )
					{
						m_SessionPool[ i ]->PostSend( SendPkt.nSize, (char*)&SendPkt ); 
					}
				}
			}
			break;
		}

		return;
	}


private:
	/**
	 * 비동기 Accept를 시작한다.
	 * Client가 접속하면 ChatServer::handle_accept가 호출된다.
	 **/
	bool PostAccept()
	{
		if( m_SessionTickets.empty() )
		{
			m_bIsAccepting = false;
			return false;
		}
				
		m_bIsAccepting = true;
		int nSessionID = m_SessionTickets.front();

		m_SessionTickets.pop_front();
		m_acceptor.async_accept( m_SessionPool[nSessionID]->Socket(),
								 boost::bind(&ChatServer::handle_accept, 
												this, 
												m_SessionPool[nSessionID],
												boost::asio::placeholders::error)
								);

		return true;
	}

	/**
	 * Client가 접속했으면 Session을 초기화 하고 수신을 시작한다.
	 * 서버는 받은 데이터에 대해서만 응답을 하기 위해 발신을 한다.
	 * 따라서 별도로 발신하는 함수는 없다.
	 **/
	void handle_accept(Session* pSession, const boost::system::error_code& error)
	{
		if (!error)
		{	
			std::cout << "Client is connected. SessionID: " << pSession->SessionID() << std::endl;
			
			pSession->Init();
			pSession->PostReceive();
			
			PostAccept();
		}
		else 
		{
			std::cout << "error No: " << error.value() << " error Message: " << error.message() << std::endl;
		}
	}


	


	int m_nSeqNumber;
	
	bool m_bIsAccepting;

	boost::asio::ip::tcp::acceptor m_acceptor;

	std::vector< Session* > m_SessionPool;
	std::deque< int > m_SessionTickets;
	
};



