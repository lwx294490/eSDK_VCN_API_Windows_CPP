/*Copyright 2015 Huawei Technologies Co., Ltd. All rights reserved.
eSDK is licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
		http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "SDKInterface.h"
#include "bp_log.h"
#include "bp_environments.h"
#include "IService.h"
#include "Thread.h"
#include "HandleSignal.h"
#include "SDKMainCtrl.h"
#include "bp_net_factory.h"
#include "nss_mt.h"
#include "CapabilityMgr.h"
#include "rtsp_client_api.h"
#include "IVS_NetSource.h"
#include "IVS_Player.h"
#include "EventCallBackJob.h"
#include "SDKDef.h"
#include "SDKConfig.h"
#include "IVS_Trace.h"
#include "StreamEventMgr.h"

#define MAX_PORT 65535
#define MIN_PORT 1
#define MAX_IP_VALUE 255
#define IP_SPLIT_LEN 4
CSDKInterface::CSDKInterface(void)
	: m_bInit(false)
	, m_bBplogInit(true)
{
	m_pMutexArrayLock = VOS_CreateMutex();
	m_pMutexMapLock   = VOS_CreateMutex();
	m_pMutexLoginLock = VOS_CreateMutex();
	m_pMutexLoginByTktLock = VOS_CreateMutex();
    m_pRetriveStatusMapLock = VOS_CreateMutex();
	eSDK_MEMSET(m_SessionIDFlagArray, SDK_SESSION_NOTUSE, sizeof(m_SessionIDFlagArray));
}

CSDKInterface::~CSDKInterface(void)
{
	try  
    {
		if (m_bInit)
		{
			(void)Cleanup();
		}

        if (NULL != m_pMutexArrayLock)
        {
            VOS_DestroyMutex(m_pMutexArrayLock);
            m_pMutexArrayLock = NULL;
        }
    	
        if (NULL != m_pMutexMapLock)
        {
            VOS_DestroyMutex(m_pMutexMapLock);
            m_pMutexMapLock = NULL;
        }

		if (NULL != m_pMutexLoginLock)
		{
			VOS_DestroyMutex(m_pMutexLoginLock);
			m_pMutexLoginLock = NULL;
		}

		if (NULL != m_pMutexLoginByTktLock)
		{
			VOS_DestroyMutex(m_pMutexLoginByTktLock);
			m_pMutexLoginByTktLock = NULL;
		}

        if (NULL != m_pRetriveStatusMapLock)
        {
            VOS_DestroyMutex(m_pRetriveStatusMapLock);
            m_pRetriveStatusMapLock = NULL;
        }
	}
	catch (...)
	{
		// ���ܴ�ӡ��־,BP��ж��;
	}
}

// ��ʼ��BP
IVS_INT32 CSDKInterface::BPInit()
{
	InitExceptionFilter();     
	ACE::init();
	CSDKMainctrl *pMainService = IVS_NEW(pMainService);   
	if (NULL == pMainService)                            
	{ 
        m_bBplogInit = false;
		return IVS_FAIL;// BPδ��ʼ����������־
	}                                               
	pMainService->SetParam(NULL, NULL);
    // ��ʼ����־Ŀ¼��m_strLogRootPathΪ�ձ�ʾδ����·�����������ļ�Ϊ׼
    const char* pLogPath = NULL;
    std::string strLogPath = CSDKConfig::instance().GetLogPath();
    if (!strLogPath.empty())
    {
        strLogPath.append(LOG_MODULE_NAME_SDK);
        pLogPath = strLogPath.c_str();
    }
	int iRet = pMainService->InitLog(pLogPath); 
	if (IVS_SUCCEED != iRet) 
	{
        m_bBplogInit = false;
		return iRet;// BPδ��ʼ����������־ 
	}
	BP_RUN_LOG_INF("init log success","NA");
	CHandleSignal::Init();                        
	CThread* pMainThread = IVS_NEW(pMainThread);              
	if (NULL == pMainThread)                      
	{                                               
		IVS_DELETE(pMainService) ;
		return IVS_FAIL;// BPδ��ʼ����������־                             
	}                                               
	CThread::SetMainThread(pMainThread);            
	pMainThread->open("main", true);               
	CThreadMgr::instance().AddThread(pMainThread);  
	pMainService->Init(); 
	CMainCtrl::SetMainCtrl(pMainService);           
	pMainThread->AddService(pMainService);  

	// ����ע���������澯��������Ҫͨ�����ע��
	// �澯��Ϣ֪ͨ
	CCapabilityMgr::instance().RegCapability(NSS_ALARM_NOTIFY_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());       //lint !e1013 !e1055  !e746 
	// �澯��Ϣ״̬֪ͨ
	CCapabilityMgr::instance().RegCapability(NSS_ALARM_STATUS_NOTIFY_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID()); //lint !e1013
	// ��������ִ�С�ֹ֪ͣͨ
	CCapabilityMgr::instance().RegCapability(NSS_DO_LINKAGE_ACTION_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
    CCapabilityMgr::instance().RegCapability(NSS_STOP_LINKAGE_ACTION_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
    // ֪ͨ�ͻ����û�����
    CCapabilityMgr::instance().RegCapability(NSS_USER_OFFLINE_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	// ���ܷ���
	CCapabilityMgr::instance().RegCapability(NSS_BA_LEARNING_SCHEDULE_REPORT_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	CCapabilityMgr::instance().RegCapability(NSS_FR_REPORT_SCHEDULE_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	CCapabilityMgr::instance().RegCapability(NSS_PUSH_ALARM_LOCUS_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	CCapabilityMgr::instance().RegCapability(NSS_ADDPLAN_SCHEDULE_REPORT_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
    // OMU�澯
	CCapabilityMgr::instance().RegCapability(NSS_OMU_ALARM_CU_REPORT_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	// �ֶ�¼��״̬�ϱ�
	CCapabilityMgr::instance().RegCapability(NSS_NOTIF_MANUAL_RECORD_STATE_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	// �����豸�ϱ�
	CCapabilityMgr::instance().RegCapability(NSS_DISCOVERED_DEVICE_NOTIFY_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
    // ģʽ�켣¼�ƽ���֪ͨ
	CCapabilityMgr::instance().RegCapability(NSS_MODE_CRUISE_RECORD_OVER_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID());  //lint !e1013
	
    // ��ѯ���豸�б�֪ͨ
    CCapabilityMgr::instance().RegCapability(NSS_GET_USER_CHANNEL_LIST_RSP, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID()); //lint !e1013

	// �ͻ��˹ر���Ƶҵ��֪ͨ
	CCapabilityMgr::instance().RegCapability(NSS_SHUTDOWN_STREAM_NOTIFIY_REQ, pMainService->GetServiceID(), CThread::GetMainThread()->GetThreadID()); //lint !e1013


	return IVS_SUCCEED;
}

// ��ʼ��
IVS_INT32 CSDKInterface::Init()
{
    int iRet = IVS_FAIL;
    do 
    {
		std::string strDir;
		TCHAR pBuf[MAX_PATH] = {'\0'};
		GetModuleFileName(::GetModuleHandle("IVS_SDK.dll"), pBuf, MAX_PATH-sizeof(TCHAR));
		char* cTemp = (char*)pBuf;
		std::string strTemp = cTemp;
		strDir = strTemp.substr(0,strTemp.rfind('\\')+1);
		set_home_directory(strDir);//lint !e1036
        if (IVS_SUCCEED != BPInit())
        {
            //BP_RUN_LOG_ERR(IVS_FAIL,"SDK Interface Init Failed.", "Reason:BPInit Failed.");
            iRet = IVS_FAIL;
            break;
        }
		
		BP_RUN_LOG_INF("BP Init Success.", "eSDK IVS compile time is %s : %s", __DATE__, __TIME__);

        // ����SDK�����ļ�ʧ�ܣ�ʹ��Ĭ��ֵ����ֱ�ӷ���ʧ�ܣ�Ĭ��ֵ��CSDKConfig���캯���г�ʼ��
        int iLoadConfig = CSDKConfig::instance().LoadConfigFile();
        if (IVS_SUCCEED != iLoadConfig)
        {
            BP_RUN_LOG_ERR(IVS_FAIL,"LoadConfigFile Failed", "Client will use default config, iLoadConfig = %d",iLoadConfig);
        }
        const char* pLogPath = NULL;
        std::string strLogRootPath = CSDKConfig::instance().GetLogPath();
        if (!strLogRootPath.empty())
        {
            strLogRootPath.append(LOG_MODULE_NAME_RTSP);
            pLogPath = strLogRootPath.c_str();
        }
        // ��ʼ��rtsp_client��;
        unsigned int uiLogLevel = CSDKConfig::instance().GetRtspLogLevel();
        iRet = RTSP_CLIENT_Init(uiLogLevel, pLogPath);
        if (IVS_SUCCEED != iRet)
        {
            BP_RUN_LOG_ERR(IVS_SDK_RET_INIT_RTSPCLIENT_ERROR,"Init rtsp client ", "failed.");
            iRet = IVS_SDK_RET_INIT_RTSPCLIENT_ERROR;
            (void)BPCleanup();
            return iRet;
        }
        strLogRootPath = CSDKConfig::instance().GetLogPath();
        pLogPath = NULL;

		std::string strExeDir;
		TCHAR pExeDirBuf[MAX_PATH] = {'\0'};
		GetModuleFileName(::GetModuleHandle(NULL), pExeDirBuf, MAX_PATH-sizeof(TCHAR));
		std::string strExeDirTemp = (char*)pExeDirBuf;
		strExeDir = strExeDirTemp.substr(0, strExeDirTemp.rfind('\\')+1);

        if (!strLogRootPath.empty())
        {
			if (std::string::npos ==strLogRootPath.find(":"))
			{
				std::string strFullDir = strExeDir + "\\" + strLogRootPath;
				strLogRootPath = strFullDir;
			}
            strLogRootPath.append(LOG_SPLIT_CHAR);
            pLogPath = strLogRootPath.c_str();
        }
		else
		{
			strLogRootPath = strExeDir;
			std::string strNetSourceLogPath = CSDKConfig::instance().GetNetSourceLogPath();
			strLogRootPath.append("\\" + strNetSourceLogPath);
			pLogPath = strLogRootPath.c_str();
		}
        // ��ʼ��NetSource��;
        iRet = IVS_NETSOURCE_Init(pLogPath);
        if (IVS_SUCCEED != iRet)
        {
            BP_RUN_LOG_ERR(IVS_PLAYER_RET_INIT_NETSOURCE_ERROR,"Init net source ", "failed.");
            iRet = IVS_PLAYER_RET_INIT_NETSOURCE_ERROR;
            (void)BPCleanup();
            RTSP_CLIENT_Cleanup();
            return iRet;
        }
        strLogRootPath = CSDKConfig::instance().GetLogPath();
        pLogPath = NULL;
        if (!strLogRootPath.empty())
        {
            strLogRootPath.append(LOG_SPLIT_CHAR);
            pLogPath = strLogRootPath.c_str();
        }
        // ��ʼ��IVS_Player���ſ�;
        iRet = IVS_PLAY_Init(pLogPath);
        if (IVS_SUCCEED != iRet)
        {
            BP_RUN_LOG_ERR(IVS_PLAYER_RET_INIT_PLAY_ERROR,"Init play ", "failed.");
            iRet = IVS_PLAYER_RET_INIT_PLAY_ERROR;
            (void)BPCleanup();
            RTSP_CLIENT_Cleanup();
            IVS_NETSOURCE_UnInit();
            return iRet;
        }

        iRet = m_eventMgr.Init();
        if (IVS_SUCCEED != iRet)
        {
            BP_RUN_LOG_ERR(iRet,"Init event mgr ", "failed.");
            (void)BPCleanup();
            RTSP_CLIENT_Cleanup();
            IVS_NETSOURCE_UnInit();
            IVS_PLAY_UnInit();
            return iRet;
        }

        iRet = CStreamEventMgr::instance().Init();
        if (IVS_SUCCEED != iRet)
        {
            BP_RUN_LOG_ERR(iRet,"Init stream event mgr ", "failed.");
            m_eventMgr.UnInit();
            (void)BPCleanup();
            RTSP_CLIENT_Cleanup();
            IVS_NETSOURCE_UnInit();
            IVS_PLAY_UnInit();
            return iRet;
        }

    } while (false);

	
	return iRet;
}

IVS_INT32 CSDKInterface::BPCleanup()
{
	CMainCtrl* pMainCtrl = CMainCtrl::GetMainCtrl();
	if (NULL != pMainCtrl)                          
	{                                               
		pMainCtrl->UnInit();                        
		//delete pMainCtrl;                           
	}    
	//// �ص������̣߳��������̣߳����߳�Ҳ��������棩
	//CThreadMgr::instance().close(); 

 //   //��ʼ����־���ɹ���m_bBplogInit����Ϊfalse������������־;
 //   if (m_bBplogInit)
 //   {
 //       IVS_LogClean();
 //   }
	ACE::fini();    

	return IVS_SUCCEED;
}//lint !e1762

// ����
IVS_INT32 CSDKInterface::Cleanup()
{
	IVS_DEBUG_TRACE("");
    m_bInit = false;

    CStreamEventMgr::instance().UnInit();

    // ֹͣ�¼�����;
    m_eventMgr.UnInit();

	// ɾ�������û�;
	//(void)DeleteLocalUserObj();
	int iKey = IVS_SUCCEED;

	int iSessionIDMapSize = m_mapSessionIDToSingleSvrProxy.size();//lint !e713
	MAP_SESSEIONID_USERMGR_ITER svrProxyIter = m_mapSessionIDToSingleSvrProxy.begin();
	while (iSessionIDMapSize>0)
	{
		svrProxyIter = m_mapSessionIDToSingleSvrProxy.begin();

		//ÿ��logout����ɾ��map�еĵ�ǰԪ��,���ÿ�ζ�ָ��ʼԪ��;
		iKey = svrProxyIter->first;
		(void)Logout(iKey);

		//ɾ��֮�����sizeֵ;
		iSessionIDMapSize = m_mapSessionIDToSingleSvrProxy.size();//lint !e713
	}

	m_mapSessionIDToSingleSvrProxy.clear();

	//ֹͣ�ص��߳�;
	//if (NULL != m_pEventCallBackThread)
	//{
	//    (void)m_pEventCallBackThread->Stop();
	//    delete m_pEventCallBackThread;
	//    m_pEventCallBackThread = NULL;
	//}

	//�ͷ�Э�鲥�ſ�
	IVS_INT32 iRet = IVS_FAIL;// = IVS_MLIB_Cleanup();
	
    // cleanup bp by z00193167   modify by x00192614��BPCleanup�����û�ע��֮�󣬷���ע�������Ͳ���ȥ
    (void)BPCleanup();
	//if (IVS_SUCCEED != iRet)
    //{
        // BP���Ѿ��ͷţ����ܴ�ӡ��־
        //BP_RUN_LOG_ERR(iRet,"BPCleanup ", "failed");
        //return iRet;
    //}

    iRet = RTSP_CLIENT_Cleanup();
    IVS_NETSOURCE_UnInit();
    IVS_PLAY_UnInit();
	return iRet;
}

IVS_INT32 CSDKInterface::SetEventCallBack( IVS_INT32 iSessionID, EventCallBack fEventCallBack /*= NULL*/, void *pUserData /*= NULL*/ )
{
	if((SDK_CLIENT_STARTNO > iSessionID) || (SDK_CLIENT_TOTAL <= iSessionID))
	{
		BP_RUN_LOG_ERR(IVS_SDK_RET_INVALID_SESSION_ID, "Set Event CallBack", "Param error, SessionID out of range.");
		return IVS_SDK_RET_INVALID_SESSION_ID;
	}

	CUserMgr *pUserMgr = GetUserMgr(iSessionID);
	if (NULL == pUserMgr)
	{
		BP_RUN_LOG_ERR(IVS_SDK_RET_INVALID_SESSION_ID, "Set Event CallBack", "Get UserMgr by id failed.");
		return IVS_PARA_INVALID;
	}
	pUserMgr->SetEventCallBack(fEventCallBack, pUserData);

	return m_eventMgr.SetEventCallBack(iSessionID, fEventCallBack, pUserData);
}

// ��ȡ�ỰID
int CSDKInterface::GetSessoinID()
{
    (void)VOS_MutexLock(m_pMutexArrayLock);
	for (int iSessionID = SDK_CLIENT_STARTNO; iSessionID < SDK_CLIENT_TOTAL; iSessionID++)
	{
		if (SDK_SESSION_NOTUSE == m_SessionIDFlagArray[iSessionID])
		{
			m_SessionIDFlagArray[iSessionID] = SDK_SESSION_INUSE;
			(void)VOS_MutexUnlock(m_pMutexArrayLock);
			return iSessionID;
		}
	}
    (void)VOS_MutexUnlock(m_pMutexArrayLock);
	return SDK_SESSIONID_INIT;
}

// �ͷ�SessionID
IVS_INT32 CSDKInterface::FreeSessoinID(int iSessionID)
{
	if((SDK_CLIENT_STARTNO <= iSessionID) && (iSessionID < SDK_CLIENT_TOTAL))
	{
		(void)VOS_MutexLock(m_pMutexArrayLock);

		m_SessionIDFlagArray[iSessionID] = SDK_SESSION_NOTUSE;
		m_mapSessionIDToSingleSvrProxy.erase(iSessionID);
		(void)VOS_MutexUnlock(m_pMutexArrayLock);

		return IVS_SUCCEED;
	}
	return IVS_FAIL;
}

// ����SessionID��map��
void CSDKInterface::AddSessoinID_SingleSvrProxyToMap(int iSessionID, CUserMgr *pSingleSvrProxy)
{
	if ((SDK_CLIENT_STARTNO <= iSessionID) && (iSessionID < SDK_CLIENT_TOTAL))
	{
		(void)VOS_MutexLock(m_pMutexMapLock);
		m_mapSessionIDToSingleSvrProxy[iSessionID] = pSingleSvrProxy;
		(void)VOS_MutexUnlock(m_pMutexMapLock);
	}
}

// ����SessionID��ȡ�û�����
CUserMgr *CSDKInterface::GetUserMgr(int iSessionID)
{
	if (SDK_CLIENT_STARTNO > iSessionID || SDK_CLIENT_TOTAL <= iSessionID)
	{
		return NULL;
	}
	
	CUserMgr *pUserMgr = NULL;
	(void)VOS_MutexLock(m_pMutexMapLock);
    MAP_SESSEIONID_USERMGR_ITER iter = m_mapSessionIDToSingleSvrProxy.find(iSessionID);
	if (iter != m_mapSessionIDToSingleSvrProxy.end())
	{
		pUserMgr = iter->second;
	}
	(void)VOS_MutexUnlock(m_pMutexMapLock);
	return pUserMgr;
}

//add by w00206574 start
//����ַ����Ƿ�Ϊ���ִ�
IVS_BOOL CheckIsDigit(IVS_CHAR* pDigitChar)
{
	// ��������;
	CHECK_POINTER(pDigitChar, IVS_IP_INVALID);
	IVS_INT32 iLen = strlen(pDigitChar);//�ַ�������
    if (1 != iLen && '0' == *pDigitChar )//���ȷ�1���ַ�������ĸ����Ϊ0
	{
		return false;
	}

	for (IVS_INT32 index =0;index<iLen;index++)
	{
		if (false == isdigit(*(pDigitChar+index)))
		{
			return false;
		}
	}
	return true;
}
//У��IP��PORT�Ƿ�Ϸ�
IVS_BOOL CheckIpAndPort(const IVS_LOGIN_INFO* pLoginReqInfo)
{
	// ��������;
	CHECK_POINTER(pLoginReqInfo, IVS_IP_INVALID);
	try
	{
		IVS_CHAR pPort[IVS_DOMAIN_LEN] = {0};
		eSDK_SPRINTF(pPort, sizeof(pPort), "%d", pLoginReqInfo->uiPort);
		struct addrinfo *pResult =NULL;

		if (MIN_PORT > pLoginReqInfo->uiPort || pLoginReqInfo->uiPort> MAX_PORT)//�˿ڲ��Ϸ�
		{
			BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ","pLoginReqInfo->uiPort is Error");
			return IVS_PORT_INVALID;
		}
		else if (0 == pLoginReqInfo->stIP.uiIPType)//ipv4
		{
			IVS_INT32 iCount = 0;    
			const IVS_CHAR *pSplitChar = ".";    
			IVS_CHAR cTempIP[IVS_IP_LEN +1]={0};    
			IVS_INT32 iIPAddr[IP_SPLIT_LEN] = {0};   
			eSDK_STRCPY(cTempIP, IVS_IP_LEN +1, pLoginReqInfo->stIP.cIP);
			for(IVS_UINT32  index = 0; index < strlen(cTempIP); index++)    
			{        
				if(cTempIP[index] == '.')        
				{            
					iCount++;                     
				}        
			}     
			if(iCount != IP_SPLIT_LEN - 1)    
			{       
				return IVS_IP_INVALID;   
			} 

			IVS_CHAR* cTemp = strtok(cTempIP, pSplitChar);
			for (IVS_INT32 index = 0;index < IP_SPLIT_LEN;index++)
			{
				if (NULL == cTemp|| *cTemp==' ' || !CheckIsDigit(cTemp))
				{
					return IVS_IP_INVALID;
				}
				iIPAddr[index] = atoi(cTemp);
				cTemp = strtok(NULL, pSplitChar);
			}
			
			if ((iIPAddr[0] >= 0&&iIPAddr[0] <= MAX_IP_VALUE)&&(iIPAddr[1] >= 0&&iIPAddr[1] <= MAX_IP_VALUE)&&(iIPAddr[2] >= 0&&iIPAddr[2] <= MAX_IP_VALUE)&&(iIPAddr[3] >= 0&&iIPAddr[3] <= MAX_IP_VALUE))   
			{           
				return IVS_SUCCEED;            
			}   
			else    
			{       
				return IVS_IP_INVALID;    
			}
		}
		else if (0 != getaddrinfo(pLoginReqInfo->stIP.cIP,pPort,NULL,&pResult))//ip���Ϸ�
		{
			BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ","stIP.cIP is Error");
			return IVS_IP_INVALID;		
		}
	}
	catch (...)
	{
		return IVS_IP_INVALID;
	}
	return IVS_SUCCEED;
}

//add by w00206574 end

// �û���¼
IVS_INT32 CSDKInterface::Login(const IVS_LOGIN_INFO* pLoginReqInfo, IVS_INT32* pSessionId)//lint !e830
{
    // ��������;
    CHECK_POINTER(pLoginReqInfo, IVS_OPERATE_MEMORY_ERROR);
    CHECK_POINTER(pSessionId, IVS_OPERATE_MEMORY_ERROR);

    //IP����У�� 
    if ((IP_V6 != pLoginReqInfo->stIP.uiIPType) && (IP_V4 != pLoginReqInfo->stIP.uiIPType))
    {
        BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "stIP.iIPType is Error");
        return IVS_PARA_INVALID;
    }

	//�û�����IP��������У��
    if ('\0' == pLoginReqInfo->cUserName[0] || '\0' == pLoginReqInfo->stIP.cIP[0]) //lint !e774    
    {
        BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "Param is Error");
        return IVS_PARA_INVALID;
    }
    //�ͻ�������У��
    IVS_INT32 iCheckClientType = CheckClientType(pLoginReqInfo->uiClientType);
	if (IVS_SUCCEED != iCheckClientType)
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "Client Type: %d is error", pLoginReqInfo->uiClientType);
		return IVS_PARA_INVALID;
	}
	//add by wangguangyan start
	IVS_INT32 iCheckIpPort = CheckIpAndPort(pLoginReqInfo);
	if (IVS_SUCCEED != iCheckIpPort)
	{
		return iCheckIpPort;
	}
	//add by wangguangyan end

	//add by zwx211831, Date:20140703, �Ե�¼�ӿڽ��м�����ȷ��ͬ����¼����ֹ��smu�ĳ��
	CLockGuard oReLinklock(m_pMutexLoginLock);

    // ɾ����ͬ�û�
    (void)RemoveUser(pLoginReqInfo->cUserName, pLoginReqInfo->stIP.cIP, "");

    int iSessionID = GetSessoinID();
    if(SDK_SESSIONID_INIT == iSessionID)  
    {
        // �Ѵ�����û���;
        BP_RUN_LOG_WAR("Login ", "Over max user count, User's iSessionID = %d", iSessionID);
        return IVS_SDK_ERR_ERR_MAX_USERNUM;
    }

    //�����ڲ��ṹ�壬
    IVS_LOGIN_INFO stLoginReqInfo;
	if(!CToolsHelp::Memcpy(&stLoginReqInfo,sizeof(IVS_LOGIN_INFO),pLoginReqInfo,sizeof(IVS_LOGIN_INFO)))
	{
		BP_RUN_LOG_ERR(IVS_ALLOC_MEMORY_ERROR,"Login", "stLoginReqInfo Memcpy error.");
		return IVS_ALLOC_MEMORY_ERROR;
	}
    stLoginReqInfo.stIP.cIP[IVS_IP_LEN - 1] = '\0'; //IPʵ�ʳ��Ȳ���ﵽ64��ǿ��char����ĩβ��'\0'
    stLoginReqInfo.uiLoginType = LOGIN_BASIC;
    CToolsHelp::GetHostName(stLoginReqInfo.cMachineName, sizeof(stLoginReqInfo.cMachineName));

    CUserMgr& userMgr = m_UserMgrArray[iSessionID];//lint !e838
    userMgr.SetSingleSvrProxy(this);
    userMgr.SetSessionID(iSessionID);
    AddSessoinID_SingleSvrProxyToMap(iSessionID, &userMgr);

    int iRet = userMgr.Login(&stLoginReqInfo);

	// �����ڴ棬��֤�ڴ��в���������Ĵ���
	SecureZeroMemory(&stLoginReqInfo, sizeof(IVS_LOGIN_INFO));

    // �״ε�¼��������ھ�������¼�ɹ����ɵ��÷����ݷ����������û��޸�����
    if(IVS_SUCCEED != iRet && IVS_SMU_USER_FIRST_LOGIN != iRet && IVS_SMU_USER_PWD_NEED_MOD != iRet)
    {
        (void)FreeSessoinID(iSessionID);
        userMgr.SetSessionID(SDK_SESSIONID_INIT);
        BP_RUN_LOG_ERR(iRet, "Login", "Login is failed");
        return iRet;
    }

    *pSessionId = iSessionID;  //����sessionId

    // Ϊÿһ���û��������ûص�������ָ����ͬ���û�����ΪsessionID;
    (void)RTSP_CLIENT_SetExceptionCallBack(m_eventMgr.GetRtspExceptionCallBack(), (void*)iSessionID);
    IVS_NETSOURCE_SetExceptionCallBack(m_eventMgr.GetNetExceptionCallBack(), (void*)iSessionID);
    IVS_PLAY_SetExceptionCallBack(m_eventMgr.GetPlayerExceptionCallBack(), (void*)iSessionID);   
    IVS_PLAY_SetEventCallBack(CStreamEventMgr::instance().GetPlayerEventCallBack(), (void*)iSessionID);

	BP_RUN_LOG_INF("Login Success", "return :%d", iRet);
    return iRet;
}


//�û������¼;
IVS_INT32 CSDKInterface::LoginByTicket(const IVS_LOGIN_INFO_EX* pLoginReqInfoEx, IVS_INT32* pSessionId)
{
	// ��������;
	CHECK_POINTER(pLoginReqInfoEx, IVS_OPERATE_MEMORY_ERROR);
	CHECK_POINTER(pSessionId, IVS_OPERATE_MEMORY_ERROR);

	//IP����У��;
	if ((IP_V6 != pLoginReqInfoEx->stIP.uiIPType) && (IP_V4 != pLoginReqInfoEx->stIP.uiIPType))
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login By Ticket ", "stIP.iIPType is Error");
		return IVS_PARA_INVALID;
	}

	//TicketID��IPУ��
	if ('\0' == pLoginReqInfoEx->stIP.cIP[0] || '\0' == pLoginReqInfoEx->cTicketID[0]) //lint !e774    
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login By Ticket, Param is Error. IP: %s, TicketID: %s", 
			pLoginReqInfoEx->stIP.cIP, pLoginReqInfoEx->cTicketID);
		return IVS_PARA_INVALID;
	}

	//�ͻ�������У��
	IVS_INT32 iCheckClientType = CheckClientType(pLoginReqInfoEx->uiClientType);
	if (IVS_SUCCEED != iCheckClientType)
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "Client Type: %d is error", pLoginReqInfoEx->uiClientType);
		return IVS_PARA_INVALID;
	}

	//add by zwx211831, Date:20140703, �Ե�¼�ӿڽ��м�����ȷ��ͬ����¼����ֹ��smu�ĳ��
	CLockGuard oReLinklock(m_pMutexLoginByTktLock);

	// ɾ����ͬ�û�;
	(void)RemoveUserByTicketID(pLoginReqInfoEx->cTicketID, pLoginReqInfoEx->stIP.cIP, "");

	int iSessionID = GetSessoinID();
	if(SDK_SESSIONID_INIT == iSessionID)  
	{
		// �Ѵ�����û���;
		BP_RUN_LOG_WAR("Login ", "Over max user count, User's iSessionID = %d", iSessionID);
		return IVS_SDK_ERR_ERR_MAX_USERNUM;
	}

	//�����ڲ��ṹ��;
	IVS_LOGIN_INFO_EX stLoginReqInfoEx;
	if(!CToolsHelp::Memcpy(&stLoginReqInfoEx,sizeof(IVS_LOGIN_INFO_EX),pLoginReqInfoEx,sizeof(IVS_LOGIN_INFO_EX)))
	{
		BP_RUN_LOG_ERR(IVS_ALLOC_MEMORY_ERROR,"Login By Ticket", "stLoginReqInfo Memcpy error.");
		return IVS_ALLOC_MEMORY_ERROR;
	}
	stLoginReqInfoEx.stIP.cIP[IVS_IP_LEN - 1] = '\0'; //IPʵ�ʳ��Ȳ���ﵽ64��ǿ��char����ĩβ��'\0'
	stLoginReqInfoEx.uiLoginType = LOGIN_BASIC;
	CToolsHelp::GetHostName(stLoginReqInfoEx.cMachineName, sizeof(stLoginReqInfoEx.cMachineName));

	CUserMgr& userMgr = m_UserMgrArray[iSessionID];//lint !e838
	userMgr.SetSingleSvrProxy(this);
	userMgr.SetSessionID(iSessionID);
	AddSessoinID_SingleSvrProxyToMap(iSessionID, &userMgr);

	int iRet = userMgr.LoginByTicket(&stLoginReqInfoEx);

	// �״ε�¼��������ھ�������¼�ɹ����ɵ��÷����ݷ����������û��޸�����;
	if(IVS_SUCCEED != iRet && IVS_SMU_USER_FIRST_LOGIN != iRet && IVS_SMU_USER_PWD_NEED_MOD != iRet)
	{
		(void)FreeSessoinID(iSessionID);
		userMgr.SetSessionID(SDK_SESSIONID_INIT);
		BP_RUN_LOG_ERR(iRet, "Login By Ticket", "Login is failed");
		return iRet;
	}

	*pSessionId = iSessionID;  //����sessionId

	// Ϊÿһ���û��������ûص�������ָ����ͬ���û�����ΪsessionID;
	(void)RTSP_CLIENT_SetExceptionCallBack(m_eventMgr.GetRtspExceptionCallBack(), (void*)iSessionID);
	IVS_NETSOURCE_SetExceptionCallBack(m_eventMgr.GetNetExceptionCallBack(), (void*)iSessionID);
	IVS_PLAY_SetExceptionCallBack(m_eventMgr.GetPlayerExceptionCallBack(), (void*)iSessionID);   
	IVS_PLAY_SetEventCallBack(CStreamEventMgr::instance().GetPlayerEventCallBack(), (void*)iSessionID);
	return iRet;
}


// window�û���¼
IVS_INT32 CSDKInterface::WinUserLogin(const IVS_LOGIN_INFO* pLoginReqInfo, IVS_INT32* pSessionId)//lint !e830
{
    // ��������;
	CHECK_POINTER(pLoginReqInfo, IVS_OPERATE_MEMORY_ERROR);
	CHECK_POINTER(pSessionId, IVS_OPERATE_MEMORY_ERROR);

	//IP����У�� 
	if ((IP_V6 != pLoginReqInfo->stIP.uiIPType) && (IP_V4 != pLoginReqInfo->stIP.uiIPType))
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "stIP.iIPType is Error");
		return IVS_PARA_INVALID;
	}

	//�û�����IP��������У��
    if (0 == strlen(pLoginReqInfo->cUserName) || 0 == strlen(pLoginReqInfo->stIP.cIP))
	// if ((NULL == pLoginReqInfo->cUserName) || (NULL == pLoginReqInfo->stIP.cIP)) //lint !e774    
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "Param is Error");
		return IVS_PARA_INVALID;
	}

	//�ͻ�������У��
	IVS_INT32 iCheckClientType = CheckClientType(pLoginReqInfo->uiClientType);
	if (IVS_SUCCEED != iCheckClientType)
	{
		BP_RUN_LOG_ERR(IVS_PARA_INVALID, "Login ", "Client Type: %d is error", pLoginReqInfo->uiClientType);
		return IVS_PARA_INVALID;
	}

	// ɾ����ͬ�û���������û�����IP������
	(void)RemoveUser(pLoginReqInfo->cUserName, pLoginReqInfo->stIP.cIP, pLoginReqInfo->cDomainName);

	int iSessionID = GetSessoinID();
	if(SDK_SESSIONID_INIT == iSessionID)  
	{
		// �Ѵ�����û���;
		BP_RUN_LOG_WAR("Login ", "User's number is the max");
		return IVS_SDK_ERR_ERR_MAX_USERNUM;
	}

	//�����ڲ��ṹ�壬
	IVS_LOGIN_INFO stLoginReqInfo;
	eSDK_MEMSET(&stLoginReqInfo, 0, sizeof(stLoginReqInfo));
	if(!CToolsHelp::Memcpy(&stLoginReqInfo,sizeof(IVS_LOGIN_INFO),pLoginReqInfo,sizeof(IVS_LOGIN_INFO)))
	{
		BP_RUN_LOG_ERR(IVS_ALLOC_MEMORY_ERROR, "Login", "stLoginReqInfo Memcpy error.");
		return IVS_ALLOC_MEMORY_ERROR;
	}
    stLoginReqInfo.stIP.cIP[IVS_IP_LEN - 1] = '\0'; //IPʵ�ʳ��Ȳ���ﵽ64��ǿ��char����ĩβ��'\0'
	CToolsHelp::GetHostName(stLoginReqInfo.cMachineName, sizeof(stLoginReqInfo.cMachineName));

	CUserMgr& userMgr = m_UserMgrArray[iSessionID];//lint !e838
	userMgr.SetSingleSvrProxy(this);
	userMgr.SetSessionID(iSessionID);
    AddSessoinID_SingleSvrProxyToMap(iSessionID, &userMgr);

	int iRet = userMgr.Login(&stLoginReqInfo);
	if(IVS_SUCCEED != iRet)
	{
		(void)FreeSessoinID(iSessionID);
        userMgr.SetSessionID(SDK_SESSIONID_INIT);
		BP_RUN_LOG_ERR(iRet, "Login ", "Login is failed");
		return iRet;
	}

	*pSessionId = iSessionID;  //����sessionId

    // Ϊÿһ���û��������ûص�������ָ����ͬ���û�����ΪsessionID;
    (void)RTSP_CLIENT_SetExceptionCallBack(m_eventMgr.GetRtspExceptionCallBack(), (void*)iSessionID);
    IVS_NETSOURCE_SetExceptionCallBack(m_eventMgr.GetNetExceptionCallBack(), (void*)iSessionID);
    IVS_PLAY_SetExceptionCallBack(m_eventMgr.GetPlayerExceptionCallBack(), (void*)iSessionID);    
    IVS_PLAY_SetEventCallBack(CStreamEventMgr::instance().GetPlayerEventCallBack(), (void*)iSessionID);
    return iRet;
}

// ����loginID��ѯ SessionID
IVS_INT32 CSDKInterface::GetSessionIDByLoginID(const string &strLoginID)
{
	IVS_DEBUG_TRACE("LoginID:%s", strLoginID.c_str());
	// ��ѯ�ѵ�½�ĻỰ����ȡ��Ӧ��iSessionID
	int iSessionID = SDK_SESSIONID_INIT;
	if (strLoginID.empty())
	{
		BP_RUN_LOG_INF("Get Session ID By Login ID", "Leave strLoginID is empty");
		return iSessionID;
	}
	// TODO strLoginIDΪ����ʱ������ܱȽ�
	CUserMgr *pSingleSvrProxy = NULL;
	(void)VOS_MutexLock(m_pMutexMapLock);
	
	string strLoginIDTemp;
	MAP_SESSEIONID_USERMGR_ITER iter = m_mapSessionIDToSingleSvrProxy.begin();
	MAP_SESSEIONID_USERMGR_ITER iterEnd = m_mapSessionIDToSingleSvrProxy.end();
	for (;iter != iterEnd; iter++)
	{
		pSingleSvrProxy = dynamic_cast<CUserMgr*>(iter->second);  //lint !e611
	    if (NULL == pSingleSvrProxy)
	    {
			continue;
	    }
		strLoginIDTemp = pSingleSvrProxy->GetLoginId();
		if (0 == strLoginIDTemp.compare(strLoginID))
		{
			iSessionID = iter->first;
			break;
		}
	}
	(void)VOS_MutexUnlock(m_pMutexMapLock);
	return iSessionID;
}

// ����Rtsp�����ȡCUserMgr
CUserMgr*  CSDKInterface::GetUserMgrbyRtspHandle(const IVS_ULONG iRtspHandle)
{
	IVS_DEBUG_TRACE("RtspHandle:0x%p", iRtspHandle);
	IVS_ULONG ulPlayHandle = 0;
	CUserMgr *pUserMgr = NULL;

	(void)VOS_MutexLock(m_pMutexMapLock);

	MAP_SESSEIONID_USERMGR_ITER iter = m_mapSessionIDToSingleSvrProxy.begin();
	MAP_SESSEIONID_USERMGR_ITER iterEnd = m_mapSessionIDToSingleSvrProxy.end();
	for (;iter != iterEnd; iter++)
	{
		pUserMgr = iter->second;
		if (NULL == pUserMgr)
		{
			continue;
		}
		ulPlayHandle = pUserMgr->GetPlayHandlebyRtspHandle(iRtspHandle);
		if (0 < ulPlayHandle)
		{
			break;
		}
	}
	if (iterEnd == iter)
	{
		pUserMgr =NULL;
	}
	(void)VOS_MutexUnlock(m_pMutexMapLock);
	return pUserMgr;
}

// �û�ע��
int CSDKInterface::Logout(int iSessionID, bool iSendLogoutCmd)
{
    IVS_DEBUG_TRACE("SessionID:%d, SendLogoutCmd:%d", iSessionID, (int)iSendLogoutCmd);
	if((SDK_CLIENT_STARTNO > iSessionID) || (SDK_CLIENT_TOTAL <= iSessionID))
	{
		BP_RUN_LOG_ERR(IVS_SDK_RET_INVALID_SESSION_ID, "Logout ", "SessionID is Illegal");
		return IVS_SDK_RET_INVALID_SESSION_ID;
	}

	// ����û�����
	CUserMgr *pUserMgr = GetUserMgr(iSessionID);
	if(NULL == pUserMgr)
	{
		BP_RUN_LOG_ERR(IVS_SDK_RET_INVALID_SESSION_ID, "Logout ", "Get UserMgr failed");
		return IVS_SDK_RET_INVALID_SESSION_ID;
	}

	//add by zwx211831, Date:20140618, ȡ����ע����¼��ص�����
	pUserMgr->SetEventCallBack(NULL, NULL);
	(void)m_eventMgr.SetEventCallBack(iSessionID, NULL, NULL);
	//end

	pUserMgr->GetRealPlayMgr().StopAllRealPlay();
	//2015-2-7 ������ add
	//ע����ֹͣ¼��طź�Զ�˴���
	pUserMgr->GetPlaybackMgr().StopAllPlayBack();
	pUserMgr->GetTelepresenceMgr().StopAllPlayByIP();
	// ɾ�����������ӵĽ�����
	pUserMgr->GetTVWallMgr().DecoderRelease();

	(void)FreeSessoinID(iSessionID);
	// modify by ywx313149 for DTS2016062709309
	//CCapabilityMgr::instance().close();
	return  pUserMgr->Logout(iSendLogoutCmd);
}

// ɾ���û�
int CSDKInterface::RemoveUser(const string &strUserName, const string &strIP, const string &strDomainName)
{
	IVS_DEBUG_TRACE("UserName:%s IP:%s DomainName:%s", strUserName.c_str(), strIP.c_str(), strDomainName.c_str());

	int iSessionID = SDK_SESSIONID_INIT;
	(void)VOS_MutexLock(m_pMutexMapLock);
	MAP_SESSEIONID_USERMGR_ITER svrProxyIter = m_mapSessionIDToSingleSvrProxy.begin();
	for (; svrProxyIter != m_mapSessionIDToSingleSvrProxy.end(); svrProxyIter++)
	{
        CUserMgr *pUserMgr = svrProxyIter->second;
        if (NULL == pUserMgr)
        {
            continue;
        }

		CUserInfo *pUserInfo = pUserMgr->GetUserInfoEx();
        if (NULL == pUserInfo)
        {
            continue;
        }
		
		if (strDomainName.empty())
		{
			// ֻ�����û�����IP
			if ((strUserName == pUserInfo->GetLoginName())
				&&(strIP == pUserInfo->GetServerIP())
				)
			{
				iSessionID = pUserInfo->GetSdkSessionID();
				break;
			}
		}
		else
		{
			// �����û�����IP������
			if ((strUserName == pUserInfo->GetLoginName())
				&&(strIP == pUserInfo->GetServerIP())
				&& (strDomainName == pUserInfo->GetDomainName()))
			{
				iSessionID = pUserInfo->GetSdkSessionID();
				break;
			}
		}
	}
	(void)VOS_MutexUnlock(m_pMutexMapLock);

	if (SDK_SESSIONID_INIT != iSessionID)
	{
		return Logout(iSessionID);
	}
	return IVS_FAIL;
}

//�ǳ��ѵ����¼���û�;
IVS_INT32 CSDKInterface::RemoveUserByTicketID(const string &strTicketID, const string &strIP, const string &strDomainName)
{
	IVS_DEBUG_TRACE("TicketID:%s IP:%s DomainName:%s", strTicketID.c_str(), strIP.c_str(), strDomainName.c_str());

	int iSessionID = SDK_SESSIONID_INIT;
	(void)VOS_MutexLock(m_pMutexMapLock);
	MAP_SESSEIONID_USERMGR_ITER svrProxyIter = m_mapSessionIDToSingleSvrProxy.begin();
	for (; svrProxyIter != m_mapSessionIDToSingleSvrProxy.end(); svrProxyIter++)
	{
		CUserMgr *pUserMgr = svrProxyIter->second;
		if (NULL == pUserMgr)
		{
			continue;
		}

		CUserInfo *pUserInfo = pUserMgr->GetUserInfoEx();
		if (NULL == pUserInfo)
		{
			continue;
		}

		if (strDomainName.empty())
		{
			// ֻ�����û�����IP;
			if ((strTicketID == pUserInfo->GetTicketID())
				&&(strIP == pUserInfo->GetServerIP())
				)
			{
				iSessionID = pUserInfo->GetSdkSessionID();
				break;
			}
		}
		else
		{
			// �����û�����IP������;
			if ((strTicketID == pUserInfo->GetTicketID())
				&&(strIP == pUserInfo->GetServerIP())
				&& (strDomainName == pUserInfo->GetDomainName()))
			{
				iSessionID = pUserInfo->GetSdkSessionID();
				break;
			}
		}
	}
	(void)VOS_MutexUnlock(m_pMutexMapLock);

	if (SDK_SESSIONID_INIT != iSessionID)
	{
		return Logout(iSessionID);
	}
	return IVS_FAIL;
}

//�ͷ�SDK������ڴ�
IVS_INT32 CSDKInterface::ReleaseBuffer(IVS_CHAR *pBuffer) const
{
    CHECK_POINTER(pBuffer, IVS_NULL_POINTER);
	IVS_DEBUG_TRACE("pBuffer: %08x", pBuffer);
	IVS_DELETE(pBuffer, MULTI);
	return IVS_SUCCEED;
}


void CSDKInterface::AddTransIDChannelListRetriveStatusMap(std::string& strTransID, IVS_CHANNLELIST_RETRIVE_STATUS_INFO& channelListRetriveStatus)
{

    (void)VOS_MutexLock(m_pRetriveStatusMapLock);
    m_ChannelListRetriveStatusMap.insert(std::make_pair(strTransID, channelListRetriveStatus));
    (void)VOS_MutexUnlock(m_pRetriveStatusMapLock);

}

void CSDKInterface::RemoveChannelListRetriveStatusByTransID(std::string& strTransID)
{
    (void)VOS_MutexLock(m_pRetriveStatusMapLock);
    (void)m_ChannelListRetriveStatusMap.erase(strTransID);
    (void)VOS_MutexUnlock(m_pRetriveStatusMapLock);
}

IVS_INT32 CSDKInterface::GetChannelListRetriveStatusByTransID(std::string& strTransID, IVS_CHANNLELIST_RETRIVE_STATUS_INFO& channelListRetriveStatus)
{
    (void)VOS_MutexLock(m_pRetriveStatusMapLock);
    std::map<std::string, IVS_CHANNLELIST_RETRIVE_STATUS_INFO>::iterator itor = m_ChannelListRetriveStatusMap.find(strTransID);
    if (m_ChannelListRetriveStatusMap.end() == itor)
    {
        (void)VOS_MutexUnlock(m_pRetriveStatusMapLock);
        return IVS_FAIL;
    }
    channelListRetriveStatus = itor->second;
    (void)VOS_MutexUnlock(m_pRetriveStatusMapLock);
    return IVS_SUCCEED;
}

// ���ͻ������ͺϷ���
IVS_INT32 CSDKInterface::CheckClientType(IVS_UINT32 uiClientType)const
{
	if(uiClientType <= CLIENT_VCM)
	{
		return IVS_SUCCEED;
	}

	BP_RUN_LOG_INF("", "Layout Manager check client type error.");
	return IVS_FAIL;
}