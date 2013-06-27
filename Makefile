all:
	make -C ./ChattingTCPServer/
	make -C ./ChattingTCPClient/


clean:
	make clean -C ./ChattingTCPServer/
	make clean -C ./ChattingTCPClient/


distclean:
	make distclean -C ./ChattingTCPServer/
	make distclean -C ./ChattingTCPClient/
