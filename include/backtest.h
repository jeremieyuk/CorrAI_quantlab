#ifndef BACKTEST_H
#define BACKTEST_H

#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <cmath>
#include <memory>
#include <chrono>
#include <algorithm>
#include <utility>

// 自定义日期时间类型
using DateTime = std::chrono::system_clock::time_point;

// 交易类型
enum class TradeType {
    LONG,
    SHORT,
    LONG_SHORT
};

// 信号优先级模式
enum class SignalPriorityMode {
    EXIT_FIRST,    // 先执行平仓信号，再执行开仓信号（默认）
    ENTRY_FIRST,   // 先执行开仓信号，再执行平仓信号
    SAME_BAR_TRADE // 允许同一个Bar内先开仓后平仓
};

// 退出原因
enum class ExitReason {
    TAKE_PROFIT,
    STOP_LOSS,
    EXIT_SIGNAL,
    MAX_HOLDING_PERIOD,
    FORCE_EXIT,
    NONE
};

// 辅助函数：将ExitReason转换为字符串
std::string exitReasonToString(ExitReason reason);

// 回测指标结构体
struct BacktestMetrics {
    double startValue = 0.0;       // 起始资金
    double endValue = 0.0;         // 最终资金
    double totalReturn = 0.0;      // 总收益率（百分比）
    double annualizedReturn = 0.0; // 年化收益率（百分比）
    double maxDrawdown = 0.0;      // 最大回撤（百分比）
    int totalTrades = 0;           // 总交易次数
    int winningTrades = 0;         // 盈利交易次数
    double winRate = 0.0;          // 胜率（百分比）
    double profitFactor = 0.0;     // 盈亏比
    double sharpeRatio = 0.0;      // 夏普比率
    double sortinoRatio = 0.0;     // 索提诺比率
    double calmarRatio = 0.0;      // 卡玛比率
    std::vector<std::pair<DateTime, double>> equityCurve; // 权益曲线
};

// 交易记录结构体
struct Trade {
    DateTime entryTime;        // 入场时间
    DateTime exitTime;         // 出场时间
    double entryPrice = 0.0;   // 入场价格
    double exitPrice = 0.0;    // 出场价格
    double quantity = 0.0;     // 交易数量
    double profit = 0.0;       // 盈亏金额
    double profitPct = 0.0;    // 盈亏百分比
    std::string direction;     // 交易方向 (long/short)
    std::string exitReason;    // 出场原因 (signal/tp/sl/max-hold)
    
    // 添加Backtest.cpp中使用的其他字段
    int entryIndex = 0;        // 入场索引
    int exitIndex = 0;         // 出场索引
    double entryFee = 0.0;     // 入场手续费
    double exitFee = 0.0;      // 出场手续费
    double entryInvestment = 0.0; // 入场投资金额
    double currentValue = 0.0; // 当前价值
    double exitValue = 0.0;    // 出场价值
    double tradeReturn = 0.0;  // 交易收益率
    std::string positionType;  // 仓位类型
    double position = 0.0;     // 仓位大小
    int signal = 0;            // 信号
};

// 价格和信号数据结构
struct TimeSeriesData {
    std::vector<DateTime> dates;
    std::vector<double> prices;
    std::vector<int> entries;
    std::vector<int> exits;
    std::vector<double> availableCapital;
    std::vector<double> positionValue;
    std::vector<double> totalCapital;
    std::vector<double> dailyReturn;
    std::vector<double> cumulativeReturn;
};

class Backtest {
public:
    // 构造函数 - 单策略回测
    Backtest(
        const std::vector<double>& prices,             // 价格序列
        const std::vector<int>& entrySignals,          // 入场信号 (1: long, -1: short, 0: no signal)
        const std::vector<int>& exitSignals,           // 出场信号 (1: exit long, -1: exit short, 0: no signal)
        const std::vector<DateTime>& dates,            // 日期时间序列
        const std::string& timeframe = "1d",           // 时间框架
        const std::string& tradeType = "long_only",    // 交易类型 (long_only, short_only, long_short)
        double initialCapital = 10000.0,               // 初始资金
        double positionSizePct = 1.0,                  // 仓位大小百分比
        double commissionPct = 0.001,                  // 手续费率
        double takeProfitPct = 0.0,                    // 止盈百分比 (0表示不使用)
        double stopLossPct = 0.0,                      // 止损百分比 (0表示不使用)
        int minHoldingPeriod = 1,                      // 最小持仓周期
        int maxHoldingPeriod = 0,                      // 最大持仓周期 (0表示不限制)
        double slippagePct = 0.0,                      // 滑点百分比
        int maxPositions = 10,                         // 最大持仓数量 (0表示不限制)
        bool forceCloseAtEnd = true,                   // 在回测结束时强制平仓
        const std::string& signalPriorityMode = "exit_first" // 信号优先级模式
    );

    // 构造函数 - 多策略回测
    Backtest(
        const std::vector<double>& prices,                          // 价格序列
        const std::map<std::string, std::vector<int>>& entrySignals, // 多个策略的入场信号
        const std::map<std::string, std::vector<int>>& exitSignals,  // 多个策略的出场信号
        const std::vector<DateTime>& dates,                         // 日期时间序列
        const std::string& timeframe = "1d",                        // 时间框架
        const std::string& tradeType = "long_only",                 // 交易类型
        double initialCapital = 10000.0,                            // 初始资金
        double positionSizePct = 1.0,                               // 仓位大小百分比
        double commissionPct = 0.001,                               // 手续费率
        double takeProfitPct = 0.0,                                 // 止盈百分比
        double stopLossPct = 0.0,                                   // 止损百分比
        int minHoldingPeriod = 1,                                   // 最小持仓周期
        int maxHoldingPeriod = 0,                                   // 最大持仓周期
        double slippagePct = 0.0,                                   // 滑点百分比
        int maxPositions = 10,                                      // 最大持仓数量 (0表示不限制)
        bool forceCloseAtEnd = true,                                // 在回测结束时强制平仓
        const std::string& signalPriorityMode = "exit_first"        // 信号优先级模式
    );

    // 执行回测
    void run();

    // 获取回测结果
    BacktestMetrics getResult() const;

    // 获取多策略回测结果
    std::map<std::string, BacktestMetrics> getResults() const;

    // 获取交易记录
    std::vector<Trade> getTrades() const;

    // 将回测结果保存到CSV文件
    void saveResultsToFile(const std::string& filepath) const;

private:
    // 价格和信号数据
    TimeSeriesData data;
    
    // 回测参数
    std::string interval;
    TradeType tradeType;
    double startFund;
    double eachTrade;
    double tradeFees;
    double tpStop;
    double slStop;
    int minHolding;
    int maxHolding;
    double slippagePct;  // 滑点百分比
    int maxPositions;    // 最大持仓数量 (0表示不限制)
    bool forceCloseAtEnd;  // 是否在回测结束时强制平仓
    SignalPriorityMode signalPriority; // 信号优先级模式

    // 多参数回测
    bool isMulti;
    std::map<std::string, std::vector<int>> entriesMap;
    std::map<std::string, std::vector<int>> exitsMap;
    std::vector<std::string> params;
    
    // 结果
    BacktestMetrics result;
    std::map<std::string, BacktestMetrics> results;
    std::vector<Trade> m_trades;  // 存储交易记录
    
    // 内部方法
    void runBacktest(); // 添加缺失的runBacktest方法
    int getPeriodsPerYear();
    BacktestMetrics runSingleBacktest(const std::vector<int>& entries, const std::vector<int>& exits);
    BacktestMetrics calculateMetrics(const TimeSeriesData& data, const std::vector<Trade>& trades, double riskFreeRate = 0);
    double calculateMaxDrawdown(const std::vector<double>& equityCurve);
    double calculateAnnualizedSharpeRatio(const std::vector<double>& returns, double riskFreeRate, int periodsPerYear);
    double calculateSortinoRatio(const std::vector<double>& returns, double riskFreeRate, int periodsPerYear);
    double calculateCalmarRatio(double annualizedReturn, double maxDrawdown);
    
    // 信号处理函数
    bool processExitSignals(
        std::vector<std::unique_ptr<Trade>>& currentTrades,
        std::vector<Trade>& trades,
        TimeSeriesData& seriesData,
        size_t i,
        double price,
        int exitSignal,
        double& currentCapital
    );
    
    bool processEntrySignals(
        std::vector<std::unique_ptr<Trade>>& currentTrades,
        TimeSeriesData& seriesData,
        size_t i,
        double price,
        int entrySignal,
        double& currentCapital
    );
    
    // 辅助函数
    std::string getExitReasonString(ExitReason reason);
    TradeType parseTradeType(const std::string& type);
    SignalPriorityMode parseSignalPriorityMode(const std::string& mode);
};

// 创建完整的账户详细信息并保存为CSV (基于原始价格数据)
void create_account_details_with_prices(
    const std::vector<Trade>& trades,
    const std::vector<double>& prices,
    const std::vector<DateTime>& dates,
    double initial_capital = 10000.0,
    const std::string& output_file = "account_details.csv"
);

#endif // BACKTEST_H 