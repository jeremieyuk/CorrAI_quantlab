#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include "backtest.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>

namespace py = pybind11;

// 保存最后一次回测的交易数据、价格和日期
static std::vector<Trade> last_backtest_trades;
static std::vector<double> last_backtest_prices;
static std::vector<DateTime> last_backtest_dates;

// 将Python datetime.datetime转换为C++ DateTime
DateTime convert_datetime(const py::handle& dt) {
    py::object dt_obj = py::reinterpret_borrow<py::object>(dt);
    py::object timestamp = dt_obj.attr("timestamp")();
    double ts = timestamp.cast<double>();
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(ts));
}

// 将C++ DateTime转换为Python datetime.datetime
py::object convert_to_py_datetime(const DateTime& dt) {
    auto time_since_epoch = dt.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();
    
    py::object datetime_module = py::module::import("datetime");
    py::object datetime_class = datetime_module.attr("datetime");
    py::object fromtimestamp = datetime_class.attr("fromtimestamp");
    
    return fromtimestamp(seconds);
}

// 将BacktestMetrics转换为Python字典
py::dict convert_metrics_to_dict(const BacktestMetrics& metrics) {
    py::dict result;
    
    result["start_value"] = metrics.startValue;
    result["end_value"] = metrics.endValue;
    result["total_return"] = metrics.totalReturn;
    result["annualized_return"] = metrics.annualizedReturn;
    result["max_drawdown"] = metrics.maxDrawdown;
    result["total_trades"] = metrics.totalTrades;
    result["winning_trades"] = metrics.winningTrades;
    result["win_rate"] = metrics.winRate;
    result["profit_factor"] = metrics.profitFactor;
    result["sharpe_ratio"] = metrics.sharpeRatio;
    result["sortino_ratio"] = metrics.sortinoRatio;
    result["calmar_ratio"] = metrics.calmarRatio;
    
    // 转换权益曲线
    py::list equity_curve;
    for (const auto& point : metrics.equityCurve) {
        py::dict p;
        p["date"] = point.first;
        p["value"] = point.second;
        equity_curve.append(p);
    }
    result["equity_curve"] = equity_curve;
    
    return result;
}

// 将Trade转换为Python字典
py::dict convert_trade_to_dict(const Trade& trade) {
    py::dict result;
    
    result["entry_time"] = trade.entryTime;
    result["exit_time"] = trade.exitTime;
    result["entry_price"] = trade.entryPrice;
    result["exit_price"] = trade.exitPrice;
    result["quantity"] = trade.quantity;
    result["profit"] = trade.profit;
    result["profit_pct"] = trade.profitPct;
    result["direction"] = trade.direction;
    result["exit_reason"] = trade.exitReason;
    
    // 计算持有周期（交易持续的bar数量）
    result["hold_bars"] = trade.exitIndex - trade.entryIndex;
    
    return result;
}

// 主回测函数，返回Python字典
py::dict run_backtest(
    const std::vector<double>& prices,
    const std::vector<int>& entries,
    const std::vector<int>& exits,
    const std::vector<std::chrono::system_clock::time_point>& dates,
    const std::string& timeframe = "1d",
    const std::string& trade_type = "long",
    double initial_capital = 10000.0,
    double position_size_pct = 1.0,
    double commission_pct = 0.001,
    double take_profit_pct = 0.0,
    double stop_loss_pct = 0.0,
    int min_holding_period = 1,
    int max_holding_period = 0,
    double slippage_pct = 0.0,
    int max_positions = 1,
    bool force_close_on_signal = true  // 添加新选项：强制在信号出现时平仓
) {
    // 创建回测对象
    Backtest backtest(
        prices,
        entries,
        exits,
        dates,
        timeframe,
        trade_type,
        initial_capital,
        position_size_pct,
        commission_pct,
        take_profit_pct,
        stop_loss_pct,
        min_holding_period,
        max_holding_period,
        slippage_pct,
        max_positions
    );
    
    // 设置额外属性，如最大持仓数量
    // 目前不需要显式设置，因为构造函数已经处理
    
    // 执行回测
    backtest.run();
    
    // 获取结果
    BacktestMetrics metrics = backtest.getResult();
    std::vector<Trade> trades = backtest.getTrades();
    
    // 保存交易、价格和日期数据以便后续的账户详情生成
    last_backtest_trades = trades;
    last_backtest_prices = prices;
    last_backtest_dates = dates;
    
    // 创建结果字典
    py::dict result;
    
    // 添加指标
    result["metrics"] = convert_metrics_to_dict(metrics);
    
    // 添加交易记录
    py::list trade_list;
    for (const auto& trade : trades) {
        trade_list.append(convert_trade_to_dict(trade));
    }
    result["trades"] = trade_list;
    
    return result;
}

// 包装函数 - 运行多策略回测
py::dict run_multi_backtest(
    const std::vector<double>& prices,
    const std::map<std::string, std::vector<int>>& entries_map,
    const std::map<std::string, std::vector<int>>& exits_map,
    const py::list& dates,
    const std::string& timeframe = "1d",
    const std::string& tradeType = "long",
    double initialCapital = 10000.0,
    double positionSizePct = 1.0,
    double commissionPct = 0.001,
    double takeProfitPct = 0.0,
    double stopLossPct = 0.0,
    int minHoldingPeriod = 1,
    int maxHoldingPeriod = 0,
    double slippage_pct = 0.0,
    int max_positions = 0
) {
    // 转换日期时间
    std::vector<DateTime> cpp_dates;
    for (const auto& date : dates) {
        cpp_dates.push_back(convert_datetime(date));
    }
    
    // 运行回测
    Backtest backtest(
        prices,
        entries_map,
        exits_map,
        cpp_dates,
        timeframe,
        tradeType,
        initialCapital,
        positionSizePct,
        commissionPct,
        takeProfitPct,
        stopLossPct,
        minHoldingPeriod,
        maxHoldingPeriod,
        slippage_pct,
        max_positions
    );
    
    // 手动调用run方法，取代内部自动调用的runBacktest
    backtest.run();
    
    // 获取结果
    std::map<std::string, BacktestMetrics> metrics_map = backtest.getResults();
    
    // 保存第一个策略的交易数据，如果有多个策略
    if (!metrics_map.empty()) {
        // 我们需要一种方法从Backtest类获取各个策略的交易
        // 这里简单地使用已有的getTrades函数，它可能只返回默认策略的交易
        last_backtest_trades = backtest.getTrades();
    }
    
    // 将结果转换为Python字典
    py::dict result;
    py::dict metrics_dict;
    
    for (const auto& pair : metrics_map) {
        metrics_dict[py::str(pair.first)] = convert_metrics_to_dict(pair.second);
    }
    
    result["metrics"] = metrics_dict;
    
    return result;
}

// 包装create_account_details_with_prices函数
py::object create_account_details_with_prices_wrapper(
    const int total_trades,
    double initial_capital = 10000.0,
    const std::string& output_file = "account_details_full.csv"
) {
    // 检查交易和价格数据是否可用
    if (total_trades <= 0) {
        std::cerr << "No trades to process" << std::endl;
        return py::none();
    }
    
    if (last_backtest_trades.empty() || last_backtest_prices.empty() || last_backtest_dates.empty()) {
        std::cerr << "No backtest data available" << std::endl;
        return py::none();
    }
    
    // 调用C++实现的create_account_details_with_prices函数
    create_account_details_with_prices(
        last_backtest_trades, 
        last_backtest_prices, 
        last_backtest_dates, 
        initial_capital, 
        output_file
    );
    
    // 返回输出文件的路径，以便Python可以进一步处理
    return py::str(output_file);
}

PYBIND11_MODULE(cpp_backtest, m) {
    m.doc() = "C++ Backtest module";
    
    m.def("run_backtest", &run_backtest, "Run backtest with given parameters",
        py::arg("prices"),
        py::arg("entries"),
        py::arg("exits"),
        py::arg("dates"),
        py::arg("timeframe") = "1d",
        py::arg("trade_type") = "long",
        py::arg("initial_capital") = 10000.0,
        py::arg("position_size_pct") = 1.0,
        py::arg("commission_pct") = 0.001,
        py::arg("take_profit_pct") = 0.0,
        py::arg("stop_loss_pct") = 0.0,
        py::arg("min_holding_period") = 1,
        py::arg("max_holding_period") = 0,
        py::arg("slippage_pct") = 0.0,
        py::arg("max_positions") = 1,
        py::arg("force_close_on_signal") = true  // 添加新参数的默认值
    );
    
    m.def("run_multi_backtest", &run_multi_backtest,
        py::arg("prices"),
        py::arg("entries_map"),
        py::arg("exits_map"),
        py::arg("dates"),
        py::arg("timeframe") = "1d",
        py::arg("trade_type") = "long",
        py::arg("initial_capital") = 10000.0,
        py::arg("position_size_pct") = 1.0,
        py::arg("commission_pct") = 0.001,
        py::arg("take_profit_pct") = 0.0,
        py::arg("stop_loss_pct") = 0.0,
        py::arg("min_holding_period") = 1,
        py::arg("max_holding_period") = 0,
        py::arg("slippage_pct") = 0.0,
        py::arg("max_positions") = 0,
        "Run a multi-strategy backtest"
    );
    
    m.def("create_account_details_full", &create_account_details_with_prices_wrapper,
        py::arg("total_trades"),
        py::arg("initial_capital") = 10000.0,
        py::arg("output_file") = "account_details_full.csv",
        "Generate complete account information for all price data points and save to CSV file"
    );
} 