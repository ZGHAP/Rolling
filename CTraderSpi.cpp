#include <windows.h>
#include <iostream>
#include <iomanip>
#include <queue>
#include "CAssitant.h"
#include "CTraderSpi.h"

#include "CTP_TT_Msg.pb.h"

#pragma warning(disable : 4996)

using namespace std;

// USER_API参数

extern CThostFtdcTraderApi* pTraderApi;

extern bool OpenAsstistParam;

// 配置参数

extern char T_FRONT_ADDR[];		// 前置地址

// 会话参数
extern TThostFtdcFrontIDType	FRONT_ID;	   // 前置编号
extern TThostFtdcSessionIDType	SESSION_ID;	   // 会话编号
extern TThostFtdcOrderRefType	ORDER_REF;	   // 报单引用
extern TThostFtdcBrokerIDType	BROKER_ID;	   // 经纪公司代码
extern TThostFtdcInvestorIDType INVESTOR_ID;   // 投资者代码
extern TThostFtdcPasswordType   PASSWORD;	   // 用户密码

extern TThostFtdcPriceType	LIMIT_PRICE;	// 价格
extern TThostFtdcDirectionType	DIRECTION;	// 买卖方向 

extern CRITICAL_SECTION csOperatingOrderSysID;
extern CRITICAL_SECTION csInstrumentMapLock;
//extern map<string, CRITICAL_SECTION> mp_csOrderDoneFlag;

extern char **ppInstrumentID;	// 合约代码
extern int iInstrumentID;
extern int PULL_ORDER_LIMIT;

extern vector<int> vcOrderRef;
extern vector<string> vcSysIDRef;

// 请求编号
extern int iRequestID;

extern CRITICAL_SECTION csInsert;
extern CRITICAL_SECTION csCancel;

extern queue<CThostFtdcOrderField> qTradeRtnQueue;
extern CRITICAL_SECTION csTradeRtn;
extern HANDLE hTradeRtnEvent;

extern map<string, CAssitant*> mpAssitant;
extern map<string, string> mpRollingContract;


// 流控判断
bool IsFlowControl(int iResult)
{
	return ((iResult == -2) || (iResult == -3));
}

void CTraderSpi::OnFrontConnected()
{
	cerr << " --->>> " << "OnFrontConnected" << endl;
	///用户登录请求
	ReqUserLogin();
}

void CTraderSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	cerr<<" --->>> Trader login BROKER_ID:"<<req.BrokerID<<endl;
	strcpy(req.UserID, INVESTOR_ID);
	cerr<<" --->>> Trader login INVESTOR_ID:"<<req.UserID<<endl;
	strcpy(req.Password, PASSWORD);
	int iResult = pTraderApi->ReqUserLogin(&req, ++iRequestID);
	cerr << " --->>> Request Login " << ((iResult == 0) ? "  " : " Failed") << endl;
}

void CTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << " --->>> " << "OnRspUserLogin" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// 保存会话参数
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		sprintf(ORDER_REF, "%d", atoi(pRspUserLogin->MaxOrderRef));
		///获取当前交易日
		cerr << " --->>> Current Date = " << pTraderApi->GetTradingDay() << endl;
		TradingDate=pTraderApi->GetTradingDay();
		///投资者结算结果确认
		ReqSettlementInfoConfirm();
	}
}

void CTraderSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	int iResult = pTraderApi->ReqSettlementInfoConfirm(&req, ++iRequestID);
	cerr << " --->>> Settlement Info Confirm " << ((iResult == 0) ? "  " : " Failed") << endl;
}

void CTraderSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << " --->>> " << "OnRspSettlementInfoConfirm" << endl;
    //cerr << " --->>> InvestorID " << pSettlementInfoConfirm->InvestorID << endl;
	cerr << "-----------------------------------------"<<endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///请求查询合约
		ReqQryInstrument();
	}
}

void CTraderSpi::ReqQryInstrument()
{
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));

	int i = 0;
	while (true)
	{
		int iResult = pTraderApi->ReqQryInstrument(&req, ++iRequestID);
		if (!IsFlowControl(iResult))
		{
			cerr << " --->>> ReqQryInstrument " << ((iResult == 0) ? "  " : " Failed") << endl;
			break;
		}
		else
		{
			cerr << " --->>> ReqQryInstrument " << ", Flow Control" << endl;
			Sleep(1000);
		}
	}
}

void CTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	string tempID = pInstrument->InstrumentID;
	for (int i = 0; i < iInstrumentID;i++)
	{ 
		ofstream InstrumentInfo("ContractList.txt", ios::app);
		if ((strcmp(pInstrument->InstrumentID, ppInstrumentID[i]) == 0) && InstrumentInfo.is_open())
		{
			InstrumentInfo << " --->>> Trading IntrumentID：" << pInstrument->InstrumentID << endl;
			InstrumentInfo << " --->>> Trading Intrument Name：" << pInstrument->InstrumentName << endl;
			InstrumentInfo << " --->>> Intrument Tick Size：" << pInstrument->PriceTick << endl;
			InstrumentInfo << " --->>> Instrument Leverage：" << pInstrument->VolumeMultiple << endl;
			InstrumentInfo << " --->>> ExchangeID：" << pInstrument->ExchangeID << endl;
			InstrumentInfo << "-----------------------------------------" << endl;

			//mpAssitant[pInstrument->InstrumentID]->InstrumentTickSize = pInstrument->PriceTick;
			//mpAssitant[pInstrument->InstrumentID]->InstrumentLeverage = pInstrument->VolumeMultiple;
			//mpAssitant[pInstrument->InstrumentID]->InstrumentExchangeID = pInstrument->ExchangeID;

		}
	}
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///请求查询账户
		ReqQryTradingAccount();
	}
}

void CTraderSpi::ReqQryTradingAccount()
{
	
	CThostFtdcQryTradingAccountField req;
	CThostFtdcTradingAccountField TA;
	memset(&req, 0, sizeof(req));
	memset(&TA, 0, sizeof(TA));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	while (true)
	{
		int iResult = pTraderApi->ReqQryTradingAccount(&req, ++iRequestID);
		if (!IsFlowControl(iResult))
		{
			cerr << " --->>> Request Query Account " << ((iResult == 0) ? "  " : " Failed") << endl;
			break;
		}
		else
		{
			cerr << " --->>> Request Query Account " << iResult << ", Flow Control" << endl;
			Sleep(1000);
		}
	} // while
}

void CTraderSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	double preBalance=pTradingAccount->PreBalance-pTradingAccount->Withdraw+pTradingAccount->Deposit;
	double current=preBalance+pTradingAccount->CloseProfit+pTradingAccount->PositionProfit-pTradingAccount->Commission;
	cerr << " --->>> " << "OnRspQryTradingAccount" << endl;
	cerr << " --->>> Static  Balance： " <<setprecision(2)<<setiosflags(ios::fixed)<< preBalance << endl;
	cerr << " --->>> Current Balance： " <<setprecision(2)<<setiosflags(ios::fixed)<< current << endl;
	cerr << "-----------------------------------------"<<endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///请求查询投资者持仓
		ReqQryInvestorPosition();
	}
}

void CTraderSpi::ReqQryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	//strcpy(req.InstrumentID, *ppInstrumentID);
	while (true)
	{
		int iResult = pTraderApi->ReqQryInvestorPosition(&req, ++iRequestID);
		if (!IsFlowControl(iResult))
		{
			cerr << " --->>> Request Query Position " << ((iResult == 0) ? "  " : " Failed") << endl;
			break;
		}
		else
		{
			cerr << " --->>> Request Query Position " << iResult << ", Flow Control" << endl;
			Sleep(1000);
		}
	} // while
}

void CTraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{

	//cerr << " --->>> " << "OnRspQryInvestorPosition" << endl;
	ofstream PosiInfo("OnRspQryInvestorPosition.txt", ios::app);
	if(pInvestorPosition!=NULL)
	{
		if (PosiInfo.is_open())
		{
			PosiInfo << " --->>> Instrument       ： " << pInvestorPosition->InstrumentID << endl;
			PosiInfo << " --->>> Realized   Profit： " << pInvestorPosition->CloseProfit << endl;
			PosiInfo << " --->>> Unrealized Profit： " << pInvestorPosition->PositionProfit << endl;
			PosiInfo << " --->>> Net Position： " << pInvestorPosition->Position << endl;
			PosiInfo << " --->>> Today Position： " << pInvestorPosition->TodayPosition << endl;
			string ID = pInvestorPosition->InstrumentID;
			//if (pInvestorPosition->PosiDirection == '2'
			//	&& mpAssitant.find(ID) != mpAssitant.end())
			//{
			//	PosiInfo << " --->>> Direciton：Long " << endl;
			//	string ID = pInvestorPosition->InstrumentID;
			//	double cost = pInvestorPosition->OpenCost;
			//	if (PULL_ORDER_LIMIT != 999999)
			//	{
			//		if (pInvestorPosition->InstrumentID == mpAssitant[pInvestorPosition->InstrumentID]->InstrumentID)
			//		{
			//			if (pInvestorPosition->YdPosition != 0)
			//				mpAssitant[pInvestorPosition->InstrumentID]->YdInventory += pInvestorPosition->YdPosition;
			//			else
			//				mpAssitant[pInvestorPosition->InstrumentID]->LongInventory = pInvestorPosition->TodayPosition;
			//		}
			//		//PosiInfo << " --->>> Cost " << cost / (double)ArbitrageSet->InstrumentLeverage << endl;
			//		PosiInfo << " --->>> YdPosi " << pInvestorPosition->YdPosition << endl;
			//		PosiInfo << " --->>> YdInventory " << mpAssitant[pInvestorPosition->InstrumentID]->YdInventory << endl;
			//		PosiInfo << " --->>> TdPosi " << pInvestorPosition->TodayPosition << endl;
			//		PosiInfo << " --->>> ShortInventory " << mpAssitant[pInvestorPosition->InstrumentID]->LongInventory << endl;
			//	}
			//}
			//else if (pInvestorPosition->PosiDirection == '3'
			//	&& mpAssitant.find(ID) != mpAssitant.end())
			//{
			//	PosiInfo << " --->>> Direciton：Short " << endl;
			//	string ID = pInvestorPosition->InstrumentID;
			//	double cost = pInvestorPosition->OpenCost;
			//	if (PULL_ORDER_LIMIT != 999999)
			//	{
			//		if (pInvestorPosition->InstrumentID == mpAssitant[pInvestorPosition->InstrumentID]->InstrumentID)
			//		{
			//			if (pInvestorPosition->YdPosition != 0)
			//				mpAssitant[pInvestorPosition->InstrumentID]->YdInventory -= pInvestorPosition->YdPosition;
			//			else
			//				mpAssitant[pInvestorPosition->InstrumentID]->ShortInventory = pInvestorPosition->TodayPosition;
			//		}
			//		//PosiInfo << " --->>> Cost " << cost / (double)ArbitrageSet->InstrumentLeverage << endl;
			//		PosiInfo << " --->>> YdPosi " << pInvestorPosition->YdPosition << endl;
			//		PosiInfo << " --->>> YdInventory " << mpAssitant[pInvestorPosition->InstrumentID]->YdInventory << endl;
			//		PosiInfo << " --->>> TdPosi " << pInvestorPosition->TodayPosition << endl;
			//		PosiInfo << " --->>> ShortInventory " << mpAssitant[pInvestorPosition->InstrumentID]->ShortInventory << endl;
			//	}
			//}
			//else
			//	PosiInfo << " --->>> No Position " << endl;
		}

		PosiInfo << "-----------------------------------------"<<endl;
	}
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{

	}
			
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//订单处理部分
//日后需要设计一个类去集成这部分操作
//然后策略类继承这个订单处理类进行交易
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << " --->>> " << "OnRspOrderInsert" << endl;
	IsErrorRspInfo(pRspInfo);
}

void CTraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << " --->>> " << "OnRspOrderAction" << endl;
	IsErrorRspInfo(pRspInfo);
}

///报单通知
void CTraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	//cerr << " --->>> OnRtnOrder"  << endl;
	//system("PAUSE");
	CThostFtdcOrderField *pParam = new CThostFtdcOrderField;
	
	//copy pOrder
	strcpy(pParam->InsertTime, pOrder->InsertTime);
	strcpy(pParam->OrderRef, pOrder->OrderRef);
	strcpy(pParam->OrderSysID, pOrder->OrderSysID);
	strcpy(pParam->InstrumentID, pOrder->InstrumentID);
	strcpy(pParam->CombOffsetFlag, pOrder->CombOffsetFlag);
	strcpy(pParam->BrokerID, pOrder->BrokerID);
	strcpy(pParam->InvestorID, pOrder->InvestorID);
	strcpy(pParam->InstrumentID, pOrder->InstrumentID);
	strcpy(pParam->ExchangeID, pOrder->ExchangeID);

	pParam->Direction = pOrder->Direction;
	pParam->LimitPrice = pOrder->LimitPrice;
	pParam->VolumeTotalOriginal = pOrder->VolumeTotalOriginal;
	pParam->VolumeTotal = pOrder->VolumeTotal;
	pParam->VolumeTraded = pOrder->VolumeTraded;
	pParam->VolumeCondition = pOrder->VolumeCondition;
	pParam->OrderStatus = pOrder->OrderStatus;

	if (mpAssitant.find(pParam->InstrumentID) != mpAssitant.end()) {
		AddToThreadPool(pParam,true);
	}
	//else {
	//	for (map<string, CAssitant*>::iterator mpit = mpAssitant.begin();
	//		mpit != mpAssitant.end();
	//		++mpit) {
	//		if (mpit->second->RollingInstrumentID == pParam->InstrumentID) {
	//			AddToThreadPool(pParam,false);
	//		}
	//	}
	//}
}

///成交通知
void CTraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	////cerr << " --->>> " << "OnRtnTrade"  << endl;
	if (mpAssitant.find(pTrade->InstrumentID) != mpAssitant.end()){
		EnterCriticalSection(&csInstrumentMapLock);
		mpAssitant[pTrade->InstrumentID]->SetPositionStatus(pTrade);
		LeaveCriticalSection(&csInstrumentMapLock);
	}
	else {
		for (map<string, CAssitant*>::iterator mpit = mpAssitant.begin();
			mpit != mpAssitant.end();
			++mpit) {
			if (mpit->second->RollingInstrumentID == pTrade->InstrumentID) {
				mpit->second->SetInitPositionStatus(pTrade);
			}
		}
	}
}

//判断是否自己的单子
bool CTraderSpi::IsMyOrder(CThostFtdcOrderField *pOrder)
{
	EnterCriticalSection(&csInsert);

	bool REF = false;
	vector<int>::iterator it = find(vcOrderRef.begin(), vcOrderRef.end(), atoi(pOrder->OrderRef));
	if (it != vcOrderRef.end())
		REF = true;

	LeaveCriticalSection(&csInsert);

	return ((pOrder->FrontID == FRONT_ID) &&
			(pOrder->SessionID == SESSION_ID) &&
			REF);
}

//判断是否自己的交易
bool CTraderSpi::IsMyTrades(CThostFtdcTradeField *pTrade)
{
	EnterCriticalSection(&csOperatingOrderSysID);

	bool REF;
	vector<string>::iterator it = find(vcSysIDRef.begin(), vcSysIDRef.end(), pTrade->OrderSysID);
	if (it != vcSysIDRef.end())
		REF = true;
	else
		REF = false;

	LeaveCriticalSection(&csOperatingOrderSysID);

	return REF;
}

//判断单子是否仍在交易状态，如何情况均当作处于交易状态1、部分成交仍在队列；2、部分成交不在队列；3、没有成交
bool CTraderSpi::IsTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus == THOST_FTDC_OST_PartTradedQueueing)          //部分成交仍在队列
			||(pOrder->OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing)     //部分成交不在队列
			||(pOrder->OrderStatus == THOST_FTDC_OST_NoTradeQueueing)			//完全未成交仍在队列
			||(pOrder->OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing));      //完全未成交不在队列
}

void CTraderSpi::OnFrontDisconnected(int nReason)
{
	cerr << " --->>> " << "OnFrontDisconnected" << endl;
	cerr << " --->>> Reason = " << nReason << endl;
}

void CTraderSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << " --->>> " << "OnHeartBeatWarning" << endl;
	cerr << " --->>> nTimerLapse = " << nTimeLapse << endl;
}

void CTraderSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << " --->>> " << "OnRspError" << endl;
	IsErrorRspInfo(pRspInfo);
}

bool CTraderSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
	{
		string sz_path = "ErrorLog_" + TradingDate + ".txt";
		ofstream ErrorLog(sz_path, ios::app);
		if (ErrorLog.is_open())
			ErrorLog << " --->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
		ErrorLog.close();
	}
	return bResult;
}

void CTraderSpi::AddToThreadPool(CThostFtdcOrderField *pOrder,bool IsRollingMonth)
{
	BOOL rBet = TrySubmitThreadpoolCallback(OrderRtnCallBack, pOrder, NULL);
	if (!rBet)
	{
		cerr << "Can not add to thread pool\n";
		system("PAUSE");
		exit(0);
	}
	
}

void CTraderSpi::OrderRtnCallBack(PTP_CALLBACK_INSTANCE pInstance, PVOID pvContext)
{
	CThostFtdcOrderField *pParam = (CThostFtdcOrderField*)pvContext;
	string RESULT;
	string TradingDate = pParam->InsertDate;

	cerr << "OrderRtnCallBack\n";

	EnterCriticalSection(&csInstrumentMapLock);
	////////////////////////////////////////////////////////
	if (pParam->OrderStatus == THOST_FTDC_OST_NoTradeQueueing)
	{
		//EnterCriticalSection(&mp_csOrderDoneFlag[pParam->InstrumentID]);
		EnterCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentUnfilledLock);
		//////////////////////////////////////////////////////////
		mpAssitant[pParam->InstrumentID]->SetCancelField(pParam, pParam->VolumeTotal);
		mpAssitant[pParam->InstrumentID]->PullRestOrder(pParam);
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentUnfilledLock);

		RESULT = "Order Inserted";
		//cerr << RESULT << endl;

	}
	else if (pParam->OrderStatus == THOST_FTDC_OST_PartTradedQueueing)
	{
		EnterCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentUnfilledLock);
		//////////////////////////////////////////////////////////
		mpAssitant[pParam->InstrumentID]->SetUnfilledLots(pParam->OrderSysID, pParam->VolumeTotal, pParam->LimitPrice);
		mpAssitant[pParam->InstrumentID]->PullRestOrder(pParam);
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentUnfilledLock);

		RESULT = "Order Partly Filled";
		//cerr << RESULT << endl;

	}
	else if (pParam->OrderStatus == THOST_FTDC_OST_Canceled)
	{
		EnterCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentOrderRtnLock);
		//////////////////////////////////////////////////////////
		mpAssitant[pParam->InstrumentID]->ResetOrderDoneFlag(pParam);
		mpAssitant[pParam->InstrumentID]->DeleteLimitOrder(pParam->LimitPrice, pParam->OrderSysID, pParam->Direction, pParam->VolumeCondition);
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentOrderRtnLock);

		//Operating vcOrderRef
		EnterCriticalSection(&csInsert);
		//////////////////////////////////////////////////////////
		vector<int>::iterator OrderRefIt = find(vcOrderRef.begin(), vcOrderRef.end(), atoi(pParam->OrderRef));
		if (OrderRefIt != vcOrderRef.end())
			vcOrderRef.erase(OrderRefIt);
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&csInsert);

		RESULT = "Order Canceled";
		//cerr << RESULT << endl;

	}
	else if (pParam->OrderStatus == THOST_FTDC_OST_AllTraded)
	{
		EnterCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentOrderRtnLock);
		//////////////////////////////////////////////////////////
		mpAssitant[pParam->InstrumentID]->ResetOrderDoneFlag(pParam);
		mpAssitant[pParam->InstrumentID]->DeleteLimitOrder(pParam->LimitPrice, pParam->OrderSysID, pParam->Direction, pParam->VolumeCondition);
		cerr << pParam->InstrumentID << " " << pParam->VolumeTraded << endl;
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&mpAssitant[pParam->InstrumentID]->csInstrumentOrderRtnLock);

		//Operating vcOrderRef
		EnterCriticalSection(&csInsert);
		//////////////////////////////////////////////////////////
		vector<int>::iterator OrderRefIt = find(vcOrderRef.begin(), vcOrderRef.end(), atoi(pParam->OrderRef));
		if (OrderRefIt != vcOrderRef.end())
			vcOrderRef.erase(OrderRefIt);
		//////////////////////////////////////////////////////////
		LeaveCriticalSection(&csInsert);

		RESULT = "Order All Filled";
		//cerr << RESULT << endl;

	}
	////////////////////////////////////////////////////////
	LeaveCriticalSection(&csInstrumentMapLock);

	EnterCriticalSection(&csOperatingOrderSysID);
	//////////////////////////////////////////////////////////
	string TempSysID = pParam->OrderSysID;
	vcSysIDRef.push_back(TempSysID);
	//////////////////////////////////////////////////////////
	LeaveCriticalSection(&csOperatingOrderSysID);

	delete pParam;
}