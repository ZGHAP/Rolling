#pragma once
#pragma once


#include "CData.h"
#include "./include/ThostFtdcUserApiStruct.h"
#include "./include/ThostFtdcTraderApi.h"
#include <Windows.h>

//typedef struct ThreadParam
//{
//	COnshoreArbi *CMMObject;
//	CThostFtdcDepthMarketDataField *DepthDataRtn;
//}ThreadParam;

class CAssitant
{
public:

	CAssitant(const string&);
	~CAssitant();

	void SliceThreadPool(CThostFtdcDepthMarketDataField *DepthData);
	void DeleteLimitOrder(double, string, char, int);
	void ShowLimitOrder();
	void SetInitInstrumentID(string);
	void SetRollInstrumentID(string);

	double GetBidPrice();
	double GetAskPrice();
	double GetEntry();

	void SetInventroyCosting(double,int);
	void SetUnfilledLots(string, int, double);
	void SetPositionStatus(CThostFtdcTradeField *pTrade);
	void SetInitPositionStatus(CThostFtdcTradeField *pTrade);
	void ResetOrderDoneFlag(CThostFtdcOrderField *pOrder);
	void PullRestOrder(CThostFtdcOrderField *pOrder);
	void SetCancelField(CThostFtdcOrderField *pOrder, int OriginVolume);

	static void NTAPI SliceCallBack(PTP_CALLBACK_INSTANCE pInstance, PVOID pvContext);

	//用于控制上期所的平今标志
	string InstrumentExchangeID;
	double InstrumentTickSize;
	int InstrumentLeverage;

	int TotalPullOrderTime;
	int TotalTrades;

	int LongInventory;
	int ShortInventory;
	int YdInventory;

	PositionSet Posi;
	PositionSet InitPosi;

	string InstrumentID;
	string RollingInstrumentID;

	CRITICAL_SECTION csInstrumentOrderRtnLock;
	CRITICAL_SECTION csInstrumentUnfilledLock;

private:

	void CalculatePnL(CThostFtdcDepthMarketDataField *DepthData);
	void PendingOrder(CThostFtdcDepthMarketDataField *DepthData, double, int, char, char);
	void PullOrderBack(double);

	//未成交订单序列
	//价格,订单号,字段
	map<double, map<string, CThostFtdcInputOrderActionField>> mpUnfilledOrder;
	//挂单矩阵
	map<string, int> mpUnfilledOrderLots;
	map<double, int> mpUnfilledOrderSizeForCheck;

	TThostFtdcTimeType Timer;

	string TradingDATE;
	string TradingTIME;

	bool bInitialRunning;
	bool bTickChanged;
	bool bLongOrderFilled;
	bool bShortOrderFilled;
	bool bVolatilityChange;
	bool bLongTest, bShortTest;

	double LongLimitPrice, ShortLimitPrice;
	double LongLimitBackup, ShortLimitBackup;
	double DirectionLongSpread, DirectionShortSpread;
	double VolatilitySpread;

	double InitVolume, InitPrice;

	char LastDeltaABR;

	CRITICAL_SECTION csInstrumentInternalLock;

	CThostFtdcDepthMarketDataField *SprDepthData;
};
