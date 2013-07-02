/**
 * ServerSession.cpp
 *
 * Session - Client당 하나씩 생성되며 실제로 데이터 송, 수신이 이루어 진다.
 *           데이터는 보내기 전에 별도의 큐에 복사한 뒤 보낸다.
 * 
 *
 * 추정하는 버그:
 *	1.  handle_write()함수에 PostSend대신 asio::async_write 가 사용되어야 함
 *	2. 1이 동작하려면 PostSend(), handle_write()함수에 mutex걸려야 함.
 **/
#include "ServerSession.h"
#include "ChattingServer.h"


#ifdef CONFIG_PRINT_DEBUG_MSG
    #define MODULE_NAME "[debug] "
    #define debug(fmt, arg...)    do{printf(MODULE_NAME  "%s:%d: " fmt, __func__, __LINE__ , ##arg); }while(0)
#else
    #define debug            do{}while(0)
#endif

/**
 * 생성자 - 공유 변수 초기화
 **/
Session::Session(int nSessionID, asio::io_service& io_service, ChatServer* pServer)
		: m_Socket(io_service)
		, m_nSessionID( nSessionID )
		, m_pServer( pServer )
{
	pthread_mutex_init(&m_lock, NULL);
}


/**
 * 소멸자 - 데이터 버퍼의 메모리를 해제하고 큐를 비운다.
 **/
Session::~Session()
{
	while( m_SendDataQueue.empty() == false )
	{
		delete[] m_SendDataQueue.front();
		m_SendDataQueue.pop_front();
	}
}

/**
 * Init - 
 **/
void Session::Init()
{
	m_nPacketBufferMark = 0;
}

/**
 * PostReceive - 비동시 수신을 시작한다.
 **/
void Session::PostReceive()
{
	m_Socket.async_read_some
			( 
			asio::buffer(m_ReceiveBuffer), 
			boost::bind( &Session::handle_receive, this, 
											asio::placeholders::error, 
											asio::placeholders::bytes_transferred ) 
				
		);
}

/**
 * PostSend - 비동시 송신을 시작한다.
 * 
 * 보낼 데이터를 복사한 뒤 그 복사본으로 전송한다.
 * 만일 전송이 완료되지 않았으면 큐에 데이터를 추가하고 즉시 리턴한다.
 * 
 * 여기서 엔디언Endian은 고려하지 않아도 된다.htonl()같은 함수를 호출하지 않는다는 말이다.
 * 문자열은 1byte로 쪼개져서 가기 때문이다.
 *
 **/
void Session::PostSend( const int nSize, char* pData )
{
	char* pSendData = new char[nSize];
	memcpy( pSendData, pData, nSize);

	pthread_mutex_lock(&m_lock);
	m_SendDataQueue.push_back( pSendData );

	if (m_SendDataQueue.size() < 2) {
		asio::async_write( m_Socket, 
					  asio::buffer( pSendData, nSize ),
					  boost::bind( &Session::handle_write, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred )
					  );
	}
	pthread_mutex_unlock(&m_lock);
}

/**
 * handle_write - 비동기 전송 핸들러
 *
 * asio::async_write함수가 데이터를 모두 보냈거나 에러가 발생하면 자동으로 호출된다.
 * 송신을 마쳤으니 송신용 메모리를 해제하고 보낸 데이터를 큐에서 뺀다.
 *
 * 큐에 보낼 데이터가 있으면 보내기를 계속하고 
 *
 * 의문점: 왜 에러처리를 안했는가? 모두 보냈다는 보증이 있는가?
 *
 * @error				에러 코드
 * @bytes_transferred	전송한 바이트 수
 *
 **/
void Session::handle_write(const asio::error_code& error, size_t bytes_transferred)
{
	pthread_mutex_lock(&m_lock);

	delete[] m_SendDataQueue.front();
	m_SendDataQueue.pop_front();
	debug("queue size: %d\n", m_SendDataQueue.size());
	
	if( m_SendDataQueue.empty() == false )
	{
		char* pData = m_SendDataQueue.front();
		
		PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;

		asio::async_write( m_Socket, 
					  asio::buffer( pData, pHeader->nSize ),
					  boost::bind( &Session::handle_write, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred )
					);
	}

	pthread_mutex_unlock(&m_lock);
}


void Session::handle_receive( const asio::error_code& error, size_t bytes_transferred )
{
	if( error )
	{
		if( error == asio::error::eof )
		{
			std::cout << "client is disconnected." << std::endl;
		}
		else 
		{
			std::cout << "error No: " << error.value() << " error Message: " << error.message() << std::endl;
		}

		m_pServer->CloseSession( m_nSessionID );
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

			//경고: 책에 부등호가 반대로 써져있으니 주의
			if( pHeader->nSize <= nPacketData )
			{
				m_pServer->ProcessPacket( m_nSessionID, &m_PacketBuffer[nReadData] );
				
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

