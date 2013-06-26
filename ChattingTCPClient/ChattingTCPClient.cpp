//#include <SDKDDKVer.h>
#include <deque>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "Protocol.h"


#define USE_DEBUG

#ifdef USE_DEBUG
    #define MODULE_NAME "[debug] "
    #define debug(fmt, arg...)    do{printf(MODULE_NAME  "%s:%d: " fmt, __func__, __LINE__ , ##arg); }while(0)
#else
    #define debug            do{}while(0)
#endif
/**
 * ChatClient class - 
 **/
class ChatClient
{
public:
	/**
	 * 생성자 - 
	 **/
	ChatClient(boost::asio::io_service& io_service)
	: m_IOService(io_service),
	  m_Socket(io_service)
	{
		m_bIsLogin = false;
	}

	/**
	 * 소멸자 - 
	 **/
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

	/**
	 * Connect - 비동기적으로 서버에 접속한다.
	 *
	 * 접속이 완료되거나 에러가 발생하면 handle_connect()함수가 호출된다.
	 **/
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

	/**
	 * PostSend - 비동기적으로 서버에 데이터를 보낸다.
	 *
	 * 접속이 완료되거나 에러가 발생하면 handle_connect()함수가 호출된다.
	 **/
	void PostSend( const int nSize, char* pData )
	{
		char* pSendData = new char[nSize];
		memcpy( pSendData, pData, nSize);

		m_lock.lock();

		m_SendDataQueue.push_back( pSendData );

		/**
		 * 큐 사이즈가 1인 경우만 async_write를 호출.
		 * 그보다 큰 경우는 handle_write()에서 boost::asio::async_write를 호출할 것임.
		 **/
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

	/**
	 * PostReceive - 최초 연결시 handle_connect함수에서 단 한번만 호출된다.
	 **/
	void PostReceive()
	{
		memset( &m_ReceiveBuffer, '\0', sizeof(m_ReceiveBuffer) );

		m_Socket.async_read_some( boost::asio::buffer(m_ReceiveBuffer), 
					  boost::bind( &ChatClient::handle_receive,
						this, 
						boost::asio::placeholders::error, 
						boost::asio::placeholders::bytes_transferred ) 
					);
	}

	/**
	 * PostSend - 비동기적으로 서버에 데이터를 보낸다.
	 *
	 * 접속이 완료되거나 에러가 발생하면 handle_connect()함수가 호출된다.
	 **/
	void handle_connect(const boost::system::error_code& error)
	{
		if (!error)
		{	
			std::cout << "Connected with the server." << std::endl;
			std::cout << "Name:" << std::endl;

			PostReceive();
		}
		else
		{
			std::cout << "Can not connect to the server. error No: " << error.value() << " error Message: " << error.message() << std::endl;
		}
	}

	void handle_write(const boost::system::error_code& error, size_t bytes_transferred)
	{
		m_lock.lock();

		/* 보낸 메모리를 해제하고 포인터를 버림. */
		delete[] m_SendDataQueue.front();
		m_SendDataQueue.pop_front();

		char* pData = nullptr;

		/* 보낼 데이터가 있으면 포인터를 꺼냄  */
		if( m_SendDataQueue.empty() == false )
		{
			pData = m_SendDataQueue.front();
		}
		
		m_lock.unlock();

		
		/* 포인터가 NULL인지 아닌지 확인해 보고 PostSend()로 보냄. */
		if( pData != nullptr )
		{
			PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;
			PostSend( pHeader->nSize, pData );
		}
	}

	/**
	 * handle_receive - boost::asio::read_some함수의 완료함수.
	 * 
	 * read_some()함수는 특별히 읽는 사이즈를 정하지 않기 때문에 읽고나서 잘라야한다.
	 * 수신된 데이터는 m_ReceiveBuffer(512 byte)에 들어있다.
	 **/
	void handle_receive( const boost::system::error_code& error, size_t bytes_transferred )
	{
		debug("start.....\n");
		if( error )
		{
			debug("error\n");
			/**
			 * 에러처리
			 **/
			if( error == boost::asio::error::eof ) {
				std::cout << "Disconnected with server." << std::endl;
			} else {
				std::cout << "error No: " << error.value() << " error Message: " << error.message() << std::endl;
			}

			Close();
		}
		else
		{
			/**
			 * 데이터 수신
			 **/

			//
			//
			// 한패킷 사이즈가 150인데 2패킷+20을 받은 경우를 생각해 보자.
			//
			// 1. 320 byte 수신
			//
			//	nPacketData = 320
			//      +---------+--------+----+-------------+
			//      |   150   |   150  | 20 |             |
			//      +------------------+----+-------------+
			//      \
			//       nReadData       
			//
			// 2. 루프를 돌면서 패킷을 처리한다.
			//    처리한 패킷 사이즈를 nReadData에 표시한다.
			//    처리하고 남은 데이터를 nPacketData에 기록한다.
			//    
			//	nPacketData = 170
			//      +---------+--------+----+-------------+
			//      | 처리됨  |   150  | 20 |             |
			//      +------------------+----+-------------+
			//                 \
			//                  nReadData       
			//
			//
			//	nPacketData = 20
			//      +---------+--------+----+-------------+
			//      | 처리됨  | 처리됨 | 20 |             |
			//      +------------------+----+-------------+
			//                          \
			//                           nReadData
			//
			//  3. 자투리 데이터를 버퍼의 맨 앞으로 옮기고 수신을 계속한다.
			//     마지막으로 다음번 수신때 자투리 데이터에 이어서 수신하기 위해 
			//     m_nPacketBufferMark에 nPacketData를 저장하고 끝냄.
			//
			//	nPacketData = 20
			//      m_nPacketBufferMark = nPacketData
			//      +-----+-------------------------------+
			//      | 20  |                               |
			//      +-------------------------------------+
			//
			memcpy( &m_PacketBuffer[ m_nPacketBufferMark ], m_ReceiveBuffer.data(), bytes_transferred );

			/* m_PaacketBuffer에서 처리하지 않은 데이터의 양 */
			int nPacketData = m_nPacketBufferMark + bytes_transferred;
			/* m_PacketBuffer에서 처리한 데이터 위치 */
			int nReadData = 0;
		
			debug("nPacketDataSize = %d\n", nPacketData);/*150*/

			// 2패킷 이상 수신한 경우 루프를 돌며 모두 처리한다.
			while( nPacketData > 0 )
			{
				//더이상 처리할 패킷이 없으면 루프 종료.
				//자투리는 루프가 끝나면 임시버퍼에 저장
				if( nPacketData < sizeof(PACKET_HEADER) ) 
				{
					break;
				}

				PACKET_HEADER* pHeader = (PACKET_HEADER*)&m_PacketBuffer[nReadData];
			

				//경고: 책에 부등호가 반대로 써져있으니 주의
				//
				//딱 패킷 사이즈 만큼 받았거나 패킷 사이즈보다 더 많이 받았다면 처리
				if( pHeader->nSize <= nPacketData )
				{
					ProcessPacket( &m_PacketBuffer[nReadData] );
				
					nPacketData -= pHeader->nSize; /* 처리한 데이터만큼 뺀다. */
					nReadData += pHeader->nSize;
				} else {
					break;
				}
			}

			//처리하고 남은 자투리 데이터 처리
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

	std::array<char, MAX_RECEIVE_BUFFER_LEN> m_ReceiveBuffer;
	
	int m_nPacketBufferMark;
	char m_PacketBuffer[MAX_RECEIVE_BUFFER_LEN*2];	//512 * 2 = 1024

	boost::mutex m_lock;
	std::deque< char* > m_SendDataQueue;

	bool m_bIsLogin;
};

/**
 * main - 한줄 씩 읽어서 서버로 보낸다.
 *
 * Client.Connect(endpoint);
 * Client.PostSend()를 호출한다.
 **/
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
    
	while( std::cin.getline( szInputMessage, MAX_MESSAGE_LEN) ) {

		/**
		 * 입력 데이터가 없으면 종료
		 **/
		//if( strnlen_s( szInputMessage, MAX_MESSAGE_LEN ) == 0 )
		if( strnlen( szInputMessage, MAX_MESSAGE_LEN ) == 0 )
		{
			break;
		}

		/**
		 * 접속하기전에 입력이 있으면 메시지 출력 후 대기
		 **/
		if( Client.IsConnecting() == false )
		{
			//std::cout << "서버와 연결되지 않았습니다" << std::endl;
			std::cout << "server not found" << std::endl;
			continue;
		}

		/**
		 * 채팅방 닉네임을 서버로 전송하지 않았으면 서버로 전송
		 **/
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
			/**
			 * 닉네임을 설정했으면 본격적으로 데이터 전송 시작
			 * Client.PostSend() 계속 호출
			 **/
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
  
	std::cout << "client is terminated." << std::endl;

	return 0;
}
