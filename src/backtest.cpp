#include "backtest.h"
#include <numeric>
#include <limits>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

// 将ExitReason转换为字符串的函数实现
std::string exitReasonToString(ExitReason reason) {
    switch (reason) {
        case ExitReason::TAKE_PROFIT:
            return "Take Profit";
        case ExitReason::STOP_LOSS:
            return "Stop Loss";
        case ExitReason::EXIT_SIGNAL:
            return "Exit Signal";
        case ExitReason::MAX_HOLDING_PERIOD:
            return "Max Holding Period Reached";
        case ExitReason::FORCE_EXIT:
            return "Force Exit due to Negative Capital";
        default:
            return "Unknown";
    }
}

// 解析交易类型
TradeType Backtest::parseTradeType(const std::string& type) {
    std::string lowerType = type;
    std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);
    
    if (lowerType == "long") {
        return TradeType::LONG;
    } else if (lowerType == "short") {
        return TradeType::SHORT;
    } else if (lowerType == "long_short") {
        return TradeType::LONG_SHORT;
    }
    
    // 默认为做多
    return TradeType::LONG;
}

// 解析信号优先级模式字符串为枚举
SignalPriorityMode Backtest::parseSignalPriorityMode(const std::string& mode) {
    if (mode == "entry_first") {
        return SignalPriorityMode::ENTRY_FIRST;
    } else if (mode == "same_bar_trade") {
        return SignalPriorityMode::SAME_BAR_TRADE;
    } else {
        // 默认为exit_first
        return SignalPriorityMode::EXIT_FIRST;
    }
}

// 获取退出原因字符串
std::string Backtest::getExitReasonString(ExitReason reason) {
    return exitReasonToString(reason);
}

// 单参数构造函数
Backtest::Backtest(
    const std::vector<double>& prices,             // 价格序列
    const std::vector<int>& entrySignals,          // 入场信号 (1: long, -1: short, 0: no signal)
    const std::vector<int>& exitSignals,           // 出场信号 (1: exit long, -1: exit short, 0: no signal)
    const std::vector<DateTime>& dates,            // 日期时间序列
    const std::string& timeframe,                  // 时间框架
    const std::string& tradeType,                  // 交易类型 (long_only, short_only, long_short)
    double initialCapital,                         // 初始资金
    double positionSizePct,                        // 仓位大小百分比
    double commissionPct,                          // 手续费率
    double takeProfitPct,                          // 止盈百分比 (0表示不使用)
    double stopLossPct,                            // 止损百分比 (0表示不使用)
    int minHoldingPeriod,                          // 最小持仓周期
    int maxHoldingPeriod,                          // 最大持仓周期 (0表示不限制)
    double slippagePct,                             // 滑点百分比
    int maxPositionsParam,                           // 最大持仓数量 (0表示不限制)
    bool forceCloseAtEnd,                            // 是否在回测结束时强制平仓
    const std::string& signalPriorityMode           // 信号优先级模式
) : interval(timeframe),
    tradeType(parseTradeType(tradeType)),
    startFund(initialCapital),
    eachTrade(positionSizePct),
    tradeFees(commissionPct),
    tpStop(takeProfitPct),
    slStop(stopLossPct),
    minHolding(minHoldingPeriod),
    maxHolding(maxHoldingPeriod),
    slippagePct(slippagePct),
    maxPositions(maxPositionsParam),
    forceCloseAtEnd(forceCloseAtEnd),
    signalPriority(parseSignalPriorityMode(signalPriorityMode)),
    isMulti(false) {
    
    std::cout << "Initializing single parameter backtest" << std::endl;
    
    // 检查输入数据大小一致性
    if (prices.size() != entrySignals.size() || prices.size() != exitSignals.size() || prices.size() != dates.size()) {
        throw std::invalid_argument("Price, entries, exits, and dates must have the same size");
    }
    
    // 初始化数据
    data.prices = prices;
    data.entries = entrySignals;
    data.exits = exitSignals;
    data.dates = dates;
    
    // 初始化回测结果存储
    const size_t dataSize = prices.size();
    data.availableCapital.resize(dataSize, startFund);
    data.positionValue.resize(dataSize, 0.0);
    data.totalCapital.resize(dataSize, startFund);
    data.dailyReturn.resize(dataSize, 0.0);
    data.cumulativeReturn.resize(dataSize, 0.0);
}

// 多参数构造函数
Backtest::Backtest(
    const std::vector<double>& prices,                          // 价格序列
    const std::map<std::string, std::vector<int>>& entrySignals, // 多个策略的入场信号
    const std::map<std::string, std::vector<int>>& exitSignals,  // 多个策略的出场信号
    const std::vector<DateTime>& dates,                         // 日期时间序列
    const std::string& timeframe,                               // 时间框架
    const std::string& tradeType,                               // 交易类型
    double initialCapital,                                      // 初始资金
    double positionSizePct,                                     // 仓位大小百分比
    double commissionPct,                                       // 手续费率
    double takeProfitPct,                                       // 止盈百分比
    double stopLossPct,                                         // 止损百分比
    int minHoldingPeriod,                                       // 最小持仓周期
    int maxHoldingPeriod,                                       // 最大持仓周期
    double slippagePct,                                         // 滑点百分比
    int maxPositionsParam,                                       // 最大持仓数量 (0表示不限制)
    bool forceCloseAtEnd,                                        // 是否在回测结束时强制平仓
    const std::string& signalPriorityMode                        // 信号优先级模式
) : interval(timeframe),
    tradeType(parseTradeType(tradeType)),
    startFund(initialCapital),
    eachTrade(positionSizePct),
    tradeFees(commissionPct),
    tpStop(takeProfitPct),
    slStop(stopLossPct),
    minHolding(minHoldingPeriod),
    maxHolding(maxHoldingPeriod),
    slippagePct(slippagePct),
    maxPositions(maxPositionsParam),
    forceCloseAtEnd(forceCloseAtEnd),
    signalPriority(parseSignalPriorityMode(signalPriorityMode)),
    isMulti(true),
    entriesMap(entrySignals),
    exitsMap(exitSignals) {
    
    std::cout << "Initializing multi parameter backtest" << std::endl;
    
    // 检查输入数据一致性
    for (const auto& entry : entriesMap) {
        const std::string& param = entry.first;
        const std::vector<int>& entries = entry.second;
        
        // 检查对应的exits是否存在
        if (exitsMap.find(param) == exitsMap.end()) {
            throw std::invalid_argument("Exit signals not found for parameter: " + param);
        }
        
        // 检查大小一致性
        if (entries.size() != prices.size() || exitsMap.at(param).size() != prices.size() || prices.size() != dates.size()) {
            throw std::invalid_argument("Price, entries, exits, and dates must have the same size for parameter: " + param);
        }
        
        // 添加参数到参数列表
        params.push_back(param);
    }
    
    // 初始化数据
    data.prices = prices;
    data.dates = dates;
    
    // 初始化回测结果存储
    const size_t dataSize = prices.size();
    data.availableCapital.resize(dataSize, startFund);
    data.positionValue.resize(dataSize, 0.0);
    data.totalCapital.resize(dataSize, startFund);
    data.dailyReturn.resize(dataSize, 0.0);
    data.cumulativeReturn.resize(dataSize, 0.0);
}

// 计算每年的周期数
int Backtest::getPeriodsPerYear() {
    std::unordered_map<std::string, int> intervalMappings = {
        {"1d", 365},
        {"1h", 365 * 24},
        {"4h", 365 * 6},
        {"30m", 365 * 48},
        {"15m", 365 * 96},
        {"5m", 365 * 288},
        {"1m", 365 * 1440}
    };
    
    auto it = intervalMappings.find(interval);
    return (it != intervalMappings.end()) ? it->second : 365; // 默认为365
}

// 获取回测结果
BacktestMetrics Backtest::getResult() const {
    return result;
}

// 获取多策略回测结果
std::map<std::string, BacktestMetrics> Backtest::getResults() const {
    return results;
}

// 获取交易记录
std::vector<Trade> Backtest::getTrades() const {
    // 返回交易记录
    return m_trades;
}

// 将回测结果保存到文件
void Backtest::saveResultsToFile(const std::string& filepath) const {
    // 本简化版暂不实现内部逻辑
}

// 运行单个回测
BacktestMetrics Backtest::runSingleBacktest(const std::vector<int>& entries, const std::vector<int>& exits) {
    std::vector<Trade> trades;
    TimeSeriesData seriesData = data; // 创建一个副本进行操作
    
    const size_t dataSize = seriesData.prices.size();
    double currentCapital = startFund;
    double totalPositionValue = 0.0;
    
    // 当前持有的交易集合
    std::vector<std::unique_ptr<Trade>> currentTrades;
    
    // 预先分配容器容量，避免频繁重新分配
    // 估计每个周期的交易数量，预分配交易记录空间
    size_t estimatedMaxTrades = std::min(dataSize / 5, size_t(1000)); // 假设平均每5个周期一笔交易，最多1000笔
    trades.reserve(estimatedMaxTrades);
    
    // 预分配当前仓位容器，避免频繁扩容
    int maxPos = (maxPositions > 0) ? maxPositions : 10; // 如果不限制，默认预留10个位置
    currentTrades.reserve(maxPos);
    
    // 遍历每个时间点
    for (size_t i = 0; i < dataSize; ++i) { 
        double price = seriesData.prices[i];
        int entrySignal = entries[i];
        int exitSignal = exits[i];
        
        // 更新当前持仓的价值
        totalPositionValue = 0.0;
        for (auto& trade : currentTrades) {
            trade->currentValue = trade->quantity * price;
            totalPositionValue += trade->currentValue;
        }
        
        // 根据信号优先级模式处理信号
        bool hasProcessedEntry = false;
        
        // 根据信号优先级决定处理顺序
        if (signalPriority == SignalPriorityMode::EXIT_FIRST || signalPriority == SignalPriorityMode::SAME_BAR_TRADE) {
            // 先处理平仓信号
            bool hasProcessedExit = processExitSignals(currentTrades, trades, seriesData, i, price, exitSignal, currentCapital);
            
            // 然后处理开仓信号
            hasProcessedEntry = processEntrySignals(currentTrades, seriesData, i, price, entrySignal, currentCapital);
            
            // 如果是SAME_BAR_TRADE模式，且触发了开仓，再次尝试平仓（允许同一个bar内先开后平）
            if (signalPriority == SignalPriorityMode::SAME_BAR_TRADE && hasProcessedEntry && exitSignal != 0) {
                processExitSignals(currentTrades, trades, seriesData, i, price, exitSignal, currentCapital);
            }
        } else {
            // ENTRY_FIRST: 先处理开仓，再处理平仓
            hasProcessedEntry = processEntrySignals(currentTrades, seriesData, i, price, entrySignal, currentCapital);
            processExitSignals(currentTrades, trades, seriesData, i, price, exitSignal, currentCapital);
        }
        
        // 计算当前所有持仓的总价值
        totalPositionValue = 0.0;
        for (const auto& trade : currentTrades) {
            totalPositionValue += trade->currentValue;
        }
        
        // 更新资金序列
        seriesData.availableCapital[i] = currentCapital;
        seriesData.positionValue[i] = totalPositionValue;
        seriesData.totalCapital[i] = currentCapital + totalPositionValue;
        
        // 计算每日收益率
        if (i > 0) {
            double prevTotal = seriesData.totalCapital[i-1];
            if (prevTotal > 0) {
                seriesData.dailyReturn[i] = (seriesData.totalCapital[i] / prevTotal) - 1.0;
            }
        }
    }
    
    // 如果结束时仍有持仓，强制平仓
    size_t lastIndex = dataSize - 1;
    double lastPrice = seriesData.prices[lastIndex];
    
    // 如果启用了结束时平仓选项，则平掉所有未平仓的头寸
    if (forceCloseAtEnd) {
        for (auto& currentTrade : currentTrades) {
            bool isLong = currentTrade->direction == "long";
            
            // 添加滑点计算
            double exitPrice = lastPrice;
            if (slippagePct > 0) {
                // 根据方向计算滑点（做多时卖出比市场价低，做空时买回比市场价高）
                if (isLong) {
                    exitPrice = lastPrice * (1.0 - slippagePct);
                } else {
                    exitPrice = lastPrice * (1.0 + slippagePct);
                }
            }
            
            // 计算交易结果
            double profit;
            double exitValue;
            
            // 根据交易方向计算盈亏和资金更新值
            if (isLong) {
                // 做多：卖出收入减去开仓成本
                exitValue = currentTrade->quantity * exitPrice * (1.0 - tradeFees); // 减去出场手续费
                profit = exitValue - currentTrade->entryInvestment;
            } else {
                // 做空：开仓时借入并卖出的金额 - 平仓时买回的金额（包含费用）
                double buybackCost = currentTrade->quantity * exitPrice; // 买回成本
                double exitFee = buybackCost * tradeFees; // 平仓手续费
                double totalExitCost = buybackCost + exitFee; // 总平仓成本
                
                // 空头盈利 = 开仓卖出获得资金 - 平仓买回成本(含费用)
                profit = currentTrade->entryInvestment - totalExitCost;
                
                // 对于空头，exitValue是从锁定资金中释放的部分
                // 即锁定资金 + 盈亏
                exitValue = currentTrade->entryInvestment + profit;
            }
            
            double profitPct = profit / currentTrade->entryInvestment * 100.0;
            
            // 更新交易对象
            currentTrade->exitPrice = exitPrice;
            currentTrade->exitTime = seriesData.dates[lastIndex];
            currentTrade->exitIndex = lastIndex;
            currentTrade->exitFee = isLong ? 
                (currentTrade->quantity * exitPrice * tradeFees) : // 多头出场费用
                (currentTrade->quantity * exitPrice * tradeFees);  // 空头出场费用
            currentTrade->exitValue = exitValue;
            currentTrade->profit = profit;
            currentTrade->profitPct = profitPct;
            currentTrade->exitReason = "End of Backtest";
            
            // 添加交易记录
            trades.push_back(*currentTrade);
            
            // 更新资金
            currentCapital += exitValue;
        }
    }
    
    // 更新最终资金状态
    if (!currentTrades.empty()) {
        // 如果没有强制平仓，则计算持仓价值
        double finalPositionValue = 0.0;
        if (!forceCloseAtEnd) {
            for (const auto& trade : currentTrades) {
                finalPositionValue += trade->currentValue;
            }
        }
        
        seriesData.availableCapital[lastIndex] = currentCapital;
        seriesData.positionValue[lastIndex] = finalPositionValue;
        seriesData.totalCapital[lastIndex] = currentCapital + finalPositionValue;
    }
    
    // 计算累积收益率
    double cumulativeReturn = 1.0;
    for (size_t i = 0; i < dataSize; ++i) {
        cumulativeReturn *= (1.0 + seriesData.dailyReturn[i]);
        seriesData.cumulativeReturn[i] = (cumulativeReturn - 1.0) * 100.0;
    }
    
    // 保存交易记录
    m_trades = trades;
    
    // 计算回测指标
    return calculateMetrics(seriesData, trades);
}

// 处理平仓信号（从runSingleBacktest中提取出来的逻辑）
bool Backtest::processExitSignals(
    std::vector<std::unique_ptr<Trade>>& currentTrades,
    std::vector<Trade>& trades,
    TimeSeriesData& seriesData,
    size_t i,
    double price,
    int exitSignal,
    double& currentCapital
) {
    bool hasExited = false;
    
    // 检查是否需要出场
    for (auto it = currentTrades.begin(); it != currentTrades.end();) {
        Trade* currentTrade = it->get();
        bool isLong = currentTrade->direction == "long";
        int holdingPeriod = i - currentTrade->entryIndex;
        
        // 计算价格变化百分比（考虑做多/做空方向）
        double priceDiff, pricePct;
        if (isLong) {
            priceDiff = price - currentTrade->entryPrice;
            pricePct = priceDiff / currentTrade->entryPrice;
        } else { // 做空
            priceDiff = currentTrade->entryPrice - price;
            pricePct = priceDiff / currentTrade->entryPrice;
        }
        
        // 检查是否需要出场
        bool shouldExit = false;
        ExitReason exitReason = ExitReason::NONE;
        
        // 检查止盈条件
        if (tpStop > 0 && pricePct >= tpStop) {
            shouldExit = true;
            exitReason = ExitReason::TAKE_PROFIT;
        }
        // 检查止损条件
        else if (slStop > 0 && pricePct <= -slStop) {
            shouldExit = true;
            exitReason = ExitReason::STOP_LOSS;
        }
        // 检查最大持仓周期
        else if (maxHolding > 0 && holdingPeriod >= maxHolding) {
            shouldExit = true;
            exitReason = ExitReason::MAX_HOLDING_PERIOD;
        }
        // 检查出场信号
        // Python端信号定义: 
        // - long模式: exitSignal == -1 表示平多仓
        // - short模式: exitSignal == 1 表示平空仓
        // 确保与Python端generate_signals函数的信号定义一致
        else if (((isLong && exitSignal == -1) || (!isLong && exitSignal == 1)) && holdingPeriod >= minHolding) {
            shouldExit = true;
            exitReason = ExitReason::EXIT_SIGNAL;
        }
        
        // 如果需要出场
        if (shouldExit) {
            hasExited = true;
            
            // 添加滑点计算
            double exitPrice = price;
            if (slippagePct > 0) {
                // 根据方向计算滑点（做多时卖出比市场价低，做空时买回比市场价高）
                if (isLong) {
                    exitPrice = price * (1.0 - slippagePct);
                } else {
                    exitPrice = price * (1.0 + slippagePct);
                }
            }
            
            // 计算交易结果
            double profit;
            double exitValue;
            
            // 根据交易方向计算盈亏和资金更新值
            if (isLong) {
                // 做多：卖出收入减去开仓成本
                exitValue = currentTrade->quantity * exitPrice * (1.0 - tradeFees); // 减去出场手续费
                profit = exitValue - currentTrade->entryInvestment;
            } else {
                // 做空：开仓时借入并卖出的金额 - 平仓时买回的金额（包含费用）
                double buybackCost = currentTrade->quantity * exitPrice; // 买回成本
                double exitFee = buybackCost * tradeFees; // 平仓手续费
                double totalExitCost = buybackCost + exitFee; // 总平仓成本
                
                // 空头盈利 = 开仓卖出获得资金 - 平仓买回成本(含费用)
                profit = currentTrade->entryInvestment - totalExitCost;
                
                // 对于空头，exitValue是从锁定资金中释放的部分
                // 即锁定资金 + 盈亏
                exitValue = currentTrade->entryInvestment + profit;
            }
            
            double profitPct = profit / currentTrade->entryInvestment * 100.0;
            
            // 更新交易对象
            currentTrade->exitPrice = exitPrice;
            currentTrade->exitTime = seriesData.dates[i];
            currentTrade->exitIndex = i;
            currentTrade->exitFee = isLong ? 
                (currentTrade->quantity * exitPrice * tradeFees) : // 多头出场费用
                (currentTrade->quantity * exitPrice * tradeFees);  // 空头出场费用
            currentTrade->exitValue = exitValue;
            currentTrade->profit = profit;
            currentTrade->profitPct = profitPct;
            currentTrade->exitReason = getExitReasonString(exitReason);
            
            // 添加交易记录
            trades.push_back(*currentTrade);
            
            // 更新资金 - 使用统一计算的exitValue
            currentCapital += exitValue;
            
            // 删除此交易记录
            it = currentTrades.erase(it);
        } else {
            ++it;
        }
    }
    
    return hasExited;
}

// 处理开仓信号（从runSingleBacktest中提取出来的逻辑）
bool Backtest::processEntrySignals(
    std::vector<std::unique_ptr<Trade>>& currentTrades,
    TimeSeriesData& seriesData,
    size_t i,
    double price,
    int entrySignal,
    double& currentCapital
) {
    bool hasEntered = false;
    
    // 如果没有达到最大持仓数量，检查入场信号
    if ((maxPositions == 0 || currentTrades.size() < static_cast<size_t>(maxPositions)) && 
        (entrySignal == 1 || entrySignal == -1)) {
        // 根据信号决定交易方向
        bool isLong = entrySignal == 1;
        
        // 只有当交易类型与信号匹配时才进行交易
        bool shouldTrade = false;
        
        if (tradeType == TradeType::LONG && isLong) {
            shouldTrade = true;
        } else if (tradeType == TradeType::SHORT && !isLong) {
            shouldTrade = true;
        } else if (tradeType == TradeType::LONG_SHORT) {
            shouldTrade = true;
        }
        
        if (shouldTrade) {
            hasEntered = true;
            
            // 计算可用于交易的资金
            double tradeAmount = currentCapital * eachTrade;
            
            // 确保有足够资金进行交易
            if (tradeAmount > 0) {
                // 先应用滑点计算出实际成交价
                double entryPrice = price;
                if (slippagePct > 0) {
                    // 根据方向计算滑点（做多时买入比市场价高，做空时卖出比市场价低）
                    if (isLong) {
                        entryPrice = price * (1.0 + slippagePct);
                    } else {
                        entryPrice = price * (1.0 - slippagePct);
                    }
                }
                
                // 然后基于实际成交价计算可买入数量和手续费
                double entryFee = tradeAmount * tradeFees;
                double actualInvestment = tradeAmount - entryFee;
                double quantity = actualInvestment / entryPrice;
                
                // 创建新交易
                auto newTrade = std::make_unique<Trade>();
                newTrade->entryTime = seriesData.dates[i];
                newTrade->entryPrice = entryPrice;
                newTrade->entryIndex = i;
                newTrade->quantity = quantity;
                newTrade->entryFee = entryFee;
                newTrade->entryInvestment = tradeAmount;
                newTrade->direction = isLong ? "long" : "short";
                newTrade->currentValue = actualInvestment;
                
                // 更新资金
                currentCapital -= tradeAmount;
                
                // 添加到持仓列表
                currentTrades.push_back(std::move(newTrade));
            }
        }
    }
    
    return hasEntered;
}

// 计算回测指标
BacktestMetrics Backtest::calculateMetrics(const TimeSeriesData& data, const std::vector<Trade>& trades, double riskFreeRate) {
    BacktestMetrics metrics;
    
    if (data.totalCapital.empty()) {
        return metrics;
    }
    
    // 基本指标
    metrics.startValue = startFund;
    metrics.endValue = data.totalCapital.back();
    metrics.totalReturn = ((metrics.endValue / metrics.startValue) - 1.0) * 100.0;
    
    // 交易相关指标
    metrics.totalTrades = trades.size();
    
    // 计算胜率和盈亏比
    int winningTrades = 0;
    double totalProfit = 0.0;
    double totalLoss = 0.0;
    
    for (const auto& trade : trades) {
        if (trade.profit > 0) {
            winningTrades++;
            totalProfit += trade.profit;
        } else {
            totalLoss -= trade.profit; // 转为正数
        }
    }
    
    // 胜率
    metrics.winningTrades = winningTrades;
    metrics.winRate = (metrics.totalTrades > 0) ? 
                     (static_cast<double>(winningTrades) / metrics.totalTrades * 100.0) : 0.0;
    
    // 盈亏比
    metrics.profitFactor = (totalLoss > 0) ? (totalProfit / totalLoss) : 0.0;
    
    // 最大回撤
    metrics.maxDrawdown = calculateMaxDrawdown(data.totalCapital);
    
    // 年化收益率
    int periodsPerYear = getPeriodsPerYear();
    double years = static_cast<double>(data.totalCapital.size()) / periodsPerYear;
    if (years > 0 && metrics.totalReturn != 0) {
        metrics.annualizedReturn = (std::pow((1.0 + metrics.totalReturn / 100.0), (1.0 / years)) - 1.0) * 100.0;
    }
    
    // 创建权益曲线
    for (size_t i = 0; i < data.totalCapital.size(); i += std::max<size_t>(1, data.totalCapital.size() / 1000)) {
        metrics.equityCurve.emplace_back(data.dates[i], data.totalCapital[i]);
    }
    
    // 夏普比率
    metrics.sharpeRatio = calculateAnnualizedSharpeRatio(data.dailyReturn, riskFreeRate, periodsPerYear);
    
    // 索提诺比率 (Sortino Ratio)
    metrics.sortinoRatio = calculateSortinoRatio(data.dailyReturn, riskFreeRate, periodsPerYear);
    
    // 卡玛比率 (Calmar Ratio)
    metrics.calmarRatio = calculateCalmarRatio(metrics.annualizedReturn, metrics.maxDrawdown);
    
    return metrics;
}

// 计算最大回撤
double Backtest::calculateMaxDrawdown(const std::vector<double>& equityCurve) {
    double maxDrawdown = 0.0;
    double peak = equityCurve[0];
    
    for (size_t i = 1; i < equityCurve.size(); ++i) {
        if (equityCurve[i] > peak) {
            peak = equityCurve[i];
        } else {
            double drawdown = (peak - equityCurve[i]) / peak * 100.0;
            maxDrawdown = std::max(maxDrawdown, drawdown);
        }
    }
    
    return maxDrawdown;
}

// 计算年化夏普比率
double Backtest::calculateAnnualizedSharpeRatio(const std::vector<double>& returns, double riskFreeRate, int periodsPerYear) {
    if (returns.empty()) {
        return 0.0;
    }
    
    // 调整无风险收益率为每期收益率
    double periodRiskFreeRate = std::pow(1.0 + riskFreeRate, 1.0 / periodsPerYear) - 1.0;
    
    // 计算超额收益
    std::vector<double> excessReturns;
    for (double r : returns) {
        if (!std::isnan(r)) {
            excessReturns.push_back(r - periodRiskFreeRate);
        }
    }
    
    if (excessReturns.empty()) {
        return 0.0;
    }
    
    // 计算平均超额收益
    double meanExcessReturn = std::accumulate(excessReturns.begin(), excessReturns.end(), 0.0) / excessReturns.size();
    
    // 计算超额收益标准差
    double variance = 0.0;
    for (double r : excessReturns) {
        variance += std::pow(r - meanExcessReturn, 2);
    }
    variance /= excessReturns.size();
    
    double stdDev = std::sqrt(variance);
    if (stdDev <= std::numeric_limits<double>::epsilon()) {
        return 0.0;
    }
    
    // 计算夏普比率并年化
    double sharpeRatio = meanExcessReturn / stdDev;
    return sharpeRatio * std::sqrt(static_cast<double>(periodsPerYear));
}

// 计算Sortino比率
double Backtest::calculateSortinoRatio(const std::vector<double>& returns, double riskFreeRate, int periodsPerYear) {
    if (returns.empty()) {
        return 0.0;
    }
    
    // 调整无风险收益率为每期收益率
    double periodRiskFreeRate = std::pow(1.0 + riskFreeRate, 1.0 / periodsPerYear) - 1.0;
    
    // 计算超额收益
    std::vector<double> excessReturns;
    for (double r : returns) {
        if (!std::isnan(r)) {
            excessReturns.push_back(r - periodRiskFreeRate);
        }
    }
    
    if (excessReturns.empty()) {
        return 0.0;
    }
    
    // 计算平均超额收益
    double meanExcessReturn = std::accumulate(excessReturns.begin(), excessReturns.end(), 0.0) / excessReturns.size();
    
    // 计算下行偏差（只考虑负收益）
    double sumSquaredDownside = 0.0;
    int downsideCount = 0;
    
    for (double r : excessReturns) {
        if (r < 0) {
            sumSquaredDownside += std::pow(r, 2);
            downsideCount++;
        }
    }
    
    // 如果没有下行波动，返回一个很大的正数表示极好的比率
    if (downsideCount == 0 || sumSquaredDownside <= std::numeric_limits<double>::epsilon()) {
        return meanExcessReturn > 0 ? 100.0 : 0.0; // 如果平均收益为正，返回高值；否则返回0
    }
    
    // 计算下行偏差
    double downsideDeviation = std::sqrt(sumSquaredDownside / downsideCount);
    
    // 计算Sortino比率并年化
    double sortinoRatio = meanExcessReturn / downsideDeviation;
    return sortinoRatio * std::sqrt(static_cast<double>(periodsPerYear));
}

// 计算Calmar比率
double Backtest::calculateCalmarRatio(double annualizedReturn, double maxDrawdown) {
    // 如果最大回撤非常小或为0，返回一个高值表示极好的比率
    if (maxDrawdown < 0.01) {
        return annualizedReturn > 0 ? 100.0 : 0.0; // 如果年化收益为正，返回高值；否则返回0
    }
    
    // Calmar比率 = 年化收益率 / 最大回撤
    return annualizedReturn / maxDrawdown;
}

// Backtest::run的实现
void Backtest::run() {
    runBacktest();
}

// Backtest::runBacktest的实现
void Backtest::runBacktest() {
    if (isMulti) {
        std::cout << "Running multi-parameter backtest" << std::endl;
        // 多参数组合的回测
        for (const auto& param : params) {
            std::cout << "Running backtest for param: " << param << std::endl;
            
            // 为每个参数组合创建一个新的数据集
            std::vector<int> paramEntries = entriesMap[param];
            std::vector<int> paramExits = exitsMap[param];
            
            // 运行单个参数组合的回测
            results[param] = runSingleBacktest(paramEntries, paramExits);
        }
    } else {
        std::cout << "Running single parameter backtest" << std::endl;
        // 单参数组合的回测
        result = runSingleBacktest(data.entries, data.exits);
    }
} 