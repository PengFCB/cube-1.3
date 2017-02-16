#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "data_type.h"
#include "errno.h"
#include "alloc.h"
#include "string.h"
#include "basefunc.h"
#include "struct_deal.h"
#include "crypto_func.h"
#include "memdb.h"
#include "message.h"
#include "ex_module.h"
#include "tesi.h"

#include "file_struct.h"
#include "tesi_key.h"
#include "tesi_aik_struct.h"
#include "aik_client.h"

static struct timeval time_val={0,50*1000};
int print_error(char * str, int result)
{
	printf("%s %s",str,tss_err_string(result));
}

struct aik_proc_pointer
{
	TSS_HKEY hAIKey;
	TSS_HKEY hSignKey;
	TESI_SIGN_DATA * aik_cert;
};

int aik_client_init(void * sub_proc,void * para)
{
	int ret;
	TSS_RESULT result;	
	char local_uuid[DIGEST_SIZE*2+1];
	
	struct aik_proc_pointer * aik_pointer;
	void * context;
	aik_pointer= malloc(sizeof(struct aik_proc_pointer));
	if(aik_pointer==NULL)
		return -ENOMEM;
	memset(aik_pointer,0,sizeof(struct aik_proc_pointer));


	result=TESI_Local_ReloadWithAuth("ooo","sss");
	if(result!=TSS_SUCCESS)
	{
		printf("open tpm error %d!\n",result);
		return -ENFILE;
	}
	result= TESI_Local_GetPubEK("pubkey/pubek","ooo");
	if(result!=TSS_SUCCESS)
	{
		printf("get pubek error %d!\n",result);
		return -EIO;
	}
	ret=ex_module_setpointer(sub_proc,aik_pointer);
	if(ret<0)
		return ret;
	return 0;
}

int aik_client_start(void * sub_proc,void * para)
{
	int ret;
	int retval;
	void * recv_msg;
	void * send_msg;
	void * context;
	int i;
	int type;
	int subtype;

	char local_uuid[DIGEST_SIZE*2+1];
	char proc_name[DIGEST_SIZE*2+1];
	
	ret=proc_share_data_getvalue("uuid",local_uuid);
	ret=proc_share_data_getvalue("proc_name",proc_name);

	printf("begin aik process start!\n");

	for(i=0;i<300*1000;i++)
	{
		usleep(time_val.tv_usec);
		ret=ex_module_recvmsg(sub_proc,&recv_msg);
		if(ret<0)
			continue;
		if(recv_msg==NULL)
			continue;

 		type=message_get_type(recv_msg);
		subtype=message_get_subtype(recv_msg);
		
		if((type==DTYPE_FILE_TRANS)&&(subtype==SUBTYPE_FILE_NOTICE))
		{
			proc_aik_activate(sub_proc,recv_msg);
		}
		else
		{
			proc_aik_request(sub_proc,recv_msg);
		}
	}

	return 0;
};


int proc_aik_request(void * sub_proc,void * recv_msg)
{
	TSS_RESULT result;
	TSS_HKEY 	hSignKey;
	TSS_HKEY	hAIKey, hCAKey;
	struct aik_cert_info reqinfo;
	struct policyfile_data * reqdata;
	int ret;
	struct aik_user_info * user_info;

	user_info=memdb_get_first_record(DTYPE_AIK_STRUCT,SUBTYPE_AIK_USER_INFO);
	if(user_info==NULL)
		return -EINVAL;

	memcpy(&reqinfo.user_info,user_info,sizeof(struct aik_user_info));

/*
	BYTE		*labelString = "UserA";
	UINT32		labelLen = strlen(labelString) + 1;
*/
	char filename[DIGEST_SIZE*5];
	char local_uuid[DIGEST_SIZE];
	char proc_name[DIGEST_SIZE];
	
	ret=proc_share_data_getvalue("uuid",local_uuid);
	ret=proc_share_data_getvalue("proc_name",proc_name);

	printf("begin aik request!\n");
	char buffer[1024];
	char digest[DIGEST_SIZE];
	int blobsize=0;
	int fd;
	// create a signkey and write its key in localsignkey.key, write its pubkey in localsignkey.pem
	result=TESI_Local_ReloadWithAuth("ooo","sss");

	calculate_sm3("pubkey/pubek.pem",reqinfo.pubkey_uuid);
	Memcpy(reqinfo.machine_uuid,local_uuid,DIGEST_SIZE);
	
	// create info blob
	void * struct_template=memdb_get_template(DTYPE_AIK_STRUCT,SUBTYPE_AIK_CERT_INFO);
	if(struct_template==NULL)
		return -EINVAL;
	blobsize=struct_2_blob(&reqinfo,buffer,struct_template);

	// Load the CA Key
	result=TESI_Local_GetPubKeyFromCA(&hCAKey,"cert/CA");
	if (result != TSS_SUCCESS) {
		printf("Get pubkey error %s!\n", tss_err_string(result));
		exit(result);
	}
	
	TESI_AIK_CreateIdentKey(&hAIKey,NULL,"sss","kkk"); 
	if (result != TSS_SUCCESS) {
		printf("Create AIK error %s!\n", tss_err_string(result));
		exit(result);
	}

	result = TESI_AIK_GenerateReq(hCAKey,blobsize,buffer,hAIKey,"cert/aik");
	if (result != TSS_SUCCESS){
		printf("Generate aik failed%s!\n",tss_err_string(result));
		exit(result);
	}
	TESI_Local_WriteKeyBlob(hAIKey,"privkey/AIK");

	ret=convert_uuidname("cert/aik",".req",digest,filename);

	if(ret<0)
		return ret;

	struct policyfile_req * aikfile_req = Talloc0(sizeof(*aikfile_req));
	if(aikfile_req==NULL)
		return -EINVAL;
	Memcpy(aikfile_req->uuid,digest,DIGEST_SIZE);
	Strcat(buffer,".req");
	aikfile_req->filename=dup_str(filename,0);
	aikfile_req->requestor=dup_str(user_info->user_name,0);
		
	void * send_msg;
	send_msg=message_create(DTYPE_FILE_TRANS,SUBTYPE_FILE_REQUEST,NULL);
	if(send_msg==NULL)
		return -EINVAL;
	message_add_record(send_msg,aikfile_req);
	ex_module_sendmsg(sub_proc,send_msg);

	return 0;
}

int proc_aik_activate(void * sub_proc,void * recv_msg)
{
	printf("begin aik activate!\n");
	TSS_RESULT	result;
	TSS_HKEY	hAIKey, hCAKey;
	
	TESI_SIGN_DATA signdata;

	int fd;
	int ret;
	int retval;
	int blobsize=0;
	BYTE digest[DIGEST_SIZE];
	char buffer[DIGEST_SIZE*5];
	struct policyfile_notice * file_notice;
	struct aik_cert_info cert_info;

	ret = message_get_record(recv_msg,&file_notice,0);
	if(ret<0)
		return -EINVAL;
	if(file_notice==NULL)
		return 0;
	Strcpy(buffer,file_notice->filename);
	buffer[Strlen(file_notice->filename)-4]=0;

	result=TESI_Local_ReadKeyBlob(&hAIKey,"privkey/AIK");
	if ( result != TSS_SUCCESS )
	{
		print_error("TESI_Local_ReadKeyBlob Err!\n",result);
		return result;
	}

	result=TESI_Local_LoadKey(hAIKey,NULL,"kkk");
	if ( result != TSS_SUCCESS )
	{
		print_error("TESI_Local_LoadKey Err!\n",result);
		return result;
	}

	result=TESI_AIK_Activate(hAIKey,buffer,&signdata);
	if (result != TSS_SUCCESS) {
		print_error("Tspi_Context_CreateObject", result);
		exit(result);
	}
	// Load the CA Key
	result=TESI_Local_GetPubKeyFromCA(&hCAKey,"cert/CA");
	if (result != TSS_SUCCESS) {
		print_error("Get pubkey error!\n", result);
		exit(result);
	}
	
	// write the AIK and aipubkey

	result=TESI_Local_WriteKeyBlob(hAIKey,"privkey/AIK");
	if (result != TSS_SUCCESS) {
		print_error("store aik data error!\n", result);
		exit(result);
	}
	ret=convert_uuidname("privkey/AIK",".key",digest,buffer);
	if(ret>0)
		printf("Generate AIK private key blob %s!\n",buffer);


	result=TESI_Local_WritePubKey(hAIKey,"pubkey/AIK");
	if (result != TSS_SUCCESS) {
		print_error("store aik data error!\n", result);
		exit(result);
	}
	ret=convert_uuidname("pubkey/AIK",".pem",digest,buffer);
	if(ret>0)
		printf("Generate AIK public key blob %s!\n",buffer);

	// verify the CA signed cert
	result=TESI_AIK_VerifySignData(&signdata,"cert/CA");
	if (result != TSS_SUCCESS) {
		print_error("verify data error!\n", result);
		exit(result);
	}

	// get the content of the CA signed cert

	// read the req info from aik request package
	void * struct_template=memdb_get_template(DTYPE_AIK_STRUCT,SUBTYPE_AIK_CERT_INFO);
	if(struct_template==NULL)
		return -EINVAL;
	blobsize=blob_2_struct(signdata.data,&cert_info,struct_template);
	if(blobsize!=signdata.datalen)
		return -EINVAL;

	WriteSignDataToFile(&signdata,"cert/AIK");
	ret=convert_uuidname("cert/AIK",".sda",digest,buffer);
	if(ret>0)
		printf("Generate AIK sign data %s!\n",buffer);
	return 0;
}