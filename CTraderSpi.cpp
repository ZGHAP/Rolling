#include <windows.h>
#include <iostream>
#include <iomanip>
#include <queue>
#include "CAssitant.h"
#include "CTraderSpi.h"

#include "CTP_TT_Msg.pb.h"

#pragma warning(disable : 4996)

using namespace std;

// USER_API����

extern CThostFtdcTraderApi* pTraderApi;

extern bool OpenAsstistParam;

// ���ò���

extern char T_FRONT_ADDR[];		// ǰ�õ�ַ

// �Ự����
extern TThostFtdcFrontIDType	FRONT_ID;	   // ǰ�ñ��
extern TThostFtdcSessionIDType	SESSION_ID;	   // �Ự���
extern TThostFtdcOrderRefType	ORDER_REF;	   // ��������
extern TThostFtdcBrokerIDType	BROKER_ID;	   // ���͹�˾����
extern TThostFtdcInvestorIDType INVESTOR_ID;   // Ͷ���ߴ���
extern TThostFtdcPasswordType   PASSWORD;	   // �û�����

extern TThostFtdcPriceType	LIMIT_PRICE;	// �۸�
extern TThostFtdcDirectionType	DIRECTION;	// �������� 

extern CRITICAL_SECTION csOperatingOrderSysID;
extern CRITICAL_SECTION csInstrumentMapLock;
//extern map<string, CRITICAL_SECTION> mp_csOrderDoneFlag;

extern char **ppInstrumentID;	// ��Լ����
extern int iInstrumentID;
extern int PULL_ORDER_LIMIT;

extern vector<int> vcOrderRef;
extern vector<string> vcSysIDRef;

// ������
extern int iRequestID;

extern CRITICAL_SECTION csInsert;
extern CRITICAL_SECTION csCancel;

extern queue<CThostFtdcOrderField> qTradeRtnQueue;
extern CRITICAL_SECTION csTradeRtn;
extern HANDLE hTradeRtnEvent;

extern map<string, CAssitant*> mpAssitant;
extern map<string, string> mpRollingContract;


// �����ж�
bool IsFlowControl(int iResult)
{
	return ((iResult == -2) || (iResult == -3));
}

void CTraderSpi::OnFrontConnected()
{
	cerr << " --->>> " << "OnFrontConnected" << endl;
	///�û���¼����
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
		// ����Ự����
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		sprintf(ORDER_REF, "%d", atoi(pRspUserLogin->MaxOrderRef));
		///��ȡ��ǰ������
		cerr << " --->>> Current Date = " << pTraderApi->GetTradingDay() << endl;
		TradingDate=pTraderApi->GetTradingDay();
		///Ͷ���߽�����ȷ��
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
		///�����ѯ��Լ
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
			InstrumentInfo << " --->>> Trading IntrumentID��" << pInstrument->InstrumentID << endl;
			InstrumentInfo << " --->>> Trading Intrument Name��" << pInstrument->InstrumentName << endl;
			InstrumentInfo << " --->>> Intrument Tick Size��" << pInstrument->PriceTick << endl;
			InstrumentInfo << " --->>> Instrument Leverage��" << pInstrument->VolumeMultiple << endl;
			InstrumentInfo << " --->>> ExchangeID��" << pInstrument->ExchangeID << endl;
			InstrumentInfo << "-----------------------------------------" << endl;

			//mpAssitant[pInstrument->InstrumentID]->InstrumentTickSize = pInstrument->PriceTick;
			//mpAssitant[pInstrument->InstrumentID]->InstrumentLeverage = pInstrument->VolumeMultiple;
			//mpAssitant[pInstrument->InstrumentID]->InstrumentExchangeID = pInstrument->ExchangeID;

		}
	}
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///�����ѯ�˻�
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
	cerr << " --->>> Static  Balance�� " <<setprecision(2)<<setiosflags(ios::fixed)<< preBalance << endl;
	cerr << " --->>> Current Balance�� " <<setprecision(2)<<setiosflags(ios::fixed)<< current << endl;
	cerr << "-----------------------------------------"<<endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///�����ѯͶ���ֲ߳�
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
			PosiInfo << " --->>> Instrument       �� " << pInvestorPosition->InstrumentID << endl;
			PosiInfo << " --->>> Realized   Profit�� " << pInvestorPosition->CloseProfit << endl;
			PosiInfo << " --->>> Unrealized Profit�� " << pInvestorPosition->PositionProfit << endl;
			PosiInfo << " --->>> Net Position�� " << pInvestorPosition->Position << endl;
			PosiInfo << " --->>> Today Position�� " << pInvestorPosition->TodayPosition << endl;
			string ID = pInvestorPosition->InstrumentID;
			//if (pInvestorPosition->PosiDirection == '2'
			//	&& mpAssitant.find(ID) != mpAssitant.end())
			//{
			//	PosiInfo << " --->>> Direciton��Long " << endl;
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
			//	PosiInfo << " --->>> Direciton��Short " << endl;
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
//����������
//�պ���Ҫ���һ����ȥ�����ⲿ�ֲ���
//Ȼ�������̳����������������н���
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

///����֪ͨ
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

///�ɽ�֪ͨ
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

//�ж��Ƿ��Լ��ĵ���
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

//�ж��Ƿ��Լ��Ľ���
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

//�жϵ����Ƿ����ڽ���״̬�����������������ڽ���״̬1�����ֳɽ����ڶ��У�2�����ֳɽ����ڶ��У�3��û�гɽ�
bool CTraderSpi::IsTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus == THOST_FTDC_OST_PartTradedQueueing)          //���ֳɽ����ڶ���
			||(pOrder->OrderStatus == THOST_FTDC_OST_PartTradedNotQueueing)     //���ֳɽ����ڶ���
			||(pOrder->OrderStatus == THOST_FTDC_OST_NoTradeQueueing)			//��ȫδ�ɽ����ڶ���
			||(pOrder->OrderStatus == THOST_FTDC_OST_NoTradeNotQueueing));      //��ȫδ�ɽ����ڶ���
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
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
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