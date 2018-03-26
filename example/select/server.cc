#include "server.h"

#include "client.h"
#include "reply.h"

namespace selectsrv{

	//���̼�ͨ����Ϣͷ
	typedef struct{
		char	m_szUserID[16];	//�û�ID
		int32	m_nMsgType;		//��Ϣ����(0:�����˳�,1:ҵ����Ϣ,2:�����̼�⵽�����ո���)
		int32	m_nFlowNo;	//��ˮ��
		uint32	m_nFuncNo;	//���ܺ�
		int32	m_nErrNo;	//������(���������0)
		uint32	m_nDataLen;	//���ݳ���
	}EMsgInfo;

	server::~server(){
		for (boost::unordered_map<int32,client *>::iterator it = _clients.begin();
			it != _clients.end(); ++it){
				if (NULL != it->second){
					delete it->second;
					it->second = NULL;
				}
		}
		_clients.clear();
	}

	void server::init_server(){
		unlink(_path.c_str());
		net::unix_addr addr(_path);

		if ((_listenfd = net::unix_socket()) < 0){
			errorlog::err_sys("[init_server]: unix_socket error");
		}
		
		if (!net::bind(_listenfd,addr)){
			errorlog::err_sys("[init_server]: bind error");
		}

		if (!net::listen(_listenfd)){
			errorlog::err_sys("[init_server]: listen error");
		}

		errorlog::err_msg("bind success,address: %s",addr.to_string().c_str());
	}


	void server::worker(){
		init_server();
		ssize_t read_count = 0, write_count = 0;
		int32 i,maxi,maxfd,connfd,sockfd;
		int32 nready,client_arr[FD_SETSIZE];
		fd_set rset, wset;
		socklen_t clilen;
		struct sockaddr_un cliaddr;
		

		//��ʼ״̬�£������׽���������ֵ��������������ֵ
		//����Ϊֻ��fd0\fd1\fd2�����ű�׼���롢��׼�������׼������ô������������ֻ�ܴ�fd3��ʼ��
		maxfd = _listenfd;//���������������п��������������ֵ
		maxi = -1;//��������ɵ��׽���

		//��ʼ��client����
		for(i=0;i<FD_SETSIZE;i++)
			client_arr[i]=-1;

		//��ʼ����������
		FD_ZERO(&_rset);
		FD_ZERO(&_wset);
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		//�������������м����׽��ֶ�Ӧ��λ
		FD_SET(_listenfd,&_rset);

		for (;;){
			rset = _rset;
			wset = _wset;
			//�ȴ�rset���������е��������ɶ�
			if((nready = select(maxfd+1, &rset, &wset, NULL, NULL)) < 0){
				if(errno == EINTR)	
					continue;       //ϵͳ�жϣ�����
				else
					errorlog::err_sys("[worker]: select error");
			}

			//�ж��Ƿ��Ǽ����׽��ֿɶ�
			if(FD_ISSET(_listenfd, &rset)){
				if((connfd = accept(_listenfd,(struct sockaddr *)&cliaddr,&clilen)) < 0)
					errorlog::err_sys("[worker]: accept error");

				//���������ӵ�������
				for(i = 0; i < FD_SETSIZE; i++){
					if(client_arr[i] < 0){
						client_arr[i] = connfd;
						break;
					}
				}

				//�½�һ���û�
				selectsrv::client *pcli = new selectsrv::client(connfd,_request_handler,_request_parser);
				_clients.insert(std::make_pair(connfd,pcli));
				
				//�ͻ�������������������������ֵ
				if(i == FD_SETSIZE)
					errorlog::err_quit("[worker]: too many clients");

				//���������������п����������׽��ֶ�Ӧ��λ
				FD_SET(connfd, &_rset);
				//����������׽���������ֵ����maxfd����ô��connfdֵ��λ���������ֵ����0��ʼ������fd0��
				maxfd = MAX(connfd, maxfd);

				//����±�i�����������������ô��i����Ϊ�������
				maxi = MAX(i, maxi);

				//���ֻ��һ�������׽���׼���ã���ô�ȴ�������������Ϊ�ɶ�
				if(--nready <= 0)
					continue;
			}

			//������Կͻ���������
			for(i = 0; i <= maxi; ++i){
				//����ֱ��������һ�������ӵ��׽���(>=0)
				if((sockfd = client_arr[i]) < 0)
					continue;

				//�ж��Ƿ����׽����������ɶ�
				if(FD_ISSET(sockfd, &rset)){
					selectsrv::client *pcli = get_client(sockfd);
					if (NULL != pcli){
						char recv_buf[MAXLINE] = { '\0' };
						if((read_count = net::read(sockfd, (void *)recv_buf, MAXLINE)) <= 0){
							close_client(sockfd);//�����ȡ���ļ�β����ô�ر���������ӵ��׽���
							client_arr[i] = -1;//�����ڱ����������׽����������������г�ʼ����Ӧ������
							errorlog::err_msg("read error has occured");
						}else{
							pcli->rdata(recv_buf, read_count);
							if (!pcli->handle_request()){//asynchronous handle
								close_client(sockfd);//�����ȡ���ļ�β����ô�ر���������ӵ��׽���
								client_arr[i] = -1;//�����ڱ����������׽����������������г�ʼ����Ӧ������
								errorlog::err_msg("handle_request error has occured");
							}
						}

						//���û�пɶ����׽�������������ô����ѭ��
						if(--nready<=0) break;
					}
				}

				//if (FD_ISSET(sockfd, &wset)){
				//	selectsrv::client *pcli = get_client(sockfd);
				//	if (NULL != pcli) {
				//		StreamBuf &reply_msg = pcli->wbuf();
				//		if (0 < reply_msg.size()){
				//			write_count = net::write(pcli->fd(), (const void *)reply_msg.data(), reply_msg.size());
				//			if (write_count < 0) {
				//				if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				//					FD_SET(sockfd, &_wset);//continue to writing
				//				}else {
				//					close_client(sockfd);//�����ȡ���ļ�β����ô�ر���������ӵ��׽���
				//					client_arr[i] = -1;//�����ڱ����������׽����������������г�ʼ����Ӧ������
				//					errorlog::err_msg("write error has occured");
				//				}
				//			}else if (write_count == 0) {
				//				close_client(sockfd);//�����ȡ���ļ�β����ô�ر���������ӵ��׽���
				//				client_arr[i] = -1;//�����ڱ����������׽����������������г�ʼ����Ӧ������
				//				errorlog::err_msg("closed by peer");
				//			}else {
				//				reply_msg.clear_head(write_count);
				//			}
				//		}else {
				//			errorlog::err_msg("all data has been send");
				//			FD_CLR(sockfd, &_wset);//������������������������������Ӧ��λ
				//		}
				//	}
				//}

			}//for
		}//for(;;)
	}

	//bool server::handle_request(client *pcli){
	//	safebuf &cli_inbuf = pcli->inbuf();
	//	errorlog::err_msg("handle request %d bytes data: %s",cli_inbuf.size(),cli_inbuf.to_string().c_str());

	//	safebuf &reply_msg = pcli->outbuf();
	//	reply_msg << "handle request " << cli_inbuf.size() << " bytes data,I'm tied up at work,talk to you then.";
	//	cli_inbuf.clear_head(cli_inbuf.size());

	//	int32 write_count = net::write(pcli->fd(), (const void *)reply_msg.data(), reply_msg.size());
	//	if (write_count > 0){
	//		reply_msg.clear_head(write_count);
	//		if (!reply_msg.empty()){
	//			FD_SET(pcli->fd(), &_wset);//continue to writing
	//		}
	//	}

	//	errorlog::err_msg("has been send [%d] bytes data,out buffer left [%d] bytes data.", write_count, reply_msg.size());

	//	for (;;){
	//		request req;
	//		bool bre = false;
	//		switch (_request_parser.parser(cli_inbuf, req)) {
	//		case ERROR:
	//			bre = true;
	//			break;
	//		case INPROGRESS:
	//			bre = true;
	//			break;
	//		case HASDONE: 
	//			{
	//				reply rep;
	//				_request_handler.handle_request(req, rep);

	//			}
	//			bre = true;
	//			break;
	//		case HASLEFT:
	//			break;
	//		default:
	//			break;
	//		}

	//		if (bre) break;
	//	}


	//	return true;
	//}


}