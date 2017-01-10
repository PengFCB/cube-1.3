#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dlfcn.h>

#include "data_type.h"
#include "alloc.h"
#include "string.h"
#include "struct_deal.h"
#include "crypto_func.h"
#include "message.h"
#include "connector.h"
#include "ex_module.h"

#include "main_proc_func.h"

typedef struct proc_init_parameter
{
	char * name;
	int type;
	int (* init) (void *,void *);
	int (* start) (void *,void *);
}PROC_INIT;

static char connector_config_file[DIGEST_SIZE*2]="./connector_config.cfg";
static char router_config_file[DIGEST_SIZE*2]="./router_policy.cfg";
static char plugin_config_file[DIGEST_SIZE*2]="./plugin_config.cfg";
static char main_config_file[DIGEST_SIZE*2]="./main_config.cfg";
static char sys_config_file[DIGEST_SIZE*2]="./sys_config.cfg";
static char audit_file[DIGEST_SIZE*2]="./message.log";
static char connector_plugin_file[DIGEST_SIZE*2]="libconnector_process_func.so";
static char router_plugin_file[DIGEST_SIZE*2]="librouter_process_func.so";


int main(int argc,char **argv)
{

    struct tcloud_connector_hub * hub;
    struct tcloud_connector * temp_conn;
    int ret;
    int retval;
    void * message_box;
    int i,j;
    int argv_offset;	
    char namebuffer[DIGEST_SIZE*4];
    char * sys_plugin;		
    char * app_plugin;		


    char json_buffer[4096];
    char print_buffer[4096];
    int readlen;
    int json_offset;
    void * memdb_template ;
    BYTE uuid[DIGEST_SIZE];
    void * main_proc; // point to the main proc's subject struct
    void * conn_proc; // point to the conn proc's subject struct
    void * router_proc; // point to the conn proc's subject struct
    char local_uuid[DIGEST_SIZE*2];

    FILE * fp;
    char audit_text[4096];
    char buffer[4096];
    void * root_node;
    void * temp_node;

    char * baseconfig[] =
    {
//		"baseflag.json",
	"base_define/namelist.json",
	"base_define/dispatchnamelist.json",
	"base_define/typelist.json",
	"base_define/subtypelist.json",
	"base_define/msghead.json",
	"base_define/msgrecord.json",
	"base_define/expandrecord.json",
	"base_define/base_msg.json",
	"base_define/dispatchrecord.json",
	"base_define/exmoduledefine.json",
	 NULL
    };


    sys_plugin=getenv("CUBE_SYS_PLUGIN");
//    if(sys_plugin==NULL)
//	return -EINVAL;		
//    app_plugin=getenv("CUBE_APP_PLUGIN");
    // process the command argument
    if(argc>=2)
    {
	argv_offset=1;
	if(argc%2!=1)
	{
		printf("error format! should be %s [-m main_cfgfile] [-p plugin_cfgfile]"
			"[-c connect_cfgfile] [-r router_cfgfile] [-a audit_file]!\n",argv[0]);
		return -EINVAL;
	}
    }
      
//	alloc_init(alloc_buffer);
	struct_deal_init();
	memdb_init();

	for(i=0;baseconfig[i]!=NULL;i++)
	{
		ret=read_json_file(baseconfig[i]);
		if(ret<0)
			return ret;
		printf("read %d elem from file %s!\n",ret,baseconfig[i]);
	}

	msgfunc_init();
	dispatch_init();

	ex_module_list_init();
    for(argv_offset=1;argv_offset<argc;argv_offset+=2)
    {
	if((argv[argv_offset][0]!='-')
		&&(strlen(argv[argv_offset])!=2))
	{
		printf("error format! should be %s [-m main_cfgfile] [-p plugin_cfgfile]"
			"[-c connect_cfgfile] [-r router_cfgfile] [-a audit_file]!\n",argv[0]);
		return -EINVAL;
	}
	switch(argv[argv_offset][1])
	{
		case 'm':
			if(strlen(argv[argv_offset+1])>=DIGEST_SIZE*2)
				return -EINVAL;
			strncpy(main_config_file,argv[argv_offset+1],DIGEST_SIZE*2);
			break;			
		case 'p':
			if(strlen(argv[argv_offset+1])>=DIGEST_SIZE*2)
				return -EINVAL;
			strncpy(plugin_config_file,argv[argv_offset+1],DIGEST_SIZE*2);
			break;			
		case 'c':
			if(strlen(argv[argv_offset+1])>=DIGEST_SIZE*2)
				return -EINVAL;
			strncpy(connector_config_file,argv[argv_offset+1],DIGEST_SIZE*2);
			break;			
		case 'r':
			if(strlen(argv[argv_offset+1])>=DIGEST_SIZE*2)
				return -EINVAL;
			strncpy(router_config_file,argv[argv_offset+1],DIGEST_SIZE*2);
			break;			
		case 'a':
			if(strlen(argv[argv_offset+1])>=DIGEST_SIZE*2)
				return -EINVAL;
			strncpy(audit_file,argv[argv_offset+1],DIGEST_SIZE*2);
			break;			
		default:
			printf("error format! should be %s [-m main_cfgfile] [-p plugin_cfgfile]"
				"[-c connect_cfgfile] [-r router_cfgfile] [-a audit_file]!\n",argv[0]);
			return -EINVAL;
	
	}
    }	

    int fd =open(audit_file,O_CREAT|O_RDWR|O_TRUNC,0666);
    close(fd);

    // init system
    system("mkdir lib");
    // init the main proc struct

    fd=open(sys_config_file,O_RDONLY);
    if(fd<0)
	return -EINVAL;
/*
    json_offset=read(fd,buffer,4096);
    if(json_offset<0)
	return ret;
    if(json_offset>4096)
    {
	printf("main config file is too long!\n");
	return -EINVAL;
    }
    close(fd);
    ret=json_solve_str(&root_node,buffer);
    if(ret<0)
	return ret;	
*/

    ret=read_json_node(fd,&root_node);
    if(ret<0)
	return ret;	
    close(fd);
	
    struct lib_para_struct * lib_para;

    ret=read_sys_cfg(&lib_para,root_node);
    if(ret<0)
	return ret; 		
    fd=open(main_config_file,O_RDONLY);
    if(fd<0)
	return -EINVAL;

    ret=read_json_node(fd,&root_node);
    if(ret<0)
	return ret;	
    close(fd);
	
    ret=read_main_cfg(lib_para,root_node);
    if(ret<0)
	return ret; 		

    ret=get_local_uuid(local_uuid);
    printf("this machine's local uuid is %s\n",local_uuid);
    proc_share_data_setvalue("uuid",local_uuid);

    // init all the proc database

    usleep(time_val.tv_usec);

    PROC_INIT plugin_proc; 

    // init the connect proc	
//    strcpy(namebuffer,sys_plugin);
    strcpy(namebuffer,"../../proc/plugin/");
    strcat(namebuffer,connector_plugin_file);
    plugin_proc.init =main_read_func(namebuffer,"proc_conn_init");
    if(plugin_proc.init==NULL)
	return -EINVAL;
    plugin_proc.start =main_read_func(namebuffer,"proc_conn_start");
    if(plugin_proc.start==NULL)
	return -EINVAL;
     plugin_proc.name=dup_str("connector_proc",0);	
     plugin_proc.type=MOD_TYPE_CONN;
	
     ret=ex_module_create("connector_proc",MOD_TYPE_CONN,NULL,&conn_proc);
    if(ret<0)
	    return ret;

    ex_module_setinitfunc(conn_proc,plugin_proc.init);
    ex_module_setstartfunc(conn_proc,plugin_proc.start);

    ex_module_init(conn_proc,connector_config_file);
    add_ex_module(conn_proc);
    ex_module_start(conn_proc,NULL);

    // init the router proc	
//    strcpy(namebuffer,sys_plugin);
    strcpy(namebuffer,"../../proc/plugin/");
    strcat(namebuffer,router_plugin_file);
    plugin_proc.init =main_read_func(namebuffer,"proc_router_init");
    if(plugin_proc.init==NULL)
	return -EINVAL;
    plugin_proc.start =main_read_func(namebuffer,"proc_router_start");
    if(plugin_proc.start==NULL)
	return -EINVAL;
     plugin_proc.name=dup_str("router_proc",0);	
     plugin_proc.type=MOD_TYPE_ROUTER;
	
    ret=ex_module_create("router_proc",MOD_TYPE_MONITOR,NULL,&router_proc);
    if(ret<0)
	    return ret;

    ex_module_setinitfunc(router_proc,plugin_proc.init);
    ex_module_setstartfunc(router_proc,plugin_proc.start);

    ex_module_init(router_proc,router_config_file);
	
    printf("prepare the router proc\n");
    ret=add_ex_module(router_proc);
    if(ret<0)
	    return ret;

    // loop to init all the plugin's 

    fd=open(plugin_config_file,O_RDONLY);
    if(fd<0)
	return -EINVAL;

    while(read_json_node(fd,&root_node)>0)
    {  		
    }
    
/*
       	ret=sec_subject_create(plugin_initpara.name,plugin_initpara.type,NULL,&sub_proc);
   	if(ret<0)
		return ret;

	if(app_plugin != NULL)
        {
      	        strcpy(namebuffer,app_plugin);
        	strcat(namebuffer,plugin_initpara.plugin_dlib);
		ret=access(namebuffer,R_OK|X_OK);
		if(ret<0)
		{
      	        	strcpy(namebuffer,sys_plugin);
       	 		strcat(namebuffer,plugin_initpara.plugin_dlib);
		}
	}
        else
	{      
      	        strcpy(namebuffer,sys_plugin);
        	strcat(namebuffer,plugin_initpara.plugin_dlib);
        }
    	plugin_initpara.init =main_read_func(namebuffer,plugin_initpara.init);
    	if(plugin_initpara.init==NULL)
		return -EINVAL;
    	plugin_initpara.start =main_read_func(namebuffer,plugin_initpara.start);
    	if(plugin_initpara.start==NULL)
		return -EINVAL;

    	sec_subject_setinitfunc(sub_proc,plugin_initpara.init);
   	sec_subject_setstartfunc(sub_proc,plugin_initpara.start);
	void * init_para;
	init_para=find_json_elem("init_para",root_node);
  	ret= sec_subject_init(sub_proc,init_para);
	if(ret<0)
  		return ret;
        add_sec_subject(sub_proc);
    }
  */   
    usleep(time_val.tv_usec);
    printf("prepare the conn proc\n");
    ret=ex_module_start(conn_proc,NULL);
    if(ret<0)
	    return ret;

    // second loop:  start all the monitor process
/*       	
    ret=get_first_sec_subject(&sub_proc);

    if(ret<0)
	return ret;
    while(sub_proc!=NULL)
    {
	  if(sec_subject_gettype(sub_proc) == MOD_TYPE_MONITOR)
	  {
  		ret=sec_subject_start(sub_proc,NULL);
	  	if(ret<0)
  			return ret;
		printf("monitor sub_proc %s started successfully!\n",sec_subject_getname(sub_proc));
	  }
    	  ret=get_next_sec_subject(&sub_proc);

    	  if(ret<0)
		return ret;
    }


    int thread_retval;
    ret=sec_subject_join(conn_proc,&thread_retval);
    printf("thread return value %d!\n",thread_retval);
*/
    return ret;
}
