#!/usr/bin/env python3
import os
import pandas as pd
import numpy as np
import talib as ta
import sys
# 添加项目根目录到 Python 路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import cpp_backtest
from backtest import create_backtest, create_account_details_full, create_aggregated_record


def generate_signals(
    df: pd.DataFrame,
    entry_state: pd.DataFrame,
    exit_state: pd.DataFrame,
    trade_type: str = 'long',
    signal_type: str = 'trend'
) -> tuple[np.ndarray, np.ndarray]:
    """
    生成入场/出场信号：
    - trade_type: 'long' / 'short' / 'long_short'
    - signal_type: 'trend'（持续趋势）或 'cross'（金叉死叉）
    返回: (entry_signals, exit_signals)
    信号值: 
    - long模式: entry +1, exit -1
    - short模式: entry -1, exit +1
    - long_short模式: entry +1/-1 表示做多/做空, exit不使用
    """

    # 交叉信号：从 False->True 的那一刻
    cross_entry = ((entry_state == 1) & (entry_state.shift(1) == 0)).astype(int)
    cross_exit  = ((exit_state  == 1) & (exit_state.shift(1)  == 0)).astype(int)

    # 选择信号源
    if signal_type == 'cross':
        src_entry, src_exit = cross_entry, cross_exit
    elif signal_type == 'trend':
        src_entry, src_exit = entry_state, exit_state
    else:
        raise ValueError(f"Unsupported signal_type: {signal_type}")

    # 初始化
    n = len(df)
    entry = np.zeros(n, dtype=int)
    exit_ = np.zeros(n, dtype=int)

    # 根据新的逻辑设置信号
    if trade_type == 'long':
        # 做多模式: 入场信号+1, 出场信号-1
        entry[src_entry.values == 1] = 1
        exit_[src_exit.values == 1] = -1
    elif trade_type == 'short':
        # 做空模式: 入场信号-1, 出场信号+1
        entry[src_entry.values == 1] = -1
        exit_[src_exit.values == 1] = 1
    elif trade_type == 'long_short':
        # 长短模式保持不变: 入场信号决定方向
        entry[src_entry.values == 1] = 1   # 指标进入多头
        exit_[src_exit.values == 1] = -1   # 指标死叉做空
        entry[src_exit.values == 1] = -1   # 指标进入空头
        exit_[src_entry.values == 1] = 1   # 指标金叉做多
    else:
        raise ValueError(f"Unsupported trade_type: {trade_type}")

    return entry, exit_


def main():
    # 定位数据文件
    current_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(current_dir)
    data_path = os.path.join(root_dir, 'data', 'btc.csv')

    # 加载CSV
    df = pd.read_csv(data_path)
    if 'date' in df.columns:
        df['date'] = pd.to_datetime(df['date'], unit='ms')
        df.set_index('date', inplace=True)
    elif 'time' in df.columns:
        df['date'] = pd.to_datetime(df['time'])
        df.set_index('date', inplace=True)
    else:
        raise KeyError("CSV must contain 'date' or 'time' column")

    # 计算SMA
    df['sma'] = ta.SMA(df['close'], timeperiod=10)
    df.dropna(subset=['sma'], inplace=True)
    entry_state = (df['close'] > df['sma']).astype(int)
    exit_state  = (df['close'] < df['sma']).astype(int)
    print("数据预览:")
    print(df.head())

    # 参数配置
    trade_type = 'long'       # 'long' / 'short' / 'long_short'
    signal_type = 'trend'           # 'trend' / 'cross'

    # 生成信号
    entry, exit_ = generate_signals(df, entry_state, exit_state, trade_type, signal_type)
    df['entry_signal'] = entry
    df['exit_signal']  = exit_

    # 打印信号统计
    print(f"入场信号 +1 数量: {np.sum(entry == 1)}")
    print(f"入场信号 -1 数量: {np.sum(entry == -1)}")
    print(f"出场信号 +1 数量: {np.sum(exit_  == 1)}")
    print(f"出场信号 -1 数量: {np.sum(exit_  == -1)}")

    # 回测执行
    results = create_backtest(
        entry=entry,
        exit=exit_,
        data=df,
        commission=0.01/100,
        initial_capital=10000,
        position_size=0.3,
        take_profit=0,
        stop_loss=0,
        min_holding=1,
        max_holding=0,
        slippage=0.0,
        max_positions=10,
        timeframe='1d',
        trade_type=trade_type,
        force_close_on_signal=True
    )

    # 输出回测指标
    m = results['metrics']
    print("\n回测结果摘要:")
    for key in ['start_value','end_value','total_return','annualized_return','max_drawdown','total_trades','win_rate','profit_factor','sharpe_ratio']:
        print(f"{key}: {m[key]}")

    # 保存明细
    account_file = create_account_details_full(m['total_trades'], initial_capital=10000.0)
    print(f"账户详细信息保存至: {account_file}")

    record_file = create_aggregated_record(df, entry, exit_, results, account_file)
    print(f"聚合记录保存至: {record_file}")

    # 打印前两笔交易
    trades = results.get('trades', [])
    if trades:
        print(f"共 {len(trades)} 笔交易")
        for i, t in enumerate(trades[:2], start=1):
            print(f"# {i}: 入{t['entry_time']} 平{t['exit_time']} 利润{t['profit_pct']:.2f}% 原因{t['exit_reason']}")

if __name__ == '__main__':
    main()
