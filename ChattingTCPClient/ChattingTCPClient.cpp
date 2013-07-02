#include <boost/bind.hpp>

#include "ChattingTCPClient.h"


#ifdef CONFIG_PRINT_DEBUG_MSG
	#define MODULE_NAME "[debug] "
	#define debug(fmt, arg...)    do{printf(MODULE_NAME  "%s:%d: " fmt, __func__, __LINE__ , ##arg); }while(0)
#else
    #define debug            do{}while(0)
#endif

/**
 * 생성자 - 
 **/
ChatClient::ChatClient(asio::io_service& io_service)
	: m_IOService(io_service),
	  m_Socket(io_service)
{
	m_bIsLogin = false;
	pthread_mutex_init(&m_lock, NULL);
}

/**
 * 소멸자 - 
 **/
ChatClient::~ChatClient()
{
	pthread_mutex_lock(&m_lock);

	while( m_SendDataQueue.empty() == false )
	{
		delete[] m_SendDataQueue.front();
		m_SendDataQueue.pop_front();
	}

	pthread_mutex_unlock(&m_lock);
}
	
/**
 * Connect - 비동기적으로 서버에 접속한다.
 *
 * 접속이 완료되거나 에러가 발생하면 handle_connect()함수가 호출된다.
 **/
void ChatClient::Connect( asio::ip::tcp::endpoint endpoint )
{
	m_nPacketBufferMark = 0;

	m_Socket.async_connect( endpoint,
				boost::bind(&ChatClient::handle_connect, this,
					asio::placeholders::error)
				);
}

/**
 * Close - 연결된 소켓을 닫는다.
 **/
void ChatClient::Close()
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
void ChatClient::PostSend( const int nSize, char* pData )
{
	char* pSendData = new char[nSize];
	memcpy( pSendData, pData, nSize);

	pthread_mutex_lock(&m_lock);

	m_SendDataQueue.push_back( pSendData );

	/**
	 * 큐 사이즈가 1인 경우만 async_write를 호출.
	 * 그보다 큰 경우는 handle_write()에서 asio::async_write를 호출할 것임.
	 **/
	if( m_SendDataQueue.size() < 2 )
	{
		asio::async_write( m_Socket, asio::buffer( pSendData, nSize ),
					  boost::bind( &ChatClient::handle_write, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred )
					);
	}

	pthread_mutex_unlock(&m_lock);
}

	


/**
 * PostReceive - 최초 연결시 handle_connect함수에서 단 한번만 호출된다.
 **/
void ChatClient::PostReceive()
{
	memset( &m_ReceiveBuffer, '\0', sizeof(m_ReceiveBuffer) );

	m_Socket.async_read_some( asio::buffer(m_ReceiveBuffer), 
				  boost::bind( &ChatClient::handle_receive,
					this, 
					asio::placeholders::error, 
					asio::placeholders::bytes_transferred ) 
				);
}

/**
 * PostSend - 비동기적으로 서버에 데이터를 보낸다.
 *
 * 접속이 완료되거나 에러가 발생하면 handle_connect()함수가 호출된다.
 **/
void ChatClient::handle_connect(const asio::error_code& error)
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

void ChatClient::handle_write(const asio::error_code& error, size_t bytes_transferred)
{
	pthread_mutex_lock(&m_lock);

	/* 보낸 메모리를 해제하고 포인터를 버림. */
	delete[] m_SendDataQueue.front();
	m_SendDataQueue.pop_front();

	char* pData = NULL;

	/* 보낼 데이터가 있으면 포인터를 꺼냄  */
	if( m_SendDataQueue.empty() == false )
	{
		pData = m_SendDataQueue.front();
	}
	
	pthread_mutex_unlock(&m_lock);

	
	/* 포인터가 NULL인지 아닌지 확인해 보고 PostSend()로 보냄. */
	if( pData != NULL )
	{
		PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;
		asio::async_write( m_Socket, asio::buffer( pData, pHeader->nSize ),
					  boost::bind( &ChatClient::handle_write, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred )
					);
	}
}

/**
 * handle_receive - asio::read_some함수의 완료함수.
 * 
 * read_some()함수는 특별히 읽는 사이즈를 정하지 않기 때문에 읽고나서 잘라야한다.
 * 수신된 데이터는 m_ReceiveBuffer(512 byte)에 들어있다.
 **/
void ChatClient::handle_receive( const asio::error_code& error, size_t bytes_transferred )
{
	if( error )
	{
		/**
		 * 에러처리
		 **/
		if( error == asio::error::eof ) {
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
		//      `.
		//        nReadData       
		//
		// 2. 루프를 돌면서 패킷을 처리한다.
		//    처리한 패킷 사이즈를 nReadData에 표시한다.
		//    처리하고 남은 데이터를 nPacketData에 기록한다.
		//    
		//	nPacketData = 170
		//      +---------+--------+----+-------------+
		//      | 처리됨  |   150  | 20 |             |
		//      +------------------+----+-------------+
		//                 `.
		//                  nReadData       
		//
		//
		//	nPacketData = 20
		//      +---------+--------+----+-------------+
		//      | 처리됨  | 처리됨 | 20 |             |
		//      +------------------+----+-------------+
		//                          `.
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
		size_t nPacketData = m_nPacketBufferMark + bytes_transferred;
		/* m_PacketBuffer에서 처리한 데이터 위치 */
		int nReadData = 0;
	
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
			if( pHeader->nSize <= (int)nPacketData )
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

void ChatClient::ProcessPacket( const char*pData )
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
 


