#include "ServerSession.h"
#include "ChattingServer.h"


/**
 *
 **/
Session::Session(int nSessionID, boost::asio::io_service& io_service, ChatServer* pServer)
		: m_Socket(io_service)
		, m_nSessionID( nSessionID )
		, m_pServer( pServer )
{
	m_bCompletedWrite = true;
}


/**
 *
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
 *
 **/
void Session::Init()
{
	m_nPacketBufferMark = 0;
}

/**
 * 비동시 수신을 시작한다.
 **/
void Session::PostReceive()
{
	m_Socket.async_read_some
			( 
			boost::asio::buffer(m_ReceiveBuffer), 
			boost::bind( &Session::handle_receive, this, 
											boost::asio::placeholders::error, 
											boost::asio::placeholders::bytes_transferred ) 
				
		);
}

/**
 * PostSend - 비동시 송신을 시작한다.
 * 
 * 매개변수의 데이터를 복사해서 deque에 집어넣는다.
 * 그리고나서 비동기 전송을 수행한다. 
 * 만약 m_bCompleteWrite가 false면 아직 handler_write함수가 동작중이므로
 * 비동기 쓰기 작업을 수행하지 않는다.
 **/
void Session::PostSend( const int nSize, char* pData )
{

	char* pSendData = new char[nSize];
	memcpy( pSendData, pData, nSize);

	//FIXME: mutex.lock 필요
	m_SendDataQueue.push_back( pSendData );

	if( m_bCompletedWrite == false )
	{
		return;
	}
	//FIXME: mutex.unlock 필요

	boost::asio::async_write( m_Socket, boost::asio::buffer( pSendData, nSize ),
							 boost::bind( &Session::handle_write, this,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred )
							);
}

/**
 * handle_write - 비동기 전송 핸들러
 *
 * 이 함수가 호출되면 boost::asio::async_write 가 데이터를 모두 전송했다는 뜻이다.
 * 송신을 마쳤으니 송신용 메모리를 해제하고 보낸 데이터를 큐에서 뺀다.
 * 큐가 비어있지 않으면 
 *
 * 의문점: 왜 에러처리를 안했는가? 모두 보냈다는 보증이 있는가?
 *
 * @error				에러 코드
 * @bytes_transferred	전송한 바이트 수
 *
 **/
void Session::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
{
	//FIXME: mutex.lock 필요
	delete[] m_SendDataQueue.front();
	m_SendDataQueue.pop_front();

	if( m_SendDataQueue.empty() == false )
	{
		m_bCompletedWrite = false;

		char* pData = m_SendDataQueue.front();
		
		PACKET_HEADER* pHeader = (PACKET_HEADER*)pData;

		PostSend( pHeader->nSize, pData );
	}
	else
	{
		m_bCompletedWrite = true;
	}
	//FIXME: mutex.unlock 필요
}


void Session::handle_receive( const boost::system::error_code& error, size_t bytes_transferred )
{
	if( error )
	{
		if( error == boost::asio::error::eof )
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
			
			if( pHeader->nSize >= nPacketData )
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

