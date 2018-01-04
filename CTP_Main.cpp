// testTraderApi.cpp : 定义控制台应用程序的入口点。
// Design By ZhaomingHu



#include "./include/ThostFtdcTraderApi.h"
#include "./include/ThostFtdcMdApi.h"
#include "CMdSpi.h"
#include "CTraderSpi.h"
#include "CData.h"
#include "tchar.h"
#include <iostream>
#include <Windows.h>
#include <signal.h>
#include <random>
#include <queue>

#include "CAssitant.h"
#include "tinyxml.h"

#pragma comment(lib,"./include/thostmduserapi")
#pragma comment(lib,"./include/thosttraderapi")

using namespace std;

CThostFtdcMdApi *pMdApi;
CThostFtdcTraderApi *pTraderApi;

CMdSpi* pMdSpi = new CMdSpi();
CTraderSpi *pTraderSpi = new CTraderSpi();

//共享内存使用变量
void *pShmBuffer = malloc(1024);
void *pSHM_Ptr = NULL;
HANDLE hSHM_Mapping;
HANDLE hSHM_Mutex;
HANDLE hSHM_Semaphore;

//FrontIP
//HaitongSetting
char  H_M_FRONT_ADDR[] = "tcp://180.168.212.70:41213";
char  H_T_FRONT_ADDR[] = "tcp://180.168.212.70:41205";
//GalaxySetting
char  G_M_FRONT_ADDR[] = "tcp://101.95.8.178 :51213";
char  G_T_FRONT_ADDR[] = "tcp://101.95.8.178 :51205";
//CiticSetting
char  C_M_FRONT_ADDR[] = "tcp://180.169.101.177:41213";
char  C_T_FRONT_ADDR[] = "tcp://180.169.101.177:41205";	

// 会话参数
TThostFtdcFrontIDType	 FRONT_ID;	        // 前置编号
TThostFtdcSessionIDType	 SESSION_ID;	    // 会话编号
TThostFtdcOrderRefType	 ORDER_REF;	        // 报单引用
TThostFtdcBrokerIDType	 BROKER_ID;		    // 经纪公司代码Live
TThostFtdcInvestorIDType INVESTOR_ID;	    // 投资者代码Live
TThostFtdcPasswordType   PASSWORD;			// 用户密码Live 

// 交易品种数组
char **ppInstrumentID;
int iInstrumentID;
// 前置地址
char *M_FRONT_ADDR;
char *T_FRONT_ADDR;		

//订单线程控制组变量
vector<int> vcOrderRef;
vector<string> vcSysIDRef;
queue<CThostFtdcInputOrderField> qToPushOrderQueue;
queue<CThostFtdcInputOrderActionField> qToPullOrderQueue;
CRITICAL_SECTION csOperatingOrderSysID;
CRITICAL_SECTION csInsert;
CRITICAL_SECTION csCancel;
CRITICAL_SECTION csTT_Instrument_Lock;
CRITICAL_SECTION csInstrumentMapLock;
HANDLE hToPushEvent;
HANDLE hToPullEvent;
HANDLE hRtnDepMktOutPut;

// 请求编号
int iRequestID = 0;
int PULL_ORDER_LIMIT;

map<string, CAssitant*> mpAssitant;
map<string, string> mpRollingContract;

DWORD WINAPI Trading(LPVOID lpParameter);                   //交易回报线程
DWORD WINAPI MarketData(LPVOID lpParameter);                //市场数据线程 
DWORD WINAPI InsertOrderThread(LPVOID lpParameter);         //订单发送处理线程
DWORD WINAPI PullingOrderThread(LPVOID lpParameter);        //撤单发送处理线程

void main(int argc, char* argv[])
{
	system("del *.txt");
	system("del *.csv");

	//start strategy thread
	HANDLE hThreadMD;
	HANDLE hThreadTrader;
	HANDLE hInsertOrderThread;
	HANDLE hCancelOrderThread;
	HANDLE hReceiveTTData;
	//HANDLE hSendCTPData;

	//第二参数FALSE 事件触发后将自动复位
	hRtnDepMktOutPut = CreateEvent(NULL, FALSE, FALSE, NULL);
	hToPushEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hToPullEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&csTT_Instrument_Lock);
	InitializeCriticalSection(&csInstrumentMapLock);

	TiXmlDocument xmldoc;
	string ParseFile_Path;

	if (argc > 1)
		ParseFile_Path = argv[1];
	else
		ParseFile_Path = "PairParseFile.xml";

	iInstrumentID = 0;
	ppInstrumentID = (char**)malloc(sizeof(char*) * 20);

	if (!xmldoc.LoadFile(ParseFile_Path.c_str()))
	{
		cerr << xmldoc.ErrorDesc() << endl;
		cerr << "ParseFile\n";
	}

	TiXmlElement* root = xmldoc.FirstChildElement();
	if (root == NULL) {
		cerr << "No root element\n";
	}
	else {
		string TempID;
		for (TiXmlElement* elem = root->FirstChildElement();
			elem != NULL;
			elem = elem->NextSiblingElement())
		{
			string elemName = elem->Value();
			cerr << elemName << endl;
			if (elemName == "BrokerID") {
				string ID = elem->GetText();
				strcpy(BROKER_ID, ID.c_str());
				cerr << ID << endl;
			}
			if (elemName == "BrokerName") {
				string BrokerName = elem->GetText();
				cerr << BrokerName << endl;
				if (BrokerName == "Galaxy") {
					M_FRONT_ADDR = new char[sizeof(G_M_FRONT_ADDR)];
					T_FRONT_ADDR = new char[sizeof(G_T_FRONT_ADDR)];
					strcpy(M_FRONT_ADDR, G_M_FRONT_ADDR);
					strcpy(T_FRONT_ADDR, G_T_FRONT_ADDR);
				}
				if (BrokerName == "CITIC") {
					M_FRONT_ADDR = new char[sizeof(C_M_FRONT_ADDR)];
					T_FRONT_ADDR = new char[sizeof(C_T_FRONT_ADDR)];
					strcpy(M_FRONT_ADDR, C_M_FRONT_ADDR);
					strcpy(T_FRONT_ADDR, C_T_FRONT_ADDR);
				}
			}
			if (elemName == "ProductName") {
				string Product = elem->GetText();
				cerr << Product << endl;
				if (Product == "Product6") {
					strcpy(INVESTOR_ID, "933293");
					strcpy(PASSWORD, "729312");
				}
				if (Product == "Product7") {
					strcpy(INVESTOR_ID, "620056");
					strcpy(PASSWORD, "729312");
				}
				if (Product == "Product5") {
					strcpy(INVESTOR_ID, "933288");
					strcpy(PASSWORD, "094519");
				}
				if (Product == "Product6_citic") {
					strcpy(INVESTOR_ID, "103201168");
					strcpy(PASSWORD, "094519");
				}
			}
			if (elemName == "Pair") {
				string InitMonth;
				InitMonth.clear();
				for (TiXmlElement *Sub_elem = elem->FirstChildElement();
					Sub_elem != NULL;
					Sub_elem = Sub_elem->NextSiblingElement()) {
					string tempStr = Sub_elem->Value();
					if (tempStr == "RollingMonth") {
						TempID = Sub_elem->GetText();
						cerr << TempID << endl;
						//塞进订阅队列 
						cerr << "InstrumentID " << TempID << " \n";
						ppInstrumentID[iInstrumentID] = (char*)malloc(sizeof(TempID.c_str()) - 1);
						strcpy(ppInstrumentID[iInstrumentID], TempID.c_str());
						iInstrumentID++;
						
						mpAssitant[ppInstrumentID[iInstrumentID - 1]] = new CAssitant(ppInstrumentID[iInstrumentID - 1]);
						mpAssitant[ppInstrumentID[iInstrumentID - 1]]->SetInitInstrumentID(ppInstrumentID[iInstrumentID - 1]);
						InitMonth = TempID;
					}
					if (tempStr == "InitialMonth") {
						TempID = Sub_elem->GetText();
						cerr << TempID << endl;
						//塞进订阅队列 
						cerr << "InstrumentID " << TempID << " \n";
						ppInstrumentID[iInstrumentID] = (char*)malloc(sizeof(TempID.c_str()) - 1);
						strcpy(ppInstrumentID[iInstrumentID], TempID.c_str());
						iInstrumentID++;
						
						mpAssitant[InitMonth]->SetRollInstrumentID(TempID);
					}
				}
			}
		}
	}

	InitializeCriticalSection(&csInsert);
	InitializeCriticalSection(&csCancel);
	InitializeCriticalSection(&csOperatingOrderSysID);

	hInsertOrderThread = CreateThread(NULL, 0, InsertOrderThread, &qToPushOrderQueue, 0, NULL);
	hCancelOrderThread = CreateThread(NULL, 0, PullingOrderThread, &qToPullOrderQueue, 0, NULL);

	hThreadTrader = CreateThread(NULL, 0, Trading, NULL, 0, NULL);
	WaitForSingleObject(hThreadTrader, 5000);

	hThreadMD = CreateThread(NULL, 0, MarketData, NULL, 0, NULL);

	int OrderLots = 0;
	double OrderPrice = 0;

	SetEvent(hRtnDepMktOutPut);

	while (1)
	{
		WaitForSingleObject(hRtnDepMktOutPut, INFINITE);
		////system("cls");
		//int i = 0;
		//for (map<string, CTP_TT_Msg>::iterator mpit = ArbitrageSet->mpTT_Linked_Instrument.begin();
		//	mpit != ArbitrageSet->mpTT_Linked_Instrument.end();
		//	mpit++)
		//{
		//	cerr << ++i << " ";
		//	cerr << "time " << mpit->second.timestamp() << " ";
		//	cerr << "ID " << mpit->second.instrumentid() << " ";
		//	cerr << "bidvol " << mpit->second.bid_size() << " ";
		//	cerr << "bid " << mpit->second.bid_price() << " ";
		//	cerr << "ask " << mpit->second.ask_price() << " ";
		//	cerr << "askvol " << mpit->second.ask_size() << " ";
		//	cerr << "net " << mpit->second.inventory_net() << " ";
		//	cerr << "cost " << mpit->second.inventory_costing() << " ";
		//	cerr << "totalvalue " << mpit->second.positionvalue() << endl;
		//}
		//cerr << endl;
	}

	DeleteCriticalSection(&csInsert);
	DeleteCriticalSection(&csCancel);
	DeleteCriticalSection(&csOperatingOrderSysID);
	DeleteCriticalSection(&csInstrumentMapLock);
	DeleteCriticalSection(&csTT_Instrument_Lock);

	CloseHandle(hThreadMD);
	CloseHandle(hThreadTrader);
	CloseHandle(hInsertOrderThread);
	CloseHandle(hCancelOrderThread);
	CloseHandle(hReceiveTTData);
}

DWORD WINAPI Trading(LPVOID lpParameter)
{
	pTraderApi=CThostFtdcTraderApi::CreateFtdcTraderApi("./Trader");  // 创建TraderApi并指定生成文件名以同时调用两个DLL
	pTraderApi->RegisterSpi(pTraderSpi);
	pTraderApi->RegisterFront(T_FRONT_ADDR);
	pTraderApi->SubscribePrivateTopic(THOST_TERT_QUICK);
	pTraderApi->SubscribePublicTopic(THOST_TERT_QUICK);
	pTraderApi->Init();
	pTraderApi->Join();

	return 0;
}

DWORD WINAPI MarketData(LPVOID lpParameter)
{
	pMdApi = CThostFtdcMdApi::CreateFtdcMdApi("./MarketData");	// 创建MdApi并指定生成文件名以同时调用两个DLL
	pMdApi->RegisterSpi(pMdSpi);	                            // 注册事件类
	pMdApi->RegisterFront(M_FRONT_ADDR);						// connect
    pMdApi->Init();
	pMdApi->Join();

	return 0;
}

DWORD WINAPI InsertOrderThread(LPVOID lpParameter)
{
	cerr << " Sending Order Thread Begin \n";

	queue<CThostFtdcInputOrderField> *p_qToPush = (queue<CThostFtdcInputOrderField>*)lpParameter;

	string OffSetFlag, Direction, Date, ID;

	while (true)
	{
		WaitForSingleObject(hToPushEvent, INFINITE);
		EnterCriticalSection(&csInsert);

		while (!p_qToPush->empty())
		{
			ID = p_qToPush->front().InstrumentID;
			string sz_path = "PendingLog_" + ID + ".csv";
			ofstream PendingLog(sz_path, ios::app);

			CThostFtdcInputOrderField req;
			memset(&req, 0, sizeof(req));

			///经纪公司代码
			strcpy(req.BrokerID, BROKER_ID);
			///投资者代码
			strcpy(req.InvestorID, INVESTOR_ID);
			///合约代码 
			strcpy(req.InstrumentID, p_qToPush->front().InstrumentID);
			///报单引用
			strcpy(req.OrderRef, ORDER_REF);
			///用户代码
			//	TThostFtdcUserIDType	UserID;
			///报单价格条件: 限价
			req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;

			///买卖方向: 
			req.Direction = p_qToPush->front().Direction;

			///组合开平标志: 开仓
			req.CombOffsetFlag[0] = p_qToPush->front().CombOffsetFlag[0];

			///组合投机套保标志
			req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			///价格
			req.LimitPrice = p_qToPush->front().LimitPrice;
			///数量
			req.VolumeTotalOriginal = p_qToPush->front().VolumeTotalOriginal;
			///有效期类型: 当日有效
			req.TimeCondition = THOST_FTDC_TC_GFD;
			///GTD日期
			//	TThostFtdcDateType	GTDDate;
			///成交量类型: 任何数量
			req.VolumeCondition = THOST_FTDC_VC_AV;
			///最小成交量: 1
			req.MinVolume = 1;
			///触发条件: 立即
			req.ContingentCondition = THOST_FTDC_CC_Immediately;
			///止损价
			//	TThostFtdcPriceType	StopPrice;
			///强平原因: 非强平
			req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
			///自动挂起标志: 否
			req.IsAutoSuspend = 0;
			///业务单元
			//	TThostFtdcBusinessUnitType	BusinessUnit;
			///请求编号
			//	TThostFtdcRequestIDType	RequestID;
			///用户强评标志: 否
			req.UserForceClose = 0;

			//报单引用存储
			vcOrderRef.push_back(atoi(ORDER_REF));
			//刷新报单引用
			int iNextOrderRef = atoi(ORDER_REF);
			++iNextOrderRef;
			sprintf(ORDER_REF, "%d", iNextOrderRef);

			int iResult = pTraderApi->ReqOrderInsert(&req, ++iRequestID);

			//cerr << req.InstrumentID << " " << req.LimitPrice << endl;

			if (PendingLog.is_open()
				&& iResult == 0)
			{
				PendingLog << p_qToPush->front().InstrumentID << ",";
				PendingLog << p_qToPush->front().CombOffsetFlag << ",";
				PendingLog << p_qToPush->front().Direction << ",";
				PendingLog << p_qToPush->front().LimitPrice << ",";
				PendingLog << p_qToPush->front().VolumeTotalOriginal << ",";
				PendingLog << p_qToPush->front().OrderRef << endl;
			}
			PendingLog.close();

			p_qToPush->pop();
		}
		//printf("All Order Sent\n");
		
		LeaveCriticalSection(&csInsert);
	}

	return 0;
}

DWORD WINAPI PullingOrderThread(LPVOID lpParameter)
{
	cerr << " Pulling Order Thread Begin \n";

	string OffSetFlag, Direction, Date, ID;

	queue<CThostFtdcInputOrderActionField> *p_qToPull = (queue<CThostFtdcInputOrderActionField>*)lpParameter;

	while (true)
	{
		WaitForSingleObject(hToPullEvent, INFINITE);
		EnterCriticalSection(&csCancel);

		while (!p_qToPull->empty())
		{
			ID = p_qToPull->front().InstrumentID;
			string sz_path = "PullingLog_" + ID + ".csv";
			ofstream PullingLog(sz_path, ios::app);

			//cerr << "Order Queue Count " << p_qToPull->size() << endl;
			//system("PAUSE");
			static bool ORDER_ACTION_SENT;		//是否发送修改订单请求

			CThostFtdcInputOrderActionField req;
			memset(&req, 0, sizeof(req));
			///经纪公司代码
			strcpy(req.BrokerID, p_qToPull->front().BrokerID);
			///投资者代码
			strcpy(req.InvestorID, p_qToPull->front().InvestorID);
			///报单操作引用
			//	TThostFtdcOrderActionRefType	OrderActionRef;
			///报单引用
			strcpy(req.OrderRef, p_qToPull->front().OrderRef);
			///请求编号
			//	TThostFtdcRequestIDType	RequestID;
			///前置编号
			req.FrontID = FRONT_ID;
			///会话编号
			req.SessionID = SESSION_ID;
			///交易所代码
			//	TThostFtdcExchangeIDType	ExchangeID;
			strcpy(req.ExchangeID, p_qToPull->front().ExchangeID);
			///报单编号
			//	TThostFtdcOrderSysIDType	OrderSysID;
			strcpy(req.OrderSysID, p_qToPull->front().OrderSysID);
			///操作标志
			//  THOST_FTDC_AF_Delete 撤单
			req.ActionFlag = THOST_FTDC_AF_Delete;
			///价格
			//	TThostFtdcPriceType	LimitPrice;
			///数量变化
			//	TThostFtdcVolumeType	VolumeChange;
			///用户代码
			//	TThostFtdcUserIDType	UserID;
			///合约代码
			strcpy(req.InstrumentID, p_qToPull->front().InstrumentID);

			int iResult = pTraderApi->ReqOrderAction(&req, ++iRequestID);

			if (iResult != 0)
				cerr << "Pulled Error\n";

			if (PullingLog.is_open()
				&& iResult == 0)
			{
				PullingLog << p_qToPull->front().InstrumentID << ",";
				PullingLog << p_qToPull->front().LimitPrice << ",";
				PullingLog << p_qToPull->front().OrderRef << endl;
			}
			PullingLog.close();

			p_qToPull->pop();
		}

		LeaveCriticalSection(&csCancel);
	}


	return 0;
}
