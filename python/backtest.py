import pandas as pd
import numpy as np
from datetime import datetime
import importlib.util
import sys
import os
import uuid
import csv

# 获取当前脚本的绝对路径
current_dir = os.path.dirname(os.path.abspath(__file__))
# 获取项目根目录
root_dir = os.path.dirname(current_dir)
# 构建数据文件的完整路径
data_path = os.path.join(root_dir, 'data', 'btc.csv')

# 尝试导入cpp_backtest模块
try:
    import cpp_backtest
except ImportError:
    print("C++ backtest module not found. Please run 'pip install -e .' from the project root first.")
    sys.exit(1)

def create_backtest(
    entry,
    exit,
    data,
    commission=0.001,
    initial_capital=10000.0,
    position_size=1.0,
    take_profit=0.0,
    stop_loss=0.0,
    min_holding=1,
    max_holding=0,
    timeframe="1d",
    trade_type="long",
    slippage=0.0,
    price_type="close",
    max_positions=1,
    force_close_on_signal=True
):
    """
    使用C++后端进行回测计算
    
    参数:
    entry (Series): 入场信号 (布尔值或-1/0/1信号)
    exit (Series): 出场信号 (布尔值或-1/0/1信号)
    data (DataFrame): 数据框，必须包含价格列和日期索引
    commission (float): 交易手续费率 (默认: 0.001，即0.1%)
    initial_capital (float): 初始资金 (默认: 10000.0)
    position_size (float): 仓位大小比例 (默认: 1.0，即100%)
    take_profit (float): 止盈比例 (默认: 0.0，即不启用)
    stop_loss (float): 止损比例 (默认: 0.0，即不启用)
    min_holding (int): 最小持仓周期 (默认: 1)
    max_holding (int): 最大持仓周期 (默认: 0，即不限制)
    timeframe (str): 时间周期 (默认: "1d")
    trade_type (str): 交易类型，"long"或"short"或"long_short" (默认: "long")
    slippage (float): 滑点百分比 (默认: 0.0，即无滑点)
    price_type (str): 使用的价格类型 (默认: "close")
    max_positions (int): 最大同时持仓数量 (默认: 1，即单笔持仓，0表示不限制)
    force_close_on_signal (bool): 强制在出现出场信号时平仓 (默认: True)
    
    返回:
    dict: 包含回测结果的字典
    """
    # 确保数据有索引
    if data.index.name is None:
        data.index.name = 'date'
    
    # 转换入场信号
    if hasattr(entry, 'dtype') and isinstance(entry, np.ndarray):
        # 如果已经是numpy数组，直接使用
        entry_signals = entry
    elif hasattr(entry, 'dtype') and entry.dtype == bool:
        # 如果是布尔值Series，转为1/0
        entry_signals = np.where(entry, 1, 0)
    else:
        # 如果是pandas Series
        entry_signals = entry.values
    
    # 转换出场信号
    if hasattr(exit, 'dtype') and isinstance(exit, np.ndarray):
        # 如果已经是numpy数组，直接使用
        exit_signals = exit
    elif hasattr(exit, 'dtype') and exit.dtype == bool:
        # 如果是布尔值Series，转为1/0
        exit_signals = np.where(exit, 1, 0)
    else:
        # 如果是pandas Series
        exit_signals = exit.values
    
    # 根据price_type获取价格数据
    if price_type in data.columns:
        prices = data[price_type].values
    else:
        prices = data['close'].values
        print(f"警告: 未找到列 '{price_type}', 使用 'close' 列代替。")
    
    # 处理日期数据
    if isinstance(data.index[0], (int, float)):
        dates = [datetime.fromtimestamp(ts/1000) if ts > 1e10 else datetime.fromtimestamp(ts) 
                for ts in data.index]
    else:
        # 如果索引已经是datetime，直接使用
        dates = data.index.to_pydatetime().tolist()
    
    # 根据C++模块的要求调整交易类型参数
    cpp_trade_type = trade_type.lower()
    
    # 调用C++回测函数
    try:
        result = cpp_backtest.run_backtest(
            prices=prices,
            entries=entry_signals,
            exits=exit_signals,
            dates=dates,
            timeframe=timeframe,
            trade_type=cpp_trade_type,
            initial_capital=initial_capital,
            position_size_pct=position_size,
            commission_pct=commission,
            take_profit_pct=take_profit,
            stop_loss_pct=stop_loss,
            min_holding_period=min_holding,
            max_holding_period=max_holding,
            slippage_pct=slippage,
            max_positions=max_positions,
            force_close_on_signal=force_close_on_signal
        )
        return result
    except Exception as e:
        print(f"回测执行出错: {str(e)}")
        raise

def create_account_details_full(
    total_trades,
    initial_capital=10000.0,
    output_file="account_details_full.csv"
):
    """
    生成完整的账户详细信息并保存为CSV文件
    此函数使用原始价格数据的每一个时间点生成详细账户信息
    
    参数:
    total_trades (int): 交易总数，用于验证是否有足够的交易数据
    initial_capital (float): 初始资金 (默认: 10000.0)
    output_file (str): 输出文件路径 (默认: "account_details_full.csv")
    
    返回:
    str: 输出文件路径或None（如果出错）
    """
    try:
        result = cpp_backtest.create_account_details_full(
            total_trades=total_trades,
            initial_capital=initial_capital,
            output_file=output_file
        )
        return result
    except Exception as e:
        print(f"生成完整账户详细信息出错: {str(e)}")
        return None

# 创建一个函数，将所有数据聚合为一个DataFrame并保存为CSV
def create_aggregated_record(df, entry, exit, results, account_details_file=None):
    """
    聚合所有数据，包括原始价格、指标、信号和账户信息，保存为CSV文件
    
    参数:
    df: 原始数据DataFrame，包含价格和指标
    entry: 入场信号数组
    exit: 出场信号数组
    results: 回测结果字典
    account_details_file: 账户详情文件路径（如果有）
    
    返回:
    生成的CSV文件路径
    """
    # 创建副本以避免修改原始数据
    record_df = df.copy()
    
    # 添加入场和出场信号
    record_df['entry_signal'] = entry
    record_df['exit_signal'] = exit
    
    # 如果有账户详情文件，则加载并合并
    if account_details_file and os.path.exists(account_details_file):
        try:
            # 读取账户详情文件（跳过最后的摘要统计行）
            account_df = pd.read_csv(account_details_file)
            
            # 找到Summary Statistics的行索引
            summary_start_idx = account_df[account_df.iloc[:,0] == 'Summary Statistics'].index
            if len(summary_start_idx) > 0:
                # 排除摘要统计部分
                account_df = account_df.iloc[:summary_start_idx[0]]
            
            # 将Date列转换为datetime并设为索引
            account_df['Date'] = pd.to_datetime(account_df['Date'])
            account_df.set_index('Date', inplace=True)
            
            # 重命名列以避免冲突
            account_columns = {
                'Price': 'account_price',
                'Balance': 'account_balance',
                'Position Value': 'position_value',
                'Total Value': 'total_value',
                'Profit/Loss': 'profit_loss',
                'Cumulative Return (%)': 'cumulative_return',
                'Drawdown (%)': 'drawdown',
                'Active Trades': 'active_trades'
            }
            account_df.rename(columns=account_columns, inplace=True)
            
            # 合并数据 (使用日期索引)
            record_df = record_df.join(account_df, how='left')
        except Exception as e:
            print(f"警告: 合并账户详情失败: {e}")
    
    # 添加回测指标摘要作为元数据
    metrics = results['metrics']
    metadata = {
        "初始资金": f"${metrics['start_value']:.2f}",
        "最终资金": f"${metrics['end_value']:.2f}",
        "总收益率": f"{metrics['total_return']:.2f}%",
        "年化收益率": f"{metrics['annualized_return']:.2f}%",
        "最大回撤": f"{metrics['max_drawdown']:.2f}%",
        "总交易次数": metrics['total_trades'],
        "胜率": f"{metrics['win_rate']:.2f}%",
        "盈亏比": f"{metrics['profit_factor']:.2f}",
        "夏普比率": f"{metrics['sharpe_ratio']:.2f}"
    }
    
    # 生成随机文件名
    random_id = str(uuid.uuid4())[:8]
    record_filename = f"Record_{random_id}.csv"
    record_filepath = os.path.join(root_dir, record_filename)
    
    # 先将DataFrame保存到CSV文件
    record_df.to_csv(record_filepath)
    
    # 添加元数据到CSV底部
    with open(record_filepath, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([])  # 空行
        writer.writerow(["回测指标摘要"])
        for key, value in metadata.items():
            writer.writerow([key, value])
    
    print(f"聚合数据已保存到: {record_filepath}")
    return record_filepath
