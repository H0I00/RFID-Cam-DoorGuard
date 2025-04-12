import sys
import socket
import threading
import time
from collections import deque
import subprocess
import os

from PySide6.QtWidgets import QApplication,QWidget
from PySide6.QtGui import QPixmap, QTextCursor
from PySide6.QtCore import Qt, Signal, QObject

from ui_form import Ui_tcpui

# ------------------- 信号类，用于线程与UI通信 -------------------
class WorkerSignals(QObject):
    log_signal = Signal(str)       # 用于在UI上追加日志文本
    image_signal = Signal(bytes)   # 用于在UI上显示图像
    fps_signal = Signal(float)     # 用于在UI上显示 FPS
    update_client_port_signal = Signal(str)  # 用于更新客户端端口号
    control_signal = Signal(str)  # 新增控制信号
    update_countdown_signal = Signal(int)  # 新增倒计时信号
    enable_buttons_signal = Signal(bool)  # 新增按钮启用/禁用信号

# ------------------- 网络通信线程 -------------------
class NetworkThread(threading.Thread):
    """
    根据模式（服务器/客户端）启动 TCP 连接，接收数据并解析。
    接收到的文本通过 log_signal 发到 UI，接收到的图像通过 image_signal 发到 UI。
    """
    def __init__(self, 
                 is_server: bool, 
                 server_ip: str, 
                 server_port: int,
                 client_ip: str,
                 client_port: int,
                 signals: WorkerSignals):
        super().__init__()
        self.is_server = is_server
        self.server_ip = server_ip
        self.server_port = server_port
        self.client_ip = client_ip
        self.client_port = client_port
        self.signals = signals
        self.stop_event = threading.Event()  # 使用Event来控制线程停止
        self.sock = None
        self.frame_times = deque(maxlen=10)
        
        self.current_conn = None  # 保存当前有效的连接对象
        
        # 将倒计时相关变量提升为类属性
        self.countdown_active = False  
        self.countdown_thread = None
        self.door_opened = False
        self.last_card_id = ""
        self.is_button_event = False

    # 添加倒计时停止方法
    def stop_countdown(self):
        """停止当前正在进行的倒计时"""
        if self.countdown_active:
            self.countdown_active = False
            if self.countdown_thread and self.countdown_thread.is_alive():
                # 不需要join，只需设置标志位，线程会自行结束
                pass
            self.signals.update_countdown_signal.emit(-1)  # 清除倒计时显示
            self.signals.enable_buttons_signal.emit(False)  # 禁用按钮

    def run(self):
        if self.is_server:
            self.start_server()
        else:
            self.start_client()

    def start_server(self):
        """
        服务器模式：监听 server_ip:server_port，等待客户端连接
        """
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind((self.server_ip, self.server_port))
            self.sock.listen(1)
            self.signals.log_signal.emit(f"服务器模式已启动：{self.server_ip}:{self.server_port}\n等待客户端连接...\n")

            self.sock.settimeout(5.0)  # 方便检查stop_flag
            conn = None

            while not self.stop_event.is_set():
                try:
                    conn, addr = self.sock.accept()
                except socket.timeout:
                    continue  # 继续等待
                if conn:
                    self.signals.log_signal.emit(f"客户端已连接：{addr}\n")
                    self.handle_connection(conn)
                    conn.close()
                    self.signals.log_signal.emit("客户端已断开连接。\n")

        except Exception as e:
            self.signals.log_signal.emit(f"[服务器异常] {e}\n")
        finally:
            self.close_socket()  # 关闭socket
            self.signals.log_signal.emit("服务器已停止。\n")

    def start_client(self):
        """
        客户端模式：主动连接 server_ip:server_port
        """
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5)
            self.sock.connect((self.server_ip, self.server_port))
            self.signals.log_signal.emit(f"客户端已连接到：{self.server_ip}:{self.server_port}\n")

            # 开始接收数据
            self.handle_connection(self.sock)

        except Exception as e:
            self.signals.log_signal.emit(f"[客户端异常] {e}\n")
        finally:
            self.close_socket()  # 关闭socket
            self.signals.log_signal.emit("客户端连接已关闭。\n")

    def handle_connection(self, conn: socket.socket):
        """
        实时接收并处理图片流
        """
        self.current_conn = conn  # 保存当前连接
        conn.settimeout(1.0)
        buffer = b""  # 缓存接收到的数据
        
        client_port = conn.getpeername()[1]
        self.signals.log_signal.emit(f"客户端端口：{client_port}\n")
        self.signals.update_client_port_signal.emit(str(client_port))
        
        # 确保card.txt文件存在
        if not os.path.exists("card.txt"):
            with open("card.txt", "w", encoding="utf-8") as f:
                f.write("# 有效卡片ID列表，每行一个\n")
                f.write("12345678\n")  # 示例卡号
                f.write("ABCDEF12\n")  # 示例卡号
        
        # 倒计时线程函数 - 使用类实例属性
        def countdown_timer():
            countdown_start_time = time.time()
            self.signals.enable_buttons_signal.emit(True)  # 启用按钮
            
            for i in range(30, -1, -1):
                if not self.countdown_active:
                    self.signals.update_countdown_signal.emit(-1)  # 清除倒计时显示
                    self.signals.enable_buttons_signal.emit(False)  # 禁用按钮
                    return  # 提前退出
                self.signals.update_countdown_signal.emit(i)
                time.sleep(1)
            
            # 倒计时结束
            if self.countdown_active:
                self.countdown_active = False
                self.signals.update_countdown_signal.emit(-1)  # 清除倒计时显示
                self.signals.enable_buttons_signal.emit(False)  # 禁用按钮
                
                # 记录最终状态
                timestamp = time.strftime("%Y年%m月%d日%H:%M:%S")
                door_status = "已开门" if self.door_opened else "未开门"
                self.signals.log_signal.emit(f"倒计时结束 - {door_status}\n")
                
                # 记录日志
                log_entry = f"{timestamp}--图片--{door_status}\n"
                with open("access_log.txt", "a", encoding="utf-8") as f:
                    f.write(log_entry)
                
                # 清理标志
                self.door_opened = False
                self.is_button_event = False

        while not self.stop_event.is_set():
            try:
                data = conn.recv(1460)  
                if not data:
                    continue
                    
                buffer += data
                
                # 提前检查按钮事件标记，确保在处理图片前设置标志
                if b"BUTTON_EVENT" in buffer:
                    self.signals.log_signal.emit("检测到按钮事件，等待图片数据...\n")
                    self.is_button_event = True
                    buffer = buffer.replace(b"BUTTON_EVENT\r\n", b"")  # 移除事件标记
                
                # 处理卡号数据
                if b"CARD:" in buffer:
                    card_idx = buffer.find(b"CARD:")
                    if card_idx != -1:
                        end_idx = buffer.find(b"\r\n", card_idx)
                        if end_idx != -1:
                            card_data = buffer[card_idx:end_idx]
                            buffer = buffer[end_idx+2:]  # 移除处理过的数据
                            
                            try:
                                card_str = card_data.decode().strip()
                                card_id = card_str[5:]  # 提取卡号
                                
                                # 防止重复处理
                                if card_id != self.last_card_id:
                                    self.last_card_id = card_id
                                    self.signals.log_signal.emit(f"收到卡号: {card_id}\n")
                                    
                                    # 检查卡号有效性
                                    valid = False
                                    try:
                                        with open("card.txt", "r", encoding="utf-8") as f:
                                            valid = any(line.strip().upper() == card_id.upper() for line in f 
                                                      if not line.startswith('#'))
                                    except FileNotFoundError:
                                        valid = False
                                    
                                    # 记录日志
                                    timestamp = time.strftime("%Y年%m月%d日%H:%M:%S")
                                    status = "卡号有效" if valid else "卡号无效"
                                    log_entry = f"{timestamp}--{card_id}--{status}\n"
                                    
                                    self.signals.log_signal.emit(log_entry)
                                    with open("access_log.txt", "a", encoding="utf-8") as f:
                                        f.write(log_entry)
                                    
                                    # 如果卡有效，直接发送开门命令，不需要倒计时
                                    if valid:
                                        time.sleep(1)  # 等待1秒，确保数据完整
                                        self.signals.log_signal.emit("有效卡片，授权开门\n")
                                        conn.sendall(b"OPEN_DOOR\r\n")
                            except UnicodeDecodeError:
                                self.signals.log_signal.emit("[错误] 卡号解码失败\n")

                # 处理图片数据
                start_idx = buffer.find(b"\xff\xd8")
                end_idx = buffer.find(b"\xff\xd9")
                if start_idx != -1 and end_idx != -1 and end_idx > start_idx:
                    self.signals.log_signal.emit("接收到图像数据\n")
                    image_data = buffer[start_idx:end_idx+2]
                    buffer = buffer[end_idx+2:]
                    
                    # 保存图片
                    timestamp = time.strftime("%Y%m%d_%H%M%S")
                    filename = f"capture_{timestamp}.jpg"
                    
                    with open(filename, "wb") as f:
                        f.write(image_data)
                    
                    # 更新UI显示
                    self.signals.image_signal.emit(image_data)
                    
                    # 计算FPS
                    self.frame_times.append(time.time())
                    if len(self.frame_times) > 1:
                        fps = len(self.frame_times) / (self.frame_times[-1] - self.frame_times[0])
                    else:
                        fps = 0
                    
                    self.signals.fps_signal.emit(fps)
                    self.signals.log_signal.emit(f"图片已保存：{filename}\n")
                    
                    # 明确判断是否为按钮事件触发的图片
                    if self.is_button_event:
                        self.signals.log_signal.emit("按钮触发图片，启动30秒倒计时\n")
                        self.countdown_active = True
                        
                        # 停止已存在的倒计时线程
                        if self.countdown_thread and self.countdown_thread.is_alive():
                            self.countdown_active = False  # 停止旧线程
                            self.countdown_thread.join(1.0)
                        
                        # 创建新的倒计时线程
                        self.countdown_active = True
                        self.countdown_thread = threading.Thread(target=countdown_timer)
                        self.countdown_thread.daemon = True
                        self.countdown_thread.start()
                        
                        self.is_button_event = False  # 重置标志
                    else:
                        self.signals.log_signal.emit("普通图片，不启动倒计时\n")

            except socket.timeout:
                continue  # 超时不视为错误
            except (ConnectionResetError, socket.error, OSError) as e:
                self.signals.log_signal.emit(f"[网络异常] {e}\n")
                break  # 严重错误时断开

    def send_data(self, data: str):
        """发送文本数据到已连接的socket"""
        if self.current_conn is not None and not self.stop_event.is_set():
            try:
                # 检查连接是否仍然有效
                self.current_conn.getpeername()  # 尝试获取对端地址
                self.current_conn.sendall(data.encode())
                return True
            except (OSError, ConnectionResetError, BrokenPipeError) as e:
                self.signals.log_signal.emit(f"[连接异常] {e}\n")
                self.current_conn.close()
                self.current_conn = None
                return False
            except Exception as e:
                self.signals.log_signal.emit(f"[发送失败] 未知错误: {e}\n")
                return False
        else:
            self.signals.log_signal.emit("[错误] 未建立有效连接\n")
            return False

    def close_socket(self):
        """关闭socket连接"""
        if self.current_conn:
            try:
                self.current_conn.shutdown(socket.SHUT_RDWR)
                self.current_conn.close()
            except OSError as e:
                self.signals.log_signal.emit(f"[关闭连接异常] {e}\n")
            finally:
                self.current_conn = None
        
        if self.sock:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
                self.sock.close()
            except OSError as e:
                if e.winerror != 10057:  # 过滤"套接字未连接"的警告
                    self.signals.log_signal.emit(f"[关闭监听异常] {e}\n")
            finally:
                self.sock = None

    def stop(self):
        self.stop_event.set()  # 停止线程
        self.close_socket()  # 确保socket关闭

# ------------------- 主窗口 -------------------
class tcpui(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.ui = Ui_tcpui()
        self.ui.setupUi(self)
        self.setWindowTitle("NetworkDisplay-(xjc)")
        self.auto_fill_ip()  # 自动填充 IP
        self.setup_log_file()
        
        # 倒计时状态
        self.countdown_active = False
        self.countdown_line = None
        
        # 线程相关
        self.network_thread = None
        self.signals = WorkerSignals()

        self.ui.start_button.setEnabled(True)
        self.ui.stop_button.setEnabled(False)
        
        # 默认禁用开门和关门按钮
        self.ui.open_button.setEnabled(False)
        self.ui.close_button.setEnabled(False)

        # 信号连接       
        self.ui.start_button.clicked.connect(self.start_network)
        self.ui.stop_button.clicked.connect(self.stop_network)
        self.ui.open_button.clicked.connect(self.handle_open_door)
        self.ui.close_button.clicked.connect(self.handle_close_door)

        self.signals.log_signal.connect(self.append_log)
        self.signals.image_signal.connect(self.show_image)
        self.signals.fps_signal.connect(self.update_fps)
        self.signals.update_client_port_signal.connect(self.update_client_port)
        self.signals.update_countdown_signal.connect(self.update_countdown)
        self.signals.enable_buttons_signal.connect(self.enable_door_buttons)

    def update_ui_for_network_status(self, running: bool):
        """更新UI界面状态，并改变输入框的背景颜色"""
        disabled_style = "background-color:rgb(220, 220, 220);"  # 灰色背景
        enabled_style = ""  # 恢复默认背景

        # 设置禁用状态
        self.ui.server_ip_edit.setEnabled(not running)
        self.ui.server_port_edit.setEnabled(not running)
        self.ui.client_ip_edit.setEnabled(not running)
        self.ui.client_port_edit.setEnabled(not running)

        # 修改背景色
        self.ui.server_ip_edit.setStyleSheet(disabled_style if running else enabled_style)
        self.ui.server_port_edit.setStyleSheet(disabled_style if running else enabled_style)
        self.ui.client_ip_edit.setStyleSheet(disabled_style if running else enabled_style)
        self.ui.client_port_edit.setStyleSheet(disabled_style if running else enabled_style)

        self.ui.start_button.setEnabled(not running)
        self.ui.stop_button.setEnabled(running)

    def setup_log_file(self):
        log_dir = "logs"
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)
        self.log_path = os.path.join(log_dir, f"access_{time.strftime('%Y%m%d')}.txt")
    
    def start_network(self):
        """
        启动服务器或客户端线程
        """
        # 读取UI参数
        is_server = self.ui.server_mode_check.isChecked()
        server_ip = self.ui.server_ip_edit.text().strip()
        server_port = int(self.ui.server_port_edit.text().strip())
        client_ip = self.ui.client_ip_edit.text().strip()
        client_port = int(self.ui.client_port_edit.text().strip())
        
        # 禁用输入框
        self.update_ui_for_network_status(True)

        self.network_thread = NetworkThread(
            is_server=is_server,
            server_ip=server_ip,
            server_port=server_port,
            client_ip=client_ip,
            client_port=client_port,
            signals=self.signals
        )
        self.network_thread.start()

    def stop_network(self):
        """
        停止服务器或客户端线程
        """
        if self.network_thread:
            self.network_thread.stop()  # 确保 socket 关闭
            if self.network_thread.is_alive():
                self.network_thread.join()  # 等待线程安全退出
            self.network_thread = None

        # 启用输入框
        self.update_ui_for_network_status(False)
        
        # 清空textBrowser内容
        self.ui.textBrowser.clear()
        
        # 清空图片显示
        self.ui.image_label.clear()
        
        # 重置其他显示信息
        self.ui.fps_label.setText("FPS: 0.00")
        self.ui.image_size_label.setText("分辨率: 0 x 0")
        
        # 重置倒计时状态
        self.countdown_line = None
        
        # 添加仅有的提示信息
        self.append_log("网络已停止。\n")
        
    def auto_fill_ip(self):
        """通过 ipconfig 解析 WLAN 的 IPv4 地址"""
        try:
            # 1. 执行 ipconfig 命令并获取输出
            result = subprocess.check_output("ipconfig", encoding="gbk")
            lines = result.split("\n")

            # 2. 解析 WLAN 的 IPv4 地址
            wlan_section = False
            ip = None
            for line in lines:
                if "无线局域网适配器 WLAN" in line:
                    wlan_section = True
                elif wlan_section and "IPv4" in line:
                    ip = line.split(":")[1].strip()
                    break

            # 3. 提取第三位并填充到 UI
            if ip:
                ip_parts = ip.split(".")
                third_part = ip_parts[2]  # 例如 192.168.69.60 -> 69
                self.ui.server_ip_edit.setText(f"192.168.{third_part}.60")
                self.ui.client_ip_edit.setText(f"192.168.{third_part}.179")
                
        except Exception as e:
            self.append_log(f"[警告] 自动获取 IP 失败: {e}\n")
        

    
    def append_log(self, text: str):
        """
        在日志窗口追加文本
        """
        self.ui.textBrowser.moveCursor(QTextCursor.End)
        self.ui.textBrowser.insertPlainText(text)
        
    def handle_open_door(self):
        if self.network_thread and self.network_thread.is_alive():
            if self.network_thread.send_data("OPEN_DOOR\r\n"):
                self.append_log("[操作] 手动发送开门指令成功\n")
                
                # 标记已开门
                self.network_thread.door_opened = True
                
                # 停止倒计时
                self.network_thread.stop_countdown()
                
                # 记录日志
                timestamp = time.strftime("%Y年%m月%d日%H:%M:%S")
                self.append_log(f"{timestamp}--图片--已开门\n")
                
                # 记录日志
                with open("access_log.txt", "a", encoding="utf-8") as f:
                    f.write(f"{timestamp}--图片--已开门\n")
                
                return True
            else:
                self.append_log("[错误] 手动开门失败，请检查连接\n")
                return False
        else:
            self.append_log("[错误] 网络连接不可用\n")
            return False

    def handle_close_door(self):
        self.append_log("[操作] 取消开门\n")
        
        if self.network_thread and self.network_thread.is_alive():
            # 标记未开门
            self.network_thread.door_opened = False
            
            # 停止倒计时
            self.network_thread.stop_countdown()
        
        # 记录日志
        timestamp = time.strftime("%Y年%m月%d日%H:%M:%S")
        self.append_log(f"{timestamp}--图片--未开门\n")
        
        # 记录日志
        with open("access_log.txt", "a", encoding="utf-8") as f:
            f.write(f"{timestamp}--图片--未开门\n")

    def show_image(self, img_data: bytes):
        """
        直接显示接收到的二进制图像数据，并更新 FPS、分辨率，并保存图片
        """
        pixmap = QPixmap()
        if pixmap.loadFromData(img_data):  
            # 更新 QLabel 显示
            scaled = pixmap.scaled(self.ui.image_label.size(), Qt.KeepAspectRatio)
            self.ui.image_label.setPixmap(scaled)

            # 获取图像大小
            img_width = pixmap.width()
            img_height = pixmap.height()

            # 更新图像信息
            self.ui.image_size_label.setText(f"分辨率: {img_width} x {img_height}")


    def update_fps(self, fps: float):
        """
        更新 FPS 信息
        """
        self.ui.fps_label.setText(f"FPS: {fps:.2f}")
        
    def update_client_port(self, port: str):
        """
        更新客户端端口
        """
        self.ui.client_port_edit.setText(port)  # 更新client_port_edit显示的内容

    def update_countdown(self, seconds):
        """更新或清除倒计时显示"""
        # 如果是-1，清除倒计时
        if seconds == -1:
            if self.countdown_line is not None:
                self.ui.textBrowser.moveCursor(QTextCursor.End)
                cursor = self.ui.textBrowser.textCursor()
                cursor.movePosition(QTextCursor.StartOfBlock, QTextCursor.KeepAnchor)
                cursor.removeSelectedText()
                self.countdown_line = None
            return
        
        # 更新或创建倒计时行
        countdown_text = f"[倒计时] 剩余 {seconds} 秒做出决定..."
        
        if self.countdown_line is None:
            # 第一次创建倒计时行
            self.ui.textBrowser.moveCursor(QTextCursor.End)
            self.ui.textBrowser.insertPlainText("\n" + countdown_text)
            self.countdown_line = countdown_text
        else:
            # 更新倒计时行
            self.ui.textBrowser.moveCursor(QTextCursor.End)
            cursor = self.ui.textBrowser.textCursor()
            cursor.movePosition(QTextCursor.StartOfBlock, QTextCursor.KeepAnchor)
            cursor.removeSelectedText()
            cursor.insertText(countdown_text)
    
    def enable_door_buttons(self, enabled):
        """启用或禁用开门和关门按钮"""
        self.ui.open_button.setEnabled(enabled)
        self.ui.close_button.setEnabled(enabled)

    def closeEvent(self, event):
        """
        关闭窗口时，确保线程退出
        """
        self.stop_network()
        super().closeEvent(event)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    widget = tcpui()
    widget.show()
    sys.exit(app.exec())
