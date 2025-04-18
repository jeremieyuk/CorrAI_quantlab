#include "backtest.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <numeric>

// 将DateTime格式化为字符串
std::string formatDateTime(const DateTime& dt) {
    // 验证日期是否有效
    auto now = std::chrono::system_clock::now();
    DateTime valid_dt = dt;
    
    // 如果日期在未来，使用当前时间代替
    if (dt > now) {
        std::cerr << "警告: 检测到未来日期，使用当前时间替代" << std::endl;
        valid_dt = now;
    }
    
    std::time_t tt = std::chrono::system_clock::to_time_t(valid_dt);
    std::tm* tm = std::localtime(&tt);
    
    char buffer[25];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    
    // 确保年份在合理范围内 (1970-2100)
    std::string date_str(buffer);
    if (date_str.length() >= 4) {
        int year = std::stoi(date_str.substr(0, 4));
        if (year > 2100 || year < 1970) {
            // 替换为当前年份
            auto now_tt = std::chrono::system_clock::to_time_t(now);
            std::tm* now_tm = std::localtime(&now_tt);
            char year_buffer[5];
            std::strftime(year_buffer, sizeof(year_buffer), "%Y", now_tm);
            date_str.replace(0, 4, year_buffer);
        }
    }
    
    return date_str;
}

// 创建完整的账户详细信息并保存为CSV (基于原始价格数据)
void create_account_details_with_prices(
    const std::vector<Trade>& trades,
    const std::vector<double>& prices,
    const std::vector<DateTime>& dates,
    double initial_capital,
    const std::string& output_file
) {
    if (trades.empty()) {
        std::cerr << "No trades provided for account details generation" << std::endl;
        return;
    }
    
    if (prices.empty() || dates.empty() || prices.size() != dates.size()) {
        std::cerr << "Invalid price data or date data for account details generation" << std::endl;
        return;
    }
    
    std::cout << "Generating complete account details for " << trades.size() 
              << " trades across " << dates.size() << " time points..." << std::endl;
    
    // 创建账户详细信息数据结构
    struct AccountDetail {
        DateTime date;
        double price;
        double balance;
        double position_value;
        double total_value;
        double profit_loss;
        double cumulative_return;
        double drawdown;
        int active_trades;
    };
    
    // 对于每个交易创建一个交易事件列表
    struct TradeEvent {
        DateTime time;
        bool is_entry;
        double amount;
        int trade_id;
    };
    
    std::vector<TradeEvent> trade_events;
    
    for (size_t i = 0; i < trades.size(); ++i) {
        const Trade& trade = trades[i];
        
        // 添加入场事件
        trade_events.push_back({
            trade.entryTime,
            true,
            trade.entryInvestment,
            static_cast<int>(i)
        });
        
        // 添加出场事件
        trade_events.push_back({
            trade.exitTime,
            false,
            trade.exitValue,
            static_cast<int>(i)
        });
    }
    
    // 按日期对事件进行排序
    std::sort(trade_events.begin(), trade_events.end(), 
        [](const TradeEvent& a, const TradeEvent& b) {
            return a.time < b.time;
        });
    
    // 创建账户详细信息时间序列
    std::vector<AccountDetail> account_details;
    
    double balance = initial_capital;
    double max_value = initial_capital;
    std::map<int, double> active_positions; // trade_id -> position_value（仓位入场投资金额）
    std::map<int, double> active_position_sizes; // trade_id -> position_size（仓位大小）
    int current_event_index = 0;
    
    // 为每个原始数据点计算账户状态
    for (size_t i = 0; i < dates.size(); ++i) {
        const DateTime& current_date = dates[i];
        const double current_price = prices[i];
        
        // 处理当前日期之前或等于的所有交易事件
        while (current_event_index < trade_events.size() && 
               trade_events[current_event_index].time <= current_date) {
            
            const auto& event = trade_events[current_event_index];
            
            if (event.is_entry) {
                // 入场，减少余额，增加持仓
                balance -= event.amount;
                active_positions[event.trade_id] = event.amount;
                
                // 获取对应交易的仓位大小
                if (event.trade_id < trades.size()) {
                    active_position_sizes[event.trade_id] = trades[event.trade_id].quantity;
                }
            } else {
                // 出场，增加余额，减少持仓
                balance += event.amount;
                active_positions.erase(event.trade_id);
                active_position_sizes.erase(event.trade_id);
            }
            
            current_event_index++;
        }
        
        // 计算当前持仓价值
        double position_value = 0.0;
        
        // 根据当前价格计算持仓价值
        for (const auto& pos_pair : active_position_sizes) {
            int trade_id = pos_pair.first;
            double position_size = pos_pair.second;
            position_value += position_size * current_price;
        }
        
        // 计算总账户价值和累计收益
        double total_value = balance + position_value;
        double profit_loss = total_value - initial_capital;
        double cumulative_return = (total_value / initial_capital - 1.0) * 100.0;
        
        // 计算回撤
        max_value = std::max(max_value, total_value);
        double drawdown = ((max_value - total_value) / max_value) * 100.0;
        
        // 添加到账户详细信息
        account_details.push_back({
            current_date,
            current_price,
            balance,
            position_value,
            total_value,
            profit_loss,
            cumulative_return,
            drawdown,
            static_cast<int>(active_positions.size())
        });
    }
    
    // 生成统计数据
    double final_value = account_details.empty() ? initial_capital : account_details.back().total_value;
    double total_return = (final_value / initial_capital - 1.0) * 100.0;
    
    // 计算最大回撤
    double max_drawdown = 0.0;
    for (const auto& detail : account_details) {
        max_drawdown = std::max(max_drawdown, detail.drawdown);
    }
    
    // 写入CSV文件
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return;
    }
    
    // 写入CSV头
    file << "Date,Price,Balance,Position Value,Total Value,Profit/Loss,Cumulative Return (%),Drawdown (%),Active Trades\n";
    
    // 写入详细数据
    for (const auto& detail : account_details) {
        file << formatDateTime(detail.date) << ","
             << std::fixed << std::setprecision(2) << detail.price << ","
             << std::fixed << std::setprecision(2) << detail.balance << ","
             << std::fixed << std::setprecision(2) << detail.position_value << ","
             << std::fixed << std::setprecision(2) << detail.total_value << ","
             << std::fixed << std::setprecision(2) << detail.profit_loss << ","
             << std::fixed << std::setprecision(2) << detail.cumulative_return << ","
             << std::fixed << std::setprecision(2) << detail.drawdown << ","
             << detail.active_trades << "\n";
    }
    
    // 写入摘要统计
    file << "\nSummary Statistics\n";
    file << "Initial Capital," << std::fixed << std::setprecision(2) << initial_capital << "\n";
    file << "Final Value," << std::fixed << std::setprecision(2) << final_value << "\n";
    file << "Total Return (%)," << std::fixed << std::setprecision(2) << total_return << "\n";
    file << "Max Drawdown (%)," << std::fixed << std::setprecision(2) << max_drawdown << "\n";
    file << "Total Trades," << trades.size() << "\n";
    file << "Total Data Points," << dates.size() << "\n";
    
    file.close();
    
    std::cout << "Complete account details saved to " << output_file << std::endl;
} 