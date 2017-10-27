#ifndef 	TCLOUD_CONNECTOR_H
#define 	TCLOUD_CONNECTOR_H
#include "../include/data_type.h"

enum  dtype_sysconn_typelist
{
	DTYPE_SYS_CONN=0x230,
};
enum subtypelist_sys_conn
{
	SUBTYPE_PEER_INFO=0x01
};
 
enum    connector_type
{
	CONN_INVALID=1,
	CONN_P2P_SERVER,
	CONN_P2P_CLIENT,
	CONN_CLIENT,
	CONN_CHANNEL,
	CONN_SERVER,
};

enum connector_server_state
{

	CONN_SERVER_INIT=1,
	CONN_SERVER_READY,
	CONN_SERVER_LISTEN,
	CONN_SERVER_SERVICE,
	CONN_SERVER_SLEEP,
	CONN_SERVER_SHUTDOWN=0x10000,
};

enum connector_client_state
{

	CONN_CLIENT_INIT=1,
	CONN_CLIENT_CONNECT,
	CONN_CLIENT_RESPONSE,
	CONN_CLIENT_DISCONNECT=0x10000,
};

enum connector_channel_state
{

	CONN_CHANNEL_INIT=1,
	CONN_CHANNEL_ACCEPT,
	CONN_CHANNEL_HANDSHAKE,
	CONN_CHANNEL_CLOSE=0x10000,
};

struct connector_ops 
{
	int conn_type;
	int (*init)(void * connector,char * name,char * addr);
	char * (*getname)(void * connector);
	char * (*getaddr)(void * connector);
	char * (*getpeeraddr)(void * connector); // only for channel connector
	int (*setname)(void * connector,char * name);
	int (*ioctl)(void * connector, void * setdata);
	int (*listen)(void * connector);   // just for server connector
	void * (*accept)(void * connector);  // just for server connector
	int (*close_channel)(void * connector,void * channel);// only for server connector
	int (*connect)(void * connector);    // only for client connector
	int (*read)(void * connector,void * buf,size_t count);
	int (*write)(void * connector,void * buf,size_t count);
	int (*getfd)(void * connector);       
	int (*wait)(void * connector, struct timeval * timeout);
	void * (*getserver)(void * connector);  //only for channel connector
	int (*disconnect)(void * connector);
};

typedef struct tcloud_connector
{
	int conn_type;				//connector's type, such as client,server or channel builded by server	
	int conn_protocol;             		//connector's protocol, such as AF_UNIX,AF_INET,etc;
	int conn_state;				//connector's current state
	int conn_fd;
	char * conn_name;			//name of this connector
	char * conn_addr;			//address for this connector
	char * conn_peeraddr;			//address for this connector
	struct connector_ops * conn_ops;	//this connector's operation functions
	void * conn_base_info;			//relative static connector information, fixed by special protocol
	void * conn_var_info;          		//the information generated by special connector,like those channels generated by server, 
						// or a pointer to the server in channel 
	void * conn_extern_info;          	// An extern describe of this tcloud connector, defined by outside, connector only store and remain it, do not
						// know the details of it 
}TCLOUD_CONN;


struct connector_hub_ops
{
	int (*init)(void * hub);
	int (*ioctl)(void * hub,int d,void * ctldata);
	int (*add_connector)(void * hub,void * connector,void * attr);
	int (*del_connector)(void * hub,void * connector);
	int  (*select)( void * hub,struct timeval * timeout);
	void * (*getactiveread)(void * hub);
	void * (*getactivewrite)(void * hub);
	void * (*getactiveexcept)(void * hub);
	void (*hub_destroy)(void * hub);
};

typedef struct tcloud_connector_hub
{
	int type;
	struct connector_hub_ops * hub_ops;
	void *  connector_list;
	int	nfds;
	fd_set	readfds;
	fd_set  writefds;
	fd_set  exceptfds;
	struct List_head * curr_read;
	struct List_head * curr_write;
	struct List_head * curr_except;
	void * curr_conn;
}TCLOUD_CONN_HUB;

struct connect_proc_info
{
	char uuid[DIGEST_SIZE];
	char * proc_name;
	char * channel_name;
	int islocal;
	int channel_state;
	int proc_state;
} __attribute__((packed));

struct sysconn_peer_info
{
	BYTE uuid[DIGEST_SIZE];
	BYTE machine_uuid[DIGEST_SIZE];
	char proc_name[DIGEST_SIZE];
	char user_name[DIGEST_SIZE];
	BYTE session_uuid[DIGEST_SIZE];
	char * peer_addr;
	char * conn_describe;
} __attribute__((packed));
void * get_connector(int type,int protocol);

extern struct connector_ops connector_af_unix_server_ops; 
extern struct connector_ops connector_af_unix_client_ops; 
extern struct connector_ops connector_af_unix_channel_ops; 

extern struct connector_ops connector_af_inet_server_ops; 
extern struct connector_ops connector_af_inet_client_ops; 
extern struct connector_ops connector_af_inet_channel_ops; 

char * connector_getname(void *);
int  connector_setstate(void *,int );
int  connector_getstate(void *);
char * connector_getaddr(void *);
char * connector_getpeeraddr(void *);

void * connector_get_server(void *);
int connector_get_type(void *);
int connector_get_protocol(void *);

int connector_setname(void *,char *);
int connector_getfd(void *);

void * get_connector_hub( );

struct tcloud_connector * hub_get_connector(void * hub,char * name) ;
struct tcloud_connector * general_hub_get_connector(void * hub,char * uuid,char * name);
void * hub_get_first_connector(void * hub);
void * hub_get_next_connector(void * hub);
void * build_server_syn_message(char * service,char * local_uuid,char *proc_name);
void * build_client_ack_message(void * message,char * local_uuid,char *proc_name,void * conn);
void * hub_get_connector_byreceiver(void * hub,char * uuid,char * name,char * service);
void * hub_get_connector_bypeeruuid(void * hub,char * uuid);

#endif
