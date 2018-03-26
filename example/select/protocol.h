#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#include "../util/util.h"

namespace selectsrv {
#pragma pack(push, 1)
	typedef struct {
		char	_ueser_id[16];	//�û�ID
		int32	_msg_type;		//��Ϣ����(0:�����˳�,1:ҵ����Ϣ,2:�����̼�⵽�����ո���)
		int32	_flow_no;		//��ˮ��
		uint32	_func_no;		//���ܺ�
		int32	_errorno;		//������(���������0)
		uint32	_bodylen;		//���ݳ���
	}protocol;
#pragma pack(pop)

	static const int32 PROTOCOL_LEN = sizeof(protocol);
}

#endif