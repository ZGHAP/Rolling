#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include "./include/ThostFtdcUserApiStruct.h"

#pragma warning(disable : 4996)

using namespace std;

typedef struct
{
	string InstrumentID;
	map<double, int> mp_PriceMap;
	map<double, int> mp_PositionDistribution;
	double TotalProfit;
	double TotalValue;
	double CostLine;
	double AveragePositionEntry;
	int NetPosition;
}PositionSet;

typedef struct depth_struct
{
	double bid_price;
	double ask_price;
	bool operator<(const depth_struct &depth)const
	{
		if (bid_price < depth.bid_price)
			return true;
		else
			return false;
	}
	bool operator>(const depth_struct &depth)const
	{
		if (ask_price > depth.ask_price)
			return true;
		else
			return false;
	}
	bool operator==(const depth_struct &depth)const
	{
		if (ask_price == depth.ask_price && bid_price == depth.bid_price)
			return true;
		else
			return false;
	}
	bool operator!=(const depth_struct &depth)const
	{
		if (ask_price != depth.ask_price || bid_price != depth.bid_price)
			return true;
		else
			return false;
	}
	void operator=(const depth_struct &depth)
	{
		bid_price = depth.bid_price;
		ask_price = depth.ask_price;
	}
	double operator-(const depth_struct &depth)
	{
		double bid_gap, ask_gap;
		bid_gap = bid_price - depth.bid_price;
		ask_gap = ask_price - depth.ask_price;
		return (bid_gap + ask_gap) / 2;
	}
}depth_struct;

typedef struct depth_detail
{
	double InitABR;
	double LastAccuDelta;

	int LastMvt;
	int LastDirSym;
	//Last CrossTime
	int iLCT;
	int iLstRuns;
	int Lst_Max_Delta_Runs;
	double dLABS_AccuDelta, dLMax_AccuDelta;


}depth_detail;

typedef struct Data_Block
{
	double open, high, low, close;
	double TickCounts;
	double last_bid, last_ask;
	int HorizonCounts;

	bool bEnableTrading;
	bool bEnableStop;
	bool bHighVolatility;

	double dTickCounts_mean;
	double dTickCounts_stdev;

	vector<double> vc_Ticks;
	string ID;
}Data_Block;

typedef struct TT_Instrument_Depth
{
	int Bid_Size, Ask_Size;
	int Investory_Net;
	double Bid_Price, Ask_Price;
	double Inventory_Costing, PositionValue;
	string TimeStamp;
	string InstrumentID;
	bool MarketOrderFlag;
}TT_Instrument_Depth;
