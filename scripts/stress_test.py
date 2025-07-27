#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
压力测试脚本
用于测试ATK_HostPC应用程序在高负载下的性能表现
"""

import os
import sys
import time
import json
import threading
import subprocess
import psutil
import sqlite3
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

class StressTestRunner:
    def __init__(self, config_path="../config/performance_config.json"):
        self.config_path = config_path
        self.config = self.load_config()
        self.test_results = []
        self.start_time = None
        self.end_time = None
        
    def load_config(self):
        """加载测试配置"""
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
            return config.get('stress_test_config', {})
        except Exception as e:
            print(f"Failed to load config: {e}")
            return self.get_default_config()
    
    def get_default_config(self):
        """获取默认测试配置"""
        return {
            "enabled": True,
            "duration_seconds": 300,
            "concurrent_operations": 50,
            "test_scenarios": {
                "database_load": True,
                "memory_stress": True,
                "ui_responsiveness": True,
                "communication_throughput": True
            }
        }
    
    def run_stress_test(self):
        """运行完整的压力测试"""
        if not self.config.get('enabled', False):
            print("Stress test is disabled in configuration")
            return
        
        print("Starting ATK_HostPC Stress Test...")
        print(f"Duration: {self.config['duration_seconds']} seconds")
        print(f"Concurrent operations: {self.config['concurrent_operations']}")
        print("-" * 50)
        
        self.start_time = datetime.now()
        
        # 创建线程池
        with ThreadPoolExecutor(max_workers=self.config['concurrent_operations']) as executor:
            futures = []
            
            # 启动各种测试场景
            test_scenarios = self.config.get('test_scenarios', {})
            
            if test_scenarios.get('database_load', False):
                futures.extend(self.start_database_stress_test(executor))
            
            if test_scenarios.get('memory_stress', False):
                futures.extend(self.start_memory_stress_test(executor))
            
            if test_scenarios.get('ui_responsiveness', False):
                futures.extend(self.start_ui_stress_test(executor))
            
            if test_scenarios.get('communication_throughput', False):
                futures.extend(self.start_communication_stress_test(executor))
            
            # 监控系统性能
            monitor_future = executor.submit(self.monitor_system_performance)
            futures.append(monitor_future)
            
            # 等待测试完成
            try:
                for future in as_completed(futures, timeout=self.config['duration_seconds'] + 60):
                    result = future.result()
                    if result:
                        self.test_results.append(result)
            except Exception as e:
                print(f"Test execution error: {e}")
        
        self.end_time = datetime.now()
        self.generate_test_report()
    
    def start_database_stress_test(self, executor):
        """启动数据库压力测试"""
        print("Starting database stress test...")
        futures = []
        
        # 创建多个数据库操作任务
        for i in range(self.config['concurrent_operations'] // 4):
            futures.append(executor.submit(self.database_stress_worker, i))
        
        return futures
    
    def database_stress_worker(self, worker_id):
        """数据库压力测试工作线程"""
        results = {
            'test_type': 'database_stress',
            'worker_id': worker_id,
            'operations': 0,
            'errors': 0,
            'avg_response_time': 0,
            'start_time': time.time()
        }
        
        # 模拟数据库操作
        db_path = '/tmp/stress_test.db'
        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            
            # 创建测试表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS stress_test (
                    id INTEGER PRIMARY KEY,
                    data TEXT,
                    timestamp REAL
                )
            ''')
            
            total_time = 0
            end_time = time.time() + self.config['duration_seconds']
            
            while time.time() < end_time:
                try:
                    start = time.time()
                    
                    # 执行各种数据库操作
                    if results['operations'] % 4 == 0:
                        # 插入操作
                        cursor.execute(
                            "INSERT INTO stress_test (data, timestamp) VALUES (?, ?)",
                            (f"test_data_{worker_id}_{results['operations']}", time.time())
                        )
                    elif results['operations'] % 4 == 1:
                        # 查询操作
                        cursor.execute("SELECT COUNT(*) FROM stress_test")
                        cursor.fetchone()
                    elif results['operations'] % 4 == 2:
                        # 更新操作
                        cursor.execute(
                            "UPDATE stress_test SET data = ? WHERE id = ?",
                            (f"updated_data_{worker_id}", results['operations'] % 100 + 1)
                        )
                    else:
                        # 删除操作
                        cursor.execute(
                            "DELETE FROM stress_test WHERE id = ?",
                            (results['operations'] % 100 + 1,)
                        )
                    
                    conn.commit()
                    
                    elapsed = time.time() - start
                    total_time += elapsed
                    results['operations'] += 1
                    
                    # 短暂休息避免过度占用资源
                    time.sleep(0.001)
                    
                except Exception as e:
                    results['errors'] += 1
                    print(f"Database operation error in worker {worker_id}: {e}")
            
            conn.close()
            
        except Exception as e:
            results['errors'] += 1
            print(f"Database connection error in worker {worker_id}: {e}")
        
        results['end_time'] = time.time()
        results['avg_response_time'] = total_time / max(results['operations'], 1)
        
        return results
    
    def start_memory_stress_test(self, executor):
        """启动内存压力测试"""
        print("Starting memory stress test...")
        futures = []
        
        for i in range(self.config['concurrent_operations'] // 4):
            futures.append(executor.submit(self.memory_stress_worker, i))
        
        return futures
    
    def memory_stress_worker(self, worker_id):
        """内存压力测试工作线程"""
        results = {
            'test_type': 'memory_stress',
            'worker_id': worker_id,
            'allocations': 0,
            'peak_memory_mb': 0,
            'start_time': time.time()
        }
        
        memory_blocks = []
        end_time = time.time() + self.config['duration_seconds']
        
        try:
            while time.time() < end_time:
                # 分配内存块
                block_size = 1024 * 1024  # 1MB
                memory_block = bytearray(block_size)
                memory_blocks.append(memory_block)
                results['allocations'] += 1
                
                # 监控内存使用
                process = psutil.Process()
                memory_mb = process.memory_info().rss / 1024 / 1024
                results['peak_memory_mb'] = max(results['peak_memory_mb'], memory_mb)
                
                # 定期释放一些内存
                if len(memory_blocks) > 100:
                    memory_blocks = memory_blocks[50:]
                
                time.sleep(0.01)
                
        except Exception as e:
            print(f"Memory stress error in worker {worker_id}: {e}")
        
        results['end_time'] = time.time()
        return results
    
    def start_ui_stress_test(self, executor):
        """启动UI响应性测试"""
        print("Starting UI responsiveness test...")
        futures = []
        
        for i in range(self.config['concurrent_operations'] // 4):
            futures.append(executor.submit(self.ui_stress_worker, i))
        
        return futures
    
    def ui_stress_worker(self, worker_id):
        """UI压力测试工作线程"""
        results = {
            'test_type': 'ui_stress',
            'worker_id': worker_id,
            'ui_operations': 0,
            'avg_response_time': 0,
            'start_time': time.time()
        }
        
        total_time = 0
        end_time = time.time() + self.config['duration_seconds']
        
        try:
            while time.time() < end_time:
                start = time.time()
                
                # 模拟UI操作（实际应用中可能需要与Qt应用交互）
                # 这里使用CPU密集型操作模拟UI渲染
                for _ in range(1000):
                    _ = sum(range(100))
                
                elapsed = time.time() - start
                total_time += elapsed
                results['ui_operations'] += 1
                
                time.sleep(0.016)  # 约60FPS
                
        except Exception as e:
            print(f"UI stress error in worker {worker_id}: {e}")
        
        results['end_time'] = time.time()
        results['avg_response_time'] = total_time / max(results['ui_operations'], 1)
        
        return results
    
    def start_communication_stress_test(self, executor):
        """启动通信吞吐量测试"""
        print("Starting communication throughput test...")
        futures = []
        
        for i in range(self.config['concurrent_operations'] // 4):
            futures.append(executor.submit(self.communication_stress_worker, i))
        
        return futures
    
    def communication_stress_worker(self, worker_id):
        """通信压力测试工作线程"""
        results = {
            'test_type': 'communication_stress',
            'worker_id': worker_id,
            'messages_sent': 0,
            'bytes_transferred': 0,
            'avg_latency': 0,
            'start_time': time.time()
        }
        
        total_latency = 0
        end_time = time.time() + self.config['duration_seconds']
        
        try:
            while time.time() < end_time:
                start = time.time()
                
                # 模拟网络通信（创建和处理数据包）
                message_size = 1024  # 1KB消息
                message_data = b'x' * message_size
                
                # 模拟数据处理延迟
                time.sleep(0.001)
                
                elapsed = time.time() - start
                total_latency += elapsed
                results['messages_sent'] += 1
                results['bytes_transferred'] += message_size
                
                time.sleep(0.005)  # 模拟网络延迟
                
        except Exception as e:
            print(f"Communication stress error in worker {worker_id}: {e}")
        
        results['end_time'] = time.time()
        results['avg_latency'] = total_latency / max(results['messages_sent'], 1)
        
        return results
    
    def monitor_system_performance(self):
        """监控系统性能"""
        results = {
            'test_type': 'system_monitor',
            'cpu_samples': [],
            'memory_samples': [],
            'disk_samples': [],
            'start_time': time.time()
        }
        
        end_time = time.time() + self.config['duration_seconds']
        
        try:
            while time.time() < end_time:
                # 收集系统指标
                cpu_percent = psutil.cpu_percent(interval=1)
                memory = psutil.virtual_memory()
                disk = psutil.disk_usage('/')
                
                results['cpu_samples'].append({
                    'timestamp': time.time(),
                    'cpu_percent': cpu_percent
                })
                
                results['memory_samples'].append({
                    'timestamp': time.time(),
                    'memory_percent': memory.percent,
                    'memory_used_gb': memory.used / 1024 / 1024 / 1024
                })
                
                results['disk_samples'].append({
                    'timestamp': time.time(),
                    'disk_percent': disk.percent,
                    'disk_used_gb': disk.used / 1024 / 1024 / 1024
                })
                
                time.sleep(5)  # 每5秒采样一次
                
        except Exception as e:
            print(f"System monitoring error: {e}")
        
        results['end_time'] = time.time()
        return results
    
    def generate_test_report(self):
        """生成测试报告"""
        print("\n" + "=" * 60)
        print("STRESS TEST REPORT")
        print("=" * 60)
        
        duration = (self.end_time - self.start_time).total_seconds()
        print(f"Test Duration: {duration:.2f} seconds")
        print(f"Start Time: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"End Time: {self.end_time.strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # 按测试类型分组结果
        results_by_type = {}
        for result in self.test_results:
            test_type = result.get('test_type', 'unknown')
            if test_type not in results_by_type:
                results_by_type[test_type] = []
            results_by_type[test_type].append(result)
        
        # 生成各类型测试的报告
        for test_type, results in results_by_type.items():
            print(f"--- {test_type.upper()} RESULTS ---")
            
            if test_type == 'database_stress':
                self.report_database_results(results)
            elif test_type == 'memory_stress':
                self.report_memory_results(results)
            elif test_type == 'ui_stress':
                self.report_ui_results(results)
            elif test_type == 'communication_stress':
                self.report_communication_results(results)
            elif test_type == 'system_monitor':
                self.report_system_results(results)
            
            print()
        
        # 保存详细报告到文件
        self.save_detailed_report()
    
    def report_database_results(self, results):
        """报告数据库测试结果"""
        total_ops = sum(r['operations'] for r in results)
        total_errors = sum(r['errors'] for r in results)
        avg_response_time = sum(r['avg_response_time'] for r in results) / len(results)
        
        print(f"Total Operations: {total_ops}")
        print(f"Total Errors: {total_errors}")
        print(f"Error Rate: {(total_errors/max(total_ops,1)*100):.2f}%")
        print(f"Average Response Time: {avg_response_time*1000:.2f}ms")
        print(f"Operations per Second: {total_ops/self.config['duration_seconds']:.2f}")
    
    def report_memory_results(self, results):
        """报告内存测试结果"""
        total_allocations = sum(r['allocations'] for r in results)
        peak_memory = max(r['peak_memory_mb'] for r in results)
        
        print(f"Total Memory Allocations: {total_allocations}")
        print(f"Peak Memory Usage: {peak_memory:.2f} MB")
        print(f"Allocations per Second: {total_allocations/self.config['duration_seconds']:.2f}")
    
    def report_ui_results(self, results):
        """报告UI测试结果"""
        total_ops = sum(r['ui_operations'] for r in results)
        avg_response_time = sum(r['avg_response_time'] for r in results) / len(results)
        
        print(f"Total UI Operations: {total_ops}")
        print(f"Average Response Time: {avg_response_time*1000:.2f}ms")
        print(f"Operations per Second: {total_ops/self.config['duration_seconds']:.2f}")
        print(f"Effective FPS: {total_ops/self.config['duration_seconds']:.2f}")
    
    def report_communication_results(self, results):
        """报告通信测试结果"""
        total_messages = sum(r['messages_sent'] for r in results)
        total_bytes = sum(r['bytes_transferred'] for r in results)
        avg_latency = sum(r['avg_latency'] for r in results) / len(results)
        
        print(f"Total Messages: {total_messages}")
        print(f"Total Bytes Transferred: {total_bytes/1024/1024:.2f} MB")
        print(f"Average Latency: {avg_latency*1000:.2f}ms")
        print(f"Messages per Second: {total_messages/self.config['duration_seconds']:.2f}")
        print(f"Throughput: {total_bytes/1024/1024/self.config['duration_seconds']:.2f} MB/s")
    
    def report_system_results(self, results):
        """报告系统监控结果"""
        if not results:
            return
        
        result = results[0]  # 只有一个系统监控结果
        
        if result['cpu_samples']:
            avg_cpu = sum(s['cpu_percent'] for s in result['cpu_samples']) / len(result['cpu_samples'])
            max_cpu = max(s['cpu_percent'] for s in result['cpu_samples'])
            print(f"Average CPU Usage: {avg_cpu:.2f}%")
            print(f"Peak CPU Usage: {max_cpu:.2f}%")
        
        if result['memory_samples']:
            avg_memory = sum(s['memory_percent'] for s in result['memory_samples']) / len(result['memory_samples'])
            max_memory = max(s['memory_percent'] for s in result['memory_samples'])
            print(f"Average Memory Usage: {avg_memory:.2f}%")
            print(f"Peak Memory Usage: {max_memory:.2f}%")
    
    def save_detailed_report(self):
        """保存详细报告到文件"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        report_file = f"stress_test_report_{timestamp}.json"
        
        report_data = {
            'test_config': self.config,
            'start_time': self.start_time.isoformat(),
            'end_time': self.end_time.isoformat(),
            'duration_seconds': (self.end_time - self.start_time).total_seconds(),
            'results': self.test_results
        }
        
        try:
            with open(report_file, 'w', encoding='utf-8') as f:
                json.dump(report_data, f, indent=2, ensure_ascii=False)
            print(f"Detailed report saved to: {report_file}")
        except Exception as e:
            print(f"Failed to save detailed report: {e}")

def main():
    """主函数"""
    if len(sys.argv) > 1:
        config_path = sys.argv[1]
    else:
        config_path = "../config/performance_config.json"
    
    runner = StressTestRunner(config_path)
    runner.run_stress_test()

if __name__ == "__main__":
    main()