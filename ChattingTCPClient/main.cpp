#include <pthread.h>

#include "ChattingTCPClient.h"

static boost::asio::io_service io_service;

void *asio_thread(void *)
{
	io_service.run();

	return NULL;
}


/**
 * main - 한줄 씩 읽어서 서버로 보낸다.
 *
 * Client.Connect(endpoint);
 * Client.PostSend()를 호출한다.
 **/
int main()
{
	pthread_t tid;

	boost::asio::ip::tcp::endpoint endpoint = boost::asio::ip::tcp::endpoint( 
							boost::asio::ip::address::from_string("127.0.0.1"), 
							PORT_NUMBER);

	ChatClient Client( io_service );
	Client.Connect( endpoint );

	//boost::thread thread( boost::bind(&boost::asio::io_service::run, &io_service) );
	pthread_create(&tid, NULL, asio_thread, NULL);

		

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
	
	//thread.join();
	pthread_join(tid, NULL);
  
	std::cout << "client is terminated." << std::endl;

	return 0;
}
