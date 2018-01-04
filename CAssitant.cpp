#include"CAssitant.h"
#include"CTraderSpi.h"
#include <tchar.h>
#include<Windows.h>
#include<algorithm>
#include<cmath>
#include<queue>



extern CTraderSpi *pTraderSpi;
extern CThostFtdcTraderApi* pTraderApi;

extern int iRequestID;

// �Ự����
extern TThostFtdcFrontIDType	FRONT_ID;	   // ǰ�ñ��
extern TThostFtdcSessionIDType	SESSION_ID;	   // �Ự���
extern TThostFtdcOrderRefType	ORDER_REF;	   // ��������
extern TThostFtdcBrokerIDType	BROKER_ID;	   // ���͹�˾����
extern TThostFtdcInvestorIDType INVESTOR_ID;   // Ͷ���ߴ���
extern TThostFtdcPasswordType   PASSWORD;	   // �û�����

//�����߳̿��������
extern vector<int> vcOrderRef;
extern queue<CThostFtdcInputOrderField> qToPushOrderQueue;
extern queue<CThostFtdcInputOrderActionField> qToPullOrderQueue;
extern CRITICAL_SECTION csInsert;
extern CRITICAL_SECTION csCancel;
extern HANDLE hToPushEvent;
extern HANDLE hToPullEvent;

extern HANDLE hCTPo_Event;
extern CRITICAL_SECTION csTT_Instrument_Lock;

extern vector<string> vTT_Instrument;

extern queue<void*> qProtoMessageToSHM;
extern queue<void*> qProtoMessageFromSHM;

extern bool CLEAN_ALL_ORDERS;
extern int PULL_ORDER_LIMIT;

extern string UC;


CAssitant::CAssitant(const string& ID)
{
	memset(Timer, 0, sizeof(Timer));
	InstrumentID = ID;

	bLongOrderFilled = true;
	bShortOrderFilled = true;

	bVolatilityChange = false;

	bInitialRunning = true;
	bTickChanged = true;

	bLongTest = true;
	bShortTest = true;

	mpUnfilledOrder.clear();
	mpUnfilledOrderLots.clear();
	mpUnfilledOrderSizeForCheck.clear();

	InitVolume = 0;
	TotalTrades = 0;

	LongLimitPrice = 0;
	ShortLimitPrice = 0;
	LongLimitBackup = 0;
	ShortLimitBackup = 0;

	DirectionLongSpread = 0;
	DirectionShortSpread = 0;
	VolatilitySpread = 0;

	Posi.mp_PriceMap.clear();
	Posi.NetPosition = 0;
	Posi.TotalProfit = 0;
	Posi.TotalValue = 0;
	Posi.AveragePositionEntry = 0;

	InitPosi.mp_PriceMap.clear();
	InitPosi.NetPosition = 0;
	InitPosi.TotalProfit = 0;
	InitPosi.TotalValue = 0;
	InitPosi.AveragePositionEntry = 0;

	InstrumentTickSize = 0;
	InstrumentLeverage = 0;
	TotalPullOrderTime = 0;

	LongInventory = 0;
	ShortInventory = 0;
	YdInventory = 0;

	InitializeCriticalSection(&csInstrumentInternalLock);
	InitializeCriticalSection(&csInstrumentOrderRtnLock);
	InitializeCriticalSection(&csInstrumentUnfilledLock);
}

CAssitant::~CAssitant()
{
	if (LongLimitPrice != 0)
		PullOrderBack(LongLimitPrice);
	if (ShortLimitPrice != 0)
		PullOrderBack(ShortLimitPrice);

	DeleteCriticalSection(&csInstrumentInternalLock);
	DeleteCriticalSection(&csInstrumentOrderRtnLock);
	DeleteCriticalSection(&csInstrumentUnfilledLock);
}

void CAssitant::SliceThreadPool(CThostFtdcDepthMarketDataField *DepthData)
{
	if (InstrumentID == DepthData->InstrumentID)
	{
		this->SprDepthData = DepthData;
		BOOL rBet = TrySubmitThreadpoolCallback(SliceCallBack, this, NULL);
		if (!rBet)
		{
			cerr << "Can not add to thread pool\n";
			system("PAUSE");
		}
	}
}

void CAssitant::SliceCallBack(PTP_CALLBACK_INSTANCE pInstance, PVOID pvContext)
{
	CAssitant *pThis = (CAssitant*)pvContext;

	EnterCriticalSection(&csTT_Instrument_Lock);

	string tempID = pThis->SprDepthData->InstrumentID;

	//record time stamp
	pThis->TradingDATE = pThis->SprDepthData->TradingDay;
	pThis->TradingTIME = pThis->SprDepthData->UpdateTime;

	pThis->CalculatePnL(pThis->SprDepthData);

	double Bid_Ratio = 0;
	double Ask_Ratio = 0;

	int PositionGap = 0;

	PositionGap = -(pThis->InitPosi.NetPosition + pThis->Posi.NetPosition);

	cerr << pThis->SprDepthData->InstrumentID << endl;
	cerr << pThis->SprDepthData->BidPrice1<< endl;
	cerr << pThis->SprDepthData->AskPrice1 << endl;
	cerr << "ID " << pThis->InitPosi.InstrumentID << " " << pThis->InitPosi.NetPosition << endl;
	cerr << "ID " << pThis->Posi.InstrumentID << " " << pThis->Posi.NetPosition << endl;
	cerr << "Need " << PositionGap << endl;


	//Production Trading Environment
	char CloseOffSetFlag;
	if (pThis->InstrumentExchangeID == "SHFE")
		CloseOffSetFlag = THOST_FTDC_OF_CloseToday;
	else
		CloseOffSetFlag = THOST_FTDC_OF_Close;

	if (PositionGap > 0)
	{
		if (pThis->bLongOrderFilled)
		{
			if (pThis->ShortInventory > 0)
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->AskPrice1, min(abs(PositionGap), pThis->ShortInventory), THOST_FTDC_OF_CloseToday, THOST_FTDC_D_Buy);
				pThis->bLongOrderFilled = false;
			}
			else if (pThis->YdInventory < 0)
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->AskPrice1, min(abs(PositionGap), abs(pThis->YdInventory)), THOST_FTDC_OF_Close, THOST_FTDC_D_Buy);
				pThis->bLongOrderFilled = false;
			}
			else
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->AskPrice1, abs(PositionGap), THOST_FTDC_OF_Open, THOST_FTDC_D_Buy);
				pThis->bLongOrderFilled = false;
			}

			//pThis->bLongTest = false;
		}
	}
	else if (PositionGap < 0)
	{
		if (pThis->bShortOrderFilled)
		{
			if (pThis->LongInventory > 0)
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->BidPrice1, min(abs(PositionGap), pThis->LongInventory), THOST_FTDC_OF_CloseToday, THOST_FTDC_D_Sell);
				pThis->bShortOrderFilled = false;
			}
			else if (pThis->YdInventory > 0)
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->BidPrice1, min(abs(PositionGap), abs(pThis->YdInventory)), THOST_FTDC_OF_Close, THOST_FTDC_D_Sell);
				pThis->bShortOrderFilled = false;
			}
			else
			{
				pThis->PendingOrder(pThis->SprDepthData, pThis->SprDepthData->BidPrice1, abs(PositionGap), THOST_FTDC_OF_Open, THOST_FTDC_D_Sell);
				pThis->bShortOrderFilled = false;
			}

			//pThis->bShortTest = false;
		}
	}

	LeaveCriticalSection(&csTT_Instrument_Lock);
}

void CAssitant::DeleteLimitOrder(double price, string OrderSysID, char Dir, int _OrderSize)
{
	EnterCriticalSection(&csInstrumentUnfilledLock);

	if (!mpUnfilledOrder.empty())
	{
		if (mpUnfilledOrder.find(price) != mpUnfilledOrder.end())
		{
			if (mpUnfilledOrder[price].find(OrderSysID) != mpUnfilledOrder[price].end()
				&& !mpUnfilledOrder[price].empty())
			{
				mpUnfilledOrder[price].erase(mpUnfilledOrder[price].find(OrderSysID));
				//����ɾ���ڵ��iterator��λ���ˣ������
				//û��Ҫɾ���۸�ڵ㣬��Ϊ����ʹ�õ�
				//if (mpUnfilledOrder[price].empty())
				//	mpUnfilledOrder.erase(mpUnfilledOrder.find(price));
			}
		}
	}

	if (!mpUnfilledOrderLots.empty())
	{
		if (mpUnfilledOrderLots.find(OrderSysID) != mpUnfilledOrderLots.end())
			mpUnfilledOrderLots.erase(mpUnfilledOrderLots.find(OrderSysID));
	}

	if (!mpUnfilledOrderSizeForCheck.empty())
	{
		mpUnfilledOrderSizeForCheck[price] -= _OrderSize;
	}

	LeaveCriticalSection(&csInstrumentUnfilledLock);

	if (Dir == THOST_FTDC_D_Buy)
	{
		LongLimitPrice = 0;
	}
	else if (Dir == THOST_FTDC_D_Sell)
	{
		ShortLimitPrice = 0;
	}
}

double CAssitant::GetEntry()
{
	return Posi.AveragePositionEntry;
}

double CAssitant::GetBidPrice()
{
	return SprDepthData->BidPrice1;
}

double CAssitant::GetAskPrice()
{
	return SprDepthData->AskPrice1;
}

void CAssitant::SetInventroyCosting(double inventory_cost, int invenroty_lots)
{
	int tempTotalLots = 0;

	if (Posi.mp_PriceMap.find(inventory_cost) == Posi.mp_PriceMap.end())
		Posi.mp_PriceMap[inventory_cost] = invenroty_lots;
	else
		Posi.mp_PriceMap[inventory_cost] += invenroty_lots;

	for (map<double, int>::iterator mpit = Posi.mp_PriceMap.begin(); mpit != Posi.mp_PriceMap.end(); mpit++)
	{
		//cerr <<Posi.InstrumentID << " " << mpit->first << " " << mpit->second<<endl;
		tempTotalLots += mpit->second;
	}
	Posi.NetPosition = tempTotalLots;

	if (Posi.mp_PositionDistribution.find(inventory_cost) == Posi.mp_PositionDistribution.end())
		Posi.mp_PositionDistribution[inventory_cost] = invenroty_lots;
	else
		Posi.mp_PositionDistribution[inventory_cost] += invenroty_lots;

}

void CAssitant::SetUnfilledLots(string sysID, int TotalVolume,double price)
{
	mpUnfilledOrderLots[sysID] = TotalVolume;
	mpUnfilledOrderSizeForCheck[price] = TotalVolume;
}

void CAssitant::ShowLimitOrder()
{
	EnterCriticalSection(&csInstrumentUnfilledLock);

	if (!mpUnfilledOrder.empty())
	{
		map<double, map<string, CThostFtdcInputOrderActionField>>::iterator mpit;
		for (mpit = mpUnfilledOrder.begin();
			mpit != mpUnfilledOrder.end();
			mpit++)
		{
			if (mpit->second.size() != 0)
				cerr << " " << mpUnfilledOrderLots[mpit->second.begin()->first] << " orders @ " << mpit->first << endl;
		}
	}
	cerr << " TickSize :    " << InstrumentTickSize << endl << endl;

	LeaveCriticalSection(&csInstrumentUnfilledLock);
}

void CAssitant::SetInitInstrumentID(string ID)
{
	InstrumentID = ID;
}

void CAssitant::SetRollInstrumentID(string ID)
{
	RollingInstrumentID = ID;
}

void CAssitant::PendingOrder(CThostFtdcDepthMarketDataField *DepthData, double price, int lots, char offlag, char direction)
{
	CThostFtdcInputOrderField req;
	memset(&req, 0, sizeof(req));

	///���͹�˾����
	strcpy(req.BrokerID, BROKER_ID);
	///Ͷ���ߴ���
	strcpy(req.InvestorID, INVESTOR_ID);
	///��Լ���� 
	strcpy(req.InstrumentID, DepthData->InstrumentID);
	///��������
	strcpy(req.OrderRef, ORDER_REF);
	///�û�����
	//	TThostFtdcUserIDType	UserID;
	///�����۸�����: �޼�
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;

	///��������: 
	req.Direction = direction;

	///��Ͽ�ƽ��־: ����
	req.CombOffsetFlag[0] = offlag;

	///���Ͷ���ױ���־
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///�۸�
	req.LimitPrice = price;
	///����
	req.VolumeTotalOriginal = lots;
	///��Ч������: ������Ч
	req.TimeCondition = THOST_FTDC_TC_GFD;
	///GTD����
	//	TThostFtdcDateType	GTDDate;
	///�ɽ�������: �κ�����
	req.VolumeCondition = THOST_FTDC_VC_AV;
	///��С�ɽ���: 1
	req.MinVolume = 1;
	///��������: ����
	req.ContingentCondition = THOST_FTDC_CC_Immediately;
	///ֹ���
	//	TThostFtdcPriceType	StopPrice;
	///ǿƽԭ��: ��ǿƽ
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///�Զ������־: ��
	req.IsAutoSuspend = 0;
	///ҵ��Ԫ
	//	TThostFtdcBusinessUnitType	BusinessUnit;
	///������
	//	TThostFtdcRequestIDType	RequestID;
	///�û�ǿ����־: ��
	req.UserForceClose = 0;

	if (direction == THOST_FTDC_D_Buy)
		LongLimitPrice = price;
	else if (direction == THOST_FTDC_D_Sell)
		ShortLimitPrice = price;

	EnterCriticalSection(&csInsert);

	qToPushOrderQueue.push(req);

	LeaveCriticalSection(&csInsert);

	SetEvent(hToPushEvent);
}

void CAssitant::PullOrderBack(double price)
{
	EnterCriticalSection(&csInstrumentUnfilledLock);
	if (mpUnfilledOrder.find(price) != mpUnfilledOrder.end()
		&& !mpUnfilledOrder[price].empty())
	{
		for (map<string, CThostFtdcInputOrderActionField>::iterator mp_it = mpUnfilledOrder[price].begin();
			mp_it != mpUnfilledOrder[price].end();)
		{
			EnterCriticalSection(&csCancel);

			qToPullOrderQueue.push(mp_it->second);
			mpUnfilledOrder[price].erase(mp_it++);

			LeaveCriticalSection(&csCancel);
		}
		//mpUnfilledOrder[price].clear();
		TotalPullOrderTime++;
	}
	SetEvent(hToPullEvent);
	LeaveCriticalSection(&csInstrumentUnfilledLock);
}

void CAssitant::SetPositionStatus(CThostFtdcTradeField * pTrade)
{
	//Output
	string OffSetFlag, Direction, Date, OrderSysID;
	Date = pTrade->TradeDate;
	string sz_path = "TradeLog_" + Posi.InstrumentID + "_" + Date + ".csv";
	ofstream TradeLog(sz_path, ios::app);

	//����ͷ�����еķ�ʽ����ɱ�����110��1����120��1�����������У�ƽ��ʱ���в��Ը�ͷ��ɱ�����ʵ��ӯ����
	//ͷ����۲���������͵ķ�ʽ���㡣
	int tempLots = 0;
	int tempTotalLots = 0;
	Posi.InstrumentID = pTrade->InstrumentID;
	OrderSysID = pTrade->OrderSysID;
	if (pTrade->Direction == '0')
	{
		tempLots = pTrade->Volume;
		//LongLimitPrice = 0;
	}
	else
	{
		tempLots = -1 * pTrade->Volume;
		//ShortLimitPrice = 0;
	}

	if (Posi.mp_PriceMap.find(pTrade->Price) == Posi.mp_PriceMap.end())
		Posi.mp_PriceMap[pTrade->Price] = tempLots;
	else
		Posi.mp_PriceMap[pTrade->Price] = Posi.mp_PriceMap[pTrade->Price] + tempLots;

	for (map<double, int>::iterator mpit = Posi.mp_PriceMap.begin(); mpit != Posi.mp_PriceMap.end(); mpit++)
	{
		//cerr <<Posi.InstrumentID << " " << mpit->first << " " << mpit->second<<endl;
		tempTotalLots += mpit->second;
	}
	Posi.NetPosition = tempTotalLots;

	if (pTrade->OffsetFlag == THOST_FTDC_OF_Open)
	{
		OffSetFlag = "OPEN";

		//DiretionType is a char '0'=buy '1'=sell
		//OffsetFlag is a char '0'=open '1'=close
		if (pTrade->Direction == '0')
		{
			Direction = "LONG";
			LongInventory += pTrade->Volume;
		}
		else
		{
			Direction = "SHORT";
			ShortInventory += pTrade->Volume;
		}

		if (Posi.mp_PositionDistribution.find(pTrade->Price) == Posi.mp_PositionDistribution.end())
			Posi.mp_PositionDistribution[pTrade->Price] = tempLots;
		else
			Posi.mp_PositionDistribution[pTrade->Price] += tempLots;

		double tempTotalPosiValue = 0;
		double tempTotolLots = 0;
		for (map<double, int>::iterator mpit = Posi.mp_PositionDistribution.begin(); mpit != Posi.mp_PositionDistribution.end(); mpit++)
		{
			tempTotalPosiValue += mpit->first*mpit->second;
			tempTotolLots += mpit->second;
		}
		Posi.AveragePositionEntry = tempTotalPosiValue / tempTotolLots;

		if (pTrade->Volume != 0)
		{
			TotalTrades += pTrade->Volume;
			if (TradeLog.is_open())
				TradeLog << pTrade->InstrumentID << "," << OffSetFlag << "," << Direction << "," << pTrade->TradeTime << "," << pTrade->Price << "," << pTrade->Volume << "," << TotalTrades << endl;
			TradeLog.close();
		}
	}
	else
	{
		OffSetFlag = "CLOSE";

		//DiretionType is a char '0'=buy '1'=sell
		//OffsetFlag is a char '0'=open '1'=close
		if (pTrade->Direction == '0')
		{
			Direction = "LONG";
			ShortInventory -= pTrade->Volume;
			if (ShortInventory < 0)
			{
				ShortInventory = 0;
				YdInventory += pTrade->Volume;
			}
		}
		else
		{
			Direction = "SHORT";
			LongInventory -= pTrade->Volume;
			if (LongInventory < 0)
			{
				LongInventory = 0;
				YdInventory -= pTrade->Volume;
			}
		}

		double tempTotolLots = 0;
		for (map<double, int>::iterator mpit = Posi.mp_PositionDistribution.begin(); mpit != Posi.mp_PositionDistribution.end(); mpit++)
		{
			if (mpit->second > 0)
				mpit->second -= pTrade->Volume;
			else if(mpit->second < 0)
				mpit->second += pTrade->Volume;
			tempTotolLots += mpit->second;
		}

		if (tempTotolLots == 0)
		{
			Posi.mp_PositionDistribution.clear();
			Posi.AveragePositionEntry = 0;
		}

		if (pTrade->Volume != 0)
		{
			if (TradeLog.is_open())
				TradeLog << pTrade->InstrumentID << "," << OffSetFlag << "," << Direction << "," << pTrade->TradeTime << "," << pTrade->Price << "," << pTrade->Volume << "," << "," << Posi.TotalProfit << endl << endl;
			TradeLog.close();
		}

	}
}

void CAssitant::SetInitPositionStatus(CThostFtdcTradeField * pTrade)
{
	//Output
	string OffSetFlag, Direction, Date, OrderSysID;

	//����ͷ�����еķ�ʽ����ɱ�����110��1����120��1�����������У�ƽ��ʱ���в��Ը�ͷ��ɱ�����ʵ��ӯ����
	//ͷ����۲���������͵ķ�ʽ���㡣
	int tempLots = 0;
	int tempTotalLots = 0;
	InitPosi.InstrumentID = pTrade->InstrumentID;
	OrderSysID = pTrade->OrderSysID;
	if (pTrade->Direction == '0'){
		tempLots = pTrade->Volume;
	}
	else{
		tempLots = -1 * pTrade->Volume;
	}

	if (InitPosi.mp_PriceMap.find(pTrade->Price) == InitPosi.mp_PriceMap.end())
		InitPosi.mp_PriceMap[pTrade->Price] = tempLots;
	else
		InitPosi.mp_PriceMap[pTrade->Price] = InitPosi.mp_PriceMap[pTrade->Price] + tempLots;

	for (map<double, int>::iterator mpit = InitPosi.mp_PriceMap.begin(); mpit != InitPosi.mp_PriceMap.end(); mpit++)
	{
		tempTotalLots += mpit->second;
	}
	InitPosi.NetPosition = tempTotalLots;
}

void CAssitant::ResetOrderDoneFlag(CThostFtdcOrderField * pOrder)
{
	EnterCriticalSection(&csInstrumentOrderRtnLock);

	if (InstrumentID == pOrder->InstrumentID)
	{
		if (pOrder->Direction == '1')
			bShortOrderFilled = true;
		if (pOrder->Direction == '0')
			bLongOrderFilled = true;
	}

	LeaveCriticalSection(&csInstrumentOrderRtnLock);
}

void CAssitant::CalculatePnL(CThostFtdcDepthMarketDataField *DepthData)
{
	Posi.InstrumentID = DepthData->InstrumentID;
	Posi.TotalValue = 0;
	double total_lots = 0;
	if (Posi.mp_PriceMap.empty())
		Posi.TotalValue = 0;
	else
	{
		for (map<double, int>::iterator mpit = Posi.mp_PriceMap.begin(); mpit != Posi.mp_PriceMap.end(); mpit++)
		{
			//cerr <<Posi.InstrumentID << " " << mpit->first << " " << mpit->second<<endl;
			Posi.TotalValue = Posi.TotalValue + mpit->first*mpit->second*InstrumentLeverage;
			total_lots += mpit->second;
		}
		Posi.TotalProfit = (DepthData->LastPrice*Posi.NetPosition - Posi.TotalValue) / InstrumentTickSize;
		Posi.CostLine = Posi.TotalValue / total_lots;
	}
	//cerr << Posi.TotalProfit << " " << Posi.CostLine <<" "<< Posi.TotalValue<< endl;
}

void CAssitant::SetCancelField(CThostFtdcOrderField * pOrder, int OriginVolume)
{
	CThostFtdcInputOrderActionField req;
	//char tmpDirection = pOrder->Direction;
	double OrderPrice;
	string sysID;

	EnterCriticalSection(&csInstrumentUnfilledLock);

	memset(&req, 0, sizeof(req));
	///���͹�˾����
	strcpy(req.BrokerID, pOrder->BrokerID);
	///Ͷ���ߴ���
	strcpy(req.InvestorID, pOrder->InvestorID);
	///��������
	strcpy(req.OrderRef, pOrder->OrderRef);
	///����Ʒ��
	strcpy(req.InstrumentID, pOrder->InstrumentID);

	///����������
	strcpy(req.ExchangeID, pOrder->ExchangeID);
	///�������
	strcpy(req.OrderSysID, pOrder->OrderSysID);
	///������־
	req.ActionFlag = THOST_FTDC_AF_Delete;
	///�ҵ��۸�
	req.LimitPrice = pOrder->LimitPrice;

	OrderPrice = pOrder->LimitPrice;
	sysID = pOrder->OrderSysID;

	mpUnfilledOrder[OrderPrice][sysID] = req;
	mpUnfilledOrderLots[sysID] = OriginVolume;
	if (!mpUnfilledOrderSizeForCheck.empty())
	{
		if (mpUnfilledOrderSizeForCheck.find(OrderPrice) != mpUnfilledOrderSizeForCheck.end())
			mpUnfilledOrderSizeForCheck[OrderPrice] += OriginVolume;
		else
			mpUnfilledOrderSizeForCheck[OrderPrice] = OriginVolume;
	}

	LeaveCriticalSection(&csInstrumentUnfilledLock);
}

void CAssitant::PullRestOrder(CThostFtdcOrderField *pOrder)
{
	EnterCriticalSection(&csInstrumentInternalLock);

	if (pOrder->Direction == THOST_FTDC_D_Buy)
		PullOrderBack(LongLimitPrice);
	else if (pOrder->Direction == THOST_FTDC_D_Sell)
		PullOrderBack(ShortLimitPrice);

	LeaveCriticalSection(&csInstrumentInternalLock);
}