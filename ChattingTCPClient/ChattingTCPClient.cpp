//#include <SDKDDKVer.h>
#include <deque>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "Protocol.h"



class ChatClient
{
public:
	ChatClient(boost::asio::io_service& io_service)
    : m_IOService(io_service),
	  m_Socket(io_service)
	{
		m_bIsLogin = false;
	}

	~ChatClient()
	{
		m_lock.lock();

		while( m_SendDataQueue.empty() == false )
		{
			delete[] m_SendDataQueue.front();
			m_SendDataQueue.pop_front();
		}

		m_lock.unlock();
	}
	
	bool IsConnecting() { return m_Socket.is_open(); }

	void LoginOK() { m_bIsLogin = true; }
	bool IsLogin() { return m_bIsLogin; }

	void Connect( boost::asio::ip::tcp::endpoint endpoint )
	{
		m_nPacketBufferMark = 0;

		m_Socket.async_connect( endpoint,
								boost::bind(&ChatClient::handle_connect, this,
									boost::asio::placeholders::error)
					);
	}

	void Close()
	{
		if( m_Socket.is_open() )
		{
			m_Socket.close();
		}
	}

	void PostSend( const int nSize, char* pData )
	{
		char* pSendData = new char[nSize];
		memcpy( pSendData, pData, nSize);

		m_lock.lock();

		m_SendDataQueue.push_back( pSendData );

		if( m_SendDataQueue.size() < 2 )
		{
			boost::asio::async_write( m_Socket, boost::asio::buffer( pSendData, nSize ),
									 boost::bind( &ChatClient::handle_write, this,
										boost::asio::placeholders::error,
										boost::asio::placeholders::bytes_transferred )
									);
		}

		m_lock.unlock();
	}

	

private:

	void PostReceive()
	{
		memset( &m_ReceiveBuffer, '\0', sizeof(m_ReceiveBuffer) );

		m_Socket.async_read_some
					( 
						boost::asio::buffer(m_ReceiveBuffer), 
						boost::bind( &ChatClient::handle_receive, this, 
													boost::asio::placeholders::error, 
													boost::asio::placeholders::bytes_transferred ) 
				
					);
	}

	void handle_connect(const boost::system::error_code& error)
	{
		if (!error)
		{	
			std::cout << "서버 접속 성공" << std::endl;
			std::cout << "이름을 입력하세요!!" << std::endl;

			PostReceive();
		}
		else
		{
			std::cout << "서버 접속 실패. error No: " << error.value() << " error Message: " << error.message() << std::endl;
		}
	}

	void handle_write(const boost::system::error_code& error, size_t bytes_transferred)
	{
		m_lock.lock();

		delete[] m_SendDataQueue.front();
		m_SendDataQueue.pop_front();

		char* pData = nullptr;

		if( m_SendDataQueue.empty() == false )
		{
			pData = m_SendDataQueue.front();
		}
		
		m_lock.unlock();

		
		if( pData != nullptr )
		{
			PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;
			PostSend( pHeader->nSize, pData );
		}
	}

	void handle_receive( const boost::system::error_code& error, size_t bytes_transferred )
	{
		if( error )
		{
			if( error == boost::asio::error::eof )
			{
				std::cout << "클라이언트와 연결이 끊어졌습니다" << std::endl;
			}
			else 
			{
				std::cout << "error No: " << error.value() << " error Message: " << error.message() << std::endl;
			}

			Close();
		}
		else
		{
			memcpy( &m_PacketBuffer[ m_nPacketBufferMark ], m_ReceiveBuffer.data(), bytes_transferred );
		
			int nPacketData = m_nPacketBufferMark + bytes_transferred;
			int nReadData = 0;
		
			while( nPacketData > 0 )
			{
				if( nPacketData < sizeof(PACKET_HEADER) ) 
				{
					break;
				}

				PACKET_HEADER* pHeader = (PACKET_HEADER*)&m_PacketBuffer[nReadData];
			
				if( pHeader->nSize >= nPacketData )
				{
					ProcessPacket( &m_PacketBuffer[nReadData] );
				
					nPacketData -= pHeader->nSize;
					nReadData += pHeader->nSize;
				}
				else
				{
					break;
				}
			}

			if( nPacketData > 0 )
			{
				char TempBuffer[MAX_RECEIVE_BUFFER_LEN] = {0,};
				memcpy( &TempBuffer[ 0 ], &m_PacketBuffer[nReadData], nPacketData );
				memcpy( &m_PacketBuffer[ 0 ], &TempBuffer[0], nPacketData );
			}

			m_nPacketBufferMark = nPacketData;


			PostReceive(); 
		}
	}

	void ProcessPacket( const char*pData )
	{
		PACKET_HEADER* pheader = (PACKET_HEADER*)pData;

		switch( pheader->nID )
		{
		case RES_IN:
			{
				PKT_RES_IN* pPacket = (PKT_RES_IN*)pData;
				
				LoginOK();
				
				std::cout << "클라이언트 로그인 성공 ?: " << pPacket->bIsSuccess << std::endl; 
			}
			break;
		case NOTICE_CHAT:
			{
				PKT_NOTICE_CHAT* pPacket = (PKT_NOTICE_CHAT*)pData;

				std::cout << pPacket->szName << ": " << pPacket->szMessage << std::endl; 
			}
			break;
		}
	}
 
	  

private:
	boost::asio::io_service& m_IOService;
	boost::asio::ip::tcp::socket m_Socket;

	std::array<char, 512> m_ReceiveBuffer;
	
	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN*2];

	boost::mutex m_lock;
	std::deque< char* > m_SendDataQueue;

	bool m_bIsLogin;
};


int main()
{
	boost::asio::io_service io_service;

	auto endpoint = boost::asio::ip::tcp::endpoint( 
						boost::asio::ip::address::from_string("127.0.0.1"), 
						PORT_NUMBER);

    ChatClient Client( io_service );
	Client.Connect( endpoint );

    boost::thread thread( boost::bind(&boost::asio::io_service::run, &io_service) );
		

	char szInputMessage[MAX_MESSAGE_LEN * 2] = {0,};
    
	while( std::cin.getline( szInputMessage, MAX_MESSAGE_LEN) )
    {
		//if( strnlen_s( szInputMessage, MAX_MESSAGE_LEN ) == 0 )
		if( strnlen( szInputMessage, MAX_MESSAGE_LEN ) == 0 )
		{
			break;
		}

		if( Client.IsConnecting() == false )
		{
			//std::cout << "서버와 연결되지 않았습니다" << std::endl;
			std::cout << "no server" << std::endl;
			continue;
		}

		if( Client.IsLogin() == false )
		{
			PKT_REQ_IN SendPkt;
			SendPkt.Init();
			//strncpy_s( SendPkt.szName, MAX_NAME_LEN, szInputMessage, MAX_NAME_LEN-1 );
			strncpy( SendPkt.szName, szInputMessage, MAX_NAME_LEN-1 );

			Client.PostSend( SendPkt.nSize, (char*)&SendPkt );
		}
		else
		{
			PKT_REQ_CHAT SendPkt;
			SendPkt.Init();
			//strncpy_s( SendPkt.szMessage, MAX_MESSAGE_LEN, szInputMessage, MAX_MESSAGE_LEN-1 );
			strncpy( SendPkt.szMessage, szInputMessage, MAX_MESSAGE_LEN-1 );

			Client.PostSend( SendPkt.nSize, (char*)&SendPkt );
		}
	}

	io_service.stop();

    Client.Close();
	
    thread.join();
  
	//std::cout << "클라이언트를 종료해 주세요" << std::endl;
	std::cout << "exit the client" << std::endl;

	return 0;
}
