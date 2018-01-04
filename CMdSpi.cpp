#include "CMdSpi.h"
#include "CTraderSpi.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <queue>

#include "CAssitant.h"

#include "CTP_TT_Msg.pb.h"

using namespace std;

#pragma warning(disable : 4996)

// USER_API参数
extern CThostFtdcMdApi* pMdApi;
extern CTraderSpi *pTraderSpi;

//全局变量
extern HANDLE hRtnDepMktOutPut;
extern queue<void*> qProtoMessageToSHM;
extern HANDLE hCTP_Out_Semaphore;
extern HANDLE hCTP_Out_Mutex;

// 配置参数
extern char M_FRONT_ADDR[];		
extern TThostFtdcBrokerIDType	BROKER_ID;
extern TThostFtdcInvestorIDType INVESTOR_ID;
extern TThostFtdcPasswordType	PASSWORD;

//extern ThreadParam srThreadParam;
extern char **ppInstrumentID;	
extern int iInstrumentID;

// 请求编号
extern int iRequestID;

extern map<string, CAssitant*> mpAssitant;
extern map<string, string> mpRollingContract;

void CMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo,
		int nRequestID, bool bIsLast)
{
	cerr << " --->>> "<< "OnRspError" << endl;
	IsErrorRspInfo(pRspInfo);
}

void CMdSpi::OnFrontDisconnected(int nReason)
{
	cerr << " --->>> " << "OnFrontDisconnected" << endl;
	cerr << " --->>> Reason = " << nReason << endl;

}
		
void CMdSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << " --->>> " << "OnHeartBeatWarning" << endl;
	cerr << " --->>> nTimerLapse = " << nTimeLapse << endl;
}

void CMdSpi::OnFrontConnected()
{
	ReqUserLogin();
}

void CMdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	int iResult = pMdApi->ReqUserLogin(&req, ++iRequestID);
	cerr<<" --->>> Data login BROKER_ID:"<<req.BrokerID<<endl;
	cerr<<" --->>> Data login INVESTOR_ID:"<<req.UserID<<endl;

}

void CMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << " --->>> " << "OnRspUserLogin" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// 请求订阅行情
		cerr<<" --->>> Live Data Login "<<endl;
		cerr<<pRspUserLogin->BrokerID<<endl;
		SubscribeMarketData();	
	}
}

void CMdSpi::SubscribeMarketData()
{
	int iResult = pMdApi->SubscribeMarketData(ppInstrumentID, iInstrumentID);
	cerr << " --->>> Subscribe Market Data " << ((iResult == 0) ? "  " : "Failed") << endl;
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "OnRspSubMarketData" << endl;
}

void CMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "OnRspUnSubMarketData" << endl;
}

void CMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
	ULONGLONG StartTimer, EndTimer;
	StartTimer = GetTickCount64();

	CThostFtdcDepthMarketDataField *pDepth = new CThostFtdcDepthMarketDataField;
	strcpy(pDepth->InstrumentID, pDepthMarketData->InstrumentID);
	strcpy(pDepth->TradingDay, pDepthMarketData->TradingDay);
	strcpy(pDepth->UpdateTime, pDepthMarketData->UpdateTime);
	strcpy(pDepth->ExchangeID, pDepthMarketData->ExchangeID);
	pDepth->UpdateMillisec = pDepthMarketData->UpdateMillisec;
	pDepth->BidPrice1 = pDepthMarketData->BidPrice1;
	pDepth->AskPrice1 = pDepthMarketData->AskPrice1;
	pDepth->BidVolume1 = pDepthMarketData->BidVolume1;
	pDepth->AskVolume1 = pDepthMarketData->AskVolume1;
	pDepth->LastPrice= pDepthMarketData->LastPrice;
	pDepth->Volume = pDepthMarketData->Volume;

	if (mpAssitant.find(pDepthMarketData->InstrumentID) != mpAssitant.end())
		mpAssitant[pDepthMarketData->InstrumentID]->SliceThreadPool(pDepth);
	
	EndTimer = GetTickCount64();
	//cerr << " time used " << EndTimer - StartTimer << endl;
	//SetEvent(hRtnDepMktOutPut);
}

bool CMdSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		cerr << " --->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}



