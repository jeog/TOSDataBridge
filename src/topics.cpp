/* 
Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include "tos_databridge.h"

const TOS_Topics::topic_map_entry_type gtmArr[] = {
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::NULL_TOPIC, " "),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::HIGH52, "52HIGH"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LOW52, "52LOW"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ADX, "ADX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ADXCrossover, "ADXCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ADXR, "ADXR"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::ASK, "ASK"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::ASKX, "ASKX"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::ASK_SIZE, "ASK_SIZE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ATRWilder, "ATRWilder"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::AX, "AX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AbandonedBaby, "AbandonedBaby"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AccelerationBands, "AccelerationBands"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AccelerationDecelerationOsc, "AccelerationDecelerationOsc"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AdvanceBlock, "AdvanceBlock"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Alpha2, "Alpha2"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AlphaJensen, "AlphaJensen"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AroonIndicator, "AroonIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AroonOscillator, "AroonOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AverageTrueRange, "AverageTrueRange"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::AwesomeOscillator, "AwesomeOscillator"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BACK_EX_MOVE, "BACK_EX_MOVE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BACK_VOL, "BACK_VOL"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BA_SIZE, "BA_SIZE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BETA, "BETA"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BID, "BID"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BIDX, "BIDX"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BID_SIZE, "BID_SIZE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::BX, "BX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BalanceOfMarketPower, "BalanceOfMarketPower"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BeltHold, "BeltHold"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Beta, "Beta"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Beta2, "Beta2"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BollingerBandsCrossover, "BollingerBandsCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BollingerBandsEMA, "BollingerBandsEMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BollingerBandsSMA, "BollingerBandsSMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BollingerBandwidth, "BollingerBandwidth"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::BollingerPercentB, "BollingerPercentB"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Breakaway, "Breakaway"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::CALL_VOLUME_INDEX, "CALL_VOLUME_INDEX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CCI, "CCI"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CCIAverage, "CCIAverage"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::CLOSE, "CLOSE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::COVERED_RETURN, "COVERED_RETURN"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CSI, "CSI"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM1, "CUSTOM1"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM10, "CUSTOM10"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM11, "CUSTOM11"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM12, "CUSTOM12"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM13, "CUSTOM13"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM14, "CUSTOM14"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM15, "CUSTOM15"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM16, "CUSTOM16"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM17, "CUSTOM17"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM18, "CUSTOM18"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM19, "CUSTOM19"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM2, "CUSTOM2"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM3, "CUSTOM3"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM4, "CUSTOM4"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM5, "CUSTOM5"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM6, "CUSTOM6"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM7, "CUSTOM7"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM8, "CUSTOM8"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CUSTOM9, "CUSTOM9"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ChaikinMoneyFlow, "ChaikinMoneyFlow"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ChaikinVolatility, "ChaikinVolatility"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ChandeMomentumOscillator, "ChandeMomentumOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::CloseLocationValue, "CloseLocationValue"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ConcealingBabySwallow, "ConcealingBabySwallow"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Correlation, "Correlation"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::DELTA, "DELTA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DEMA, "DEMA"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::DESCRIPTION, "DESCRIPTION"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DIMinus, "DIMinus"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DIPlus, "DIPlus"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::DIV, "DIV"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::DIV_FREQ, "DIV_FREQ"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DMA, "DMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DMI, "DMI"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DMI_Oscillator, "DMI_Oscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DMI_ReversalAlerts, "DMI_ReversalAlerts"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DMI_StochasticExtreme, "DMI_StochasticExtreme"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::DT, "DT"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DailyHighLow, "DailyHighLow"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DailyOpen, "DailyOpen"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DailySMA, "DailySMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DarkCloudCover, "DarkCloudCover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Deliberation, "Deliberation"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DetrendedPriceOsc, "DetrendedPriceOsc"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DisparityIndex, "DisparityIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DisplacedEMA, "DisplacedEMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Displacer, "Displacer"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Doji, "Doji"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DoubleSmoothedStochastics, "DoubleSmoothedStochastics"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DownsideGapThreeMethods, "DownsideGapThreeMethods"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::DownsideTasukiGap, "DownsideTasukiGap"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::EMAEnvelope, "EMAEnvelope"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EPS, "EPS"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EXCHANGE, "EXCHANGE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EXPIRATION, "EXPIRATION"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EXTRINSIC, "EXTRINSIC"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EX_DIV_DATE, "EX_DIV_DATE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::EX_MOVE_DIFF, "EX_MOVE_DIFF"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::EaseOfMovement, "EaseOfMovement"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::EhlersDistantCoefficientFilter, "EhlersDistantCoefficientFilter"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ElderImpulse, "ElderImpulse"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ElderRay, "ElderRay"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ElderRayBearPower, "ElderRayBearPower"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ElderRayBullPower, "ElderRayBullPower"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ElliotOscillator, "ElliotOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Engulfing, "Engulfing"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ErgodicOsc, "ErgodicOsc"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::EveningDojiStar, "EveningDojiStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::EveningStar, "EveningStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FAKE_THINKSCRIPT_COLUMN, "FAKE_THINKSCRIPT_COLUMN"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::FRONT_EX_MOVE, "FRONT_EX_MOVE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::FRONT_VOL, "FRONT_VOL"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FW_CCI_Advanced, "FW_CCI_Advanced"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FW_CCI_Basic, "FW_CCI_Basic"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FW_MMG, "FW_MMG"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::FX_PAIR, "FX_PAIR"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FallingThreeMethods, "FallingThreeMethods"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::FastBeta, "FastBeta"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ForceIndex, "ForceIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ForecastOscillator, "ForecastOscillator"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::GAMMA, "GAMMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::GatorOscillator, "GatorOscillator"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::HIGH, "HIGH"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::HTB_ETB, "HTB_ETB"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Hammer, "Hammer"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HangingMan, "HangingMan"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Harami, "Harami"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HaramiCross, "HaramiCross"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HighPriceGappingPlay, "HighPriceGappingPlay"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HistoricalVolatility, "HistoricalVolatility"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HomingPigeon, "HomingPigeon"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::HullMovingAvg, "HullMovingAvg"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::IFT_StochOsc, "IFT_StochOsc"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::IMPL_VOL, "IMPL_VOL"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::INTRINSIC, "INTRINSIC"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Ichimoku, "Ichimoku"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::IdenticalThreeCrows, "IdenticalThreeCrows"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ImpVolatility, "ImpVolatility"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::InNeck, "InNeck"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Inertia, "Inertia"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::IntradayMomentumIndex, "IntradayMomentumIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::InvertedHammer, "InvertedHammer"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::KeltnerChannels, "KeltnerChannels"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Kicking, "Kicking"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LAST, "LAST"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LASTX, "LASTX"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LAST_SIZE, "LAST_SIZE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LBR_PaintBars, "LBR_PaintBars"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LBR_SmartADX, "LBR_SmartADX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LBR_ThreeTenOscillator, "LBR_ThreeTenOscillator"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LOW, "LOW"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::LX, "LX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LinearRegCurve, "LinearRegCurve"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LinearRegrReversal, "LinearRegrReversal"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LinearRegressionSlope, "LinearRegressionSlope"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LongLeggedDoji, "LongLeggedDoji"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LookUpHighest, "LookUpHighest"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LookUpLowest, "LookUpLowest"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::LowPriceGappingPlay, "LowPriceGappingPlay"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MACD, "MACD"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MACDHistogram, "MACDHistogram"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MACDHistogramCrossover, "MACDHistogramCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MACDTwoLines, "MACDTwoLines"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MACDWithPrices, "MACDWithPrices"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MARK, "MARK"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MARKET_CAP, "MARKET_CAP"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MARK_CHANGE, "MARK_CHANGE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MARK_PERCENT_CHANGE, "MARK_PERCENT_CHANGE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MAX_COVERED_RETURN, "MAX_COVERED_RETURN"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MESASineWave, "MESASineWave"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MRKT_MKR_MOVE, "MRKT_MKR_MOVE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::MT_NEWS, "MT_NEWS"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MarkerIndicator, "MarkerIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MarketForecast, "MarketForecast"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Marubozu, "Marubozu"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MassIndex, "MassIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MatHold, "MatHold"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MatchingLow, "MatchingLow"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MedianAverage, "MedianAverage"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MedianPrice, "MedianPrice"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MeetingLines, "MeetingLines"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MktFacilitationIdx, "MktFacilitationIdx"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Momentum, "Momentum"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MomentumCrossover, "MomentumCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MomentumPercent, "MomentumPercent"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MomentumSMA, "MomentumSMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MoneyFlowIndex, "MoneyFlowIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MoneyFlowIndexCrossover, "MoneyFlowIndexCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MorningDojiStar, "MorningDojiStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MorningStar, "MorningStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovAvgExpRibbon, "MovAvgExpRibbon"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovAvgExponential, "MovAvgExponential"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovAvgTriangular, "MovAvgTriangular"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovAvgTwoLines, "MovAvgTwoLines"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovAvgWeighted, "MovAvgWeighted"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::MovingAvgCrossover, "MovingAvgCrossover"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::NET_CHANGE, "NET_CHANGE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Next3rdFriday, "Next3rdFriday"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::OPEN, "OPEN"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::OPEN_INT, "OPEN_INT"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::OPTION_VOLUME_INDEX, "OPTION_VOLUME_INDEX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OnNeck, "OnNeck"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OpenInterest, "OpenInterest"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OptionDelta, "OptionDelta"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OptionGamma, "OptionGamma"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OptionRho, "OptionRho"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OptionTheta, "OptionTheta"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::OptionVega, "OptionVega"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::PE, "PE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::PERCENT_CHANGE, "PERCENT_CHANGE"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PROB_OF_EXPIRING, "PROB_OF_EXPIRING"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PROB_OF_TOUCHING, "PROB_OF_TOUCHING"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PROB_OTM, "PROB_OTM"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::PUT_CALL_RATIO, "PUT_CALL_RATIO"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::PUT_VOLUME_INDEX, "PUT_VOLUME_INDEX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PairCorrelation, "PairCorrelation"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PairRatio, "PairRatio"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PercentChg, "PercentChg"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PercentR, "PercentR"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PersonsPivots, "PersonsPivots"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PiercingLine, "PiercingLine"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PolarizedFractalEfficiency, "PolarizedFractalEfficiency"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PolychromMtm, "PolychromMtm"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceActionIndicator, "PriceActionIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceAverageCrossover, "PriceAverageCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceChannel, "PriceChannel"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceOsc, "PriceOsc"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceVolumeRank, "PriceVolumeRank"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::PriceZoneOscillator, "PriceZoneOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ProjectionBands, "ProjectionBands"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ProjectionOscillator, "ProjectionOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::QStick, "QStick"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::RHO, "RHO"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ROC, "ROC"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ROR, "ROR"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RSIWilder, "RSIWilder"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RSIWilderCrossover, "RSIWilderCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RSI_EMA, "RSI_EMA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RSquared, "RSquared"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RandomWalkIndex, "RandomWalkIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RangeBands, "RangeBands"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RangeIndicator, "RangeIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RateOfChange, "RateOfChange"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RateOfChangeCrossover, "RateOfChangeCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RelativeMomentumIndex, "RelativeMomentumIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RelativeRangeIndex, "RelativeRangeIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RelativeVolatilityIndex, "RelativeVolatilityIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ReverseEngineeringMACD, "ReverseEngineeringMACD"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RibbonStudy, "RibbonStudy"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::RisingThreeMethods, "RisingThreeMethods"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::SHARES, "SHARES"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SMAEnvelope, "SMAEnvelope"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::STARCBands, "STARCBands"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::STRENGTH_METER, "STRENGTH_METER"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::STRIKE, "STRIKE"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::SYMBOL, "SYMBOL"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SectorRotationModel, "SectorRotationModel"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SentimentZoneOscillator, "SentimentZoneOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SeparatingLines, "SeparatingLines"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ShootingStar, "ShootingStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SideBySideWhiteLines, "SideBySideWhiteLines"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SimpleMovingAvg, "SimpleMovingAvg"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Spearman, "Spearman"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SpectrumBars, "SpectrumBars"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Spreads, "Spreads"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StandardDeviation, "StandardDeviation"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StandardError, "StandardError"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StandardErrorBands, "StandardErrorBands"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StickSandwich, "StickSandwich"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochRSI, "StochRSI"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochasticCrossover, "StochasticCrossover"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochasticFast, "StochasticFast"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochasticFull, "StochasticFull"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochasticMomentumIndex, "StochasticMomentumIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::StochasticSlow, "StochasticSlow"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SwamiIntradayImpulse, "SwamiIntradayImpulse"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SwamiRelativePerformance, "SwamiRelativePerformance"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SwingIndex, "SwingIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::SymbolRelation, "SymbolRelation"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TAC_DIMinus, "TAC_DIMinus"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TAC_DIPlus, "TAC_DIPlus"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TEMA, "TEMA"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::THETA, "THETA"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TMV, "TMV"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TRIX, "TRIX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TTM_Squeeze, "TTM_Squeeze"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TTM_Wave, "TTM_Wave"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TheoreticalOptionPrice, "TheoreticalOptionPrice"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeBlackCrows, "ThreeBlackCrows"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeInsideDown, "ThreeInsideDown"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeInsideUp, "ThreeInsideUp"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeLineStrike, "ThreeLineStrike"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeOutsideDown, "ThreeOutsideDown"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeOutsideUp, "ThreeOutsideUp"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeStarsInTheSouth, "ThreeStarsInTheSouth"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::ThreeWhiteSoldiers, "ThreeWhiteSoldiers"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::Thrusting, "Thrusting"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TimeSeriesForecast, "TimeSeriesForecast"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TrendPeriods, "TrendPeriods"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TriStar, "TriStar"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TrueRangeIndicator, "TrueRangeIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TrueRangeSpecifiedVolume, "TrueRangeSpecifiedVolume"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TrueStrengthIndex, "TrueStrengthIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TwoCrows, "TwoCrows"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::TypicalPrice, "TypicalPrice"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UlcerIndex, "UlcerIndex"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UltimateOscillator, "UltimateOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UniqueThreeRiverBottom, "UniqueThreeRiverBottom"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UpsideGapThreeMethods, "UpsideGapThreeMethods"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UpsideGapTwoCrows, "UpsideGapTwoCrows"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::UpsideTasukiGap, "UpsideTasukiGap"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::VEGA, "VEGA"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::VOLUME, "VOLUME"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::VOL_DIFF, "VOL_DIFF"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::VOL_INDEX, "VOL_INDEX"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VerticalHorizontalFilter, "VerticalHorizontalFilter"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolatilityStdDev, "VolatilityStdDev"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolatilitySwitch, "VolatilitySwitch"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeAccumulation, "VolumeAccumulation"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeAvg, "VolumeAvg"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeFlowIndicator, "VolumeFlowIndicator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeOsc, "VolumeOsc"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeRateOfChange, "VolumeRateOfChange"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeWeightedMACD, "VolumeWeightedMACD"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VolumeZoneOscillator, "VolumeZoneOscillator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::VortexIndicator, "VortexIndicator"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::WEIGHTED_BACK_VOL, "WEIGHTED_BACK_VOL"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WeaknessInAStrongTrend, "WeaknessInAStrongTrend"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WeightedClose, "WeightedClose"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WildersSmoothing, "WildersSmoothing"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WilliamsAlligator, "WilliamsAlligator"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WilliamsPercentR, "WilliamsPercentR"),
    //TOS_Topics::topic_map_entry_type(
    //    TOS_Topics::TOPICS::WoodiesPivots, "WoodiesPivots"),
    TOS_Topics::topic_map_entry_type(
        TOS_Topics::TOPICS::YIELD, "YIELD")
        };
const TOS_Topics::topic_map_type TOS_Topics::_globalTopicMap = gtmArr;
const TOS_Topics::topic_map_type& TOS_Topics::globalTopicMap = TOS_Topics::_globalTopicMap;