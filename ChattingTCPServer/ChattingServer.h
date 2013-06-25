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
 * ChatServer�� �ϴ� ��.
 * 1. Init(MAX): Session Pool�� MAX_SESSION_COUNT��ŭ �����Ѵ�.
 * 1. Start(): �񵿱� accept�� �����Ѵ�.
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
	 * Session�� Poll�� �ݳ��Ѵ�.
	 * ���� Session Pool�� �������� Accept�� ������ ���¶�� �ٽ� Accept�Ѵ�.
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
	 * �� Session���� ���� Packet�� ó���Ѵ�.
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

				std::cout << "Ŭ���̾�Ʈ �α��� ���� Name: " << m_SessionPool[ nSessionID ]->GetName() << std::endl; 

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
	 * �񵿱� Accept�� �����Ѵ�.
	 * Client�� �����ϸ� ChatServer::handle_accept�� ȣ��ȴ�.
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
	 * Client�� ���������� Session�� �ʱ�ȭ �ϰ� ������ �����Ѵ�.
	 * ������ ���� �����Ϳ� ���ؼ��� ������ �ϱ� ���� �߽��� �Ѵ�.
	 * ���� ������ �߽��ϴ� �Լ��� ����.
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



